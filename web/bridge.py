#!/usr/bin/env python3
"""Mini ATS web bridge.

Serves the static dashboard, proxies browser requests to the C++ TCP gateway,
and streams UDP market-data events to the browser with Server-Sent Events.
"""

from __future__ import annotations

import argparse
import collections
import contextlib
import json
import mimetypes
import os
from pathlib import Path
import queue
import re
import socket
import subprocess
import sys
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import unquote, urlparse


WEB_ROOT = Path(__file__).resolve().parent
REPO_ROOT = WEB_ROOT.parent
DEFAULT_ENGINE = REPO_ROOT / "build" / "mini_ats"


class BridgeError(Exception):
    def __init__(self, message: str, status: HTTPStatus = HTTPStatus.BAD_REQUEST) -> None:
        super().__init__(message)
        self.status = status


class EventHub:
    def __init__(self) -> None:
        self._subscribers: set[queue.Queue[dict[str, Any]]] = set()
        self._lock = threading.Lock()

    def subscribe(self) -> queue.Queue[dict[str, Any]]:
        subscriber: queue.Queue[dict[str, Any]] = queue.Queue(maxsize=256)
        with self._lock:
            self._subscribers.add(subscriber)
        return subscriber

    def unsubscribe(self, subscriber: queue.Queue[dict[str, Any]]) -> None:
        with self._lock:
            self._subscribers.discard(subscriber)

    def publish(self, payload: dict[str, Any]) -> None:
        with self._lock:
            subscribers = list(self._subscribers)

        for subscriber in subscribers:
            try:
                subscriber.put_nowait(payload)
            except queue.Full:
                with contextlib.suppress(queue.Empty):
                    subscriber.get_nowait()
                with contextlib.suppress(queue.Full):
                    subscriber.put_nowait(payload)


def normalize_scalar(value: str) -> Any:
    if value == "NONE":
        return None
    if re.fullmatch(r"-?\d+", value):
        return int(value)
    return value


def parse_key_value_line(line: str) -> dict[str, Any]:
    tokens = line.strip().split()
    if not tokens:
        return {"type": "EMPTY", "fields": {}, "raw": line}

    fields: dict[str, Any] = {}
    for token in tokens[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = normalize_scalar(value)

    return {
        "type": tokens[0],
        "fields": fields,
        "raw": line.strip(),
    }


def collect_indexed_fields(fields: dict[str, Any], prefix: str, count_key: str) -> list[dict[str, Any]]:
    count = int(fields.get(count_key) or 0)
    rows: list[dict[str, Any]] = []
    for index in range(count):
        stem = f"{prefix}{index}_"
        row: dict[str, Any] = {}
        for key, value in fields.items():
            if key.startswith(stem):
                row[key[len(stem) :]] = value
        rows.append(row)
    return rows


def parse_gateway_response(line: str) -> dict[str, Any]:
    parsed = parse_key_value_line(line)
    fields = parsed["fields"]
    return {
        "status": parsed["type"],
        "reason": fields.get("reason"),
        "sequence": fields.get("seq"),
        "commandType": fields.get("command"),
        "detail": fields.get("detail"),
        "trades": collect_indexed_fields(fields, "trade", "trades"),
        "reports": collect_indexed_fields(fields, "report", "reports"),
        "raw": parsed["raw"],
    }


def level_from_fields(fields: dict[str, Any], prefix: str) -> dict[str, Any] | None:
    price = fields.get(f"{prefix}_price")
    quantity = fields.get(f"{prefix}_quantity")
    if price is None:
        return None
    return {"price": price, "quantity": quantity or 0}


def parse_market_data(line: str) -> dict[str, Any]:
    parsed = parse_key_value_line(line)
    fields = parsed["fields"]
    event_type = parsed["type"]
    event: dict[str, Any] = {
        "type": event_type,
        "sequence": fields.get("seq"),
        "raw": parsed["raw"],
    }

    if event_type == "TRADE":
        event.update(
            {
                "tradeId": fields.get("trade_id"),
                "instrumentId": fields.get("instrument_id"),
                "restingOrderId": fields.get("resting_order_id"),
                "incomingOrderId": fields.get("incoming_order_id"),
                "aggressorSide": fields.get("aggressor_side"),
                "price": fields.get("price"),
                "quantity": fields.get("quantity"),
            }
        )
    elif event_type == "BOOK_UPDATE":
        event.update(
            {
                "instrumentId": fields.get("instrument_id"),
                "bestBid": level_from_fields(fields, "best_bid"),
                "bestAsk": level_from_fields(fields, "best_ask"),
                "bids": collect_book_levels(fields, "bid", "bids"),
                "asks": collect_book_levels(fields, "ask", "asks"),
            }
        )

    return event


def collect_book_levels(fields: dict[str, Any], prefix: str, count_key: str) -> list[dict[str, Any]]:
    count = int(fields.get(count_key) or 0)
    levels: list[dict[str, Any]] = []
    for index in range(count):
        price = fields.get(f"{prefix}{index}_price")
        quantity = fields.get(f"{prefix}{index}_quantity")
        if price is not None:
            levels.append({"price": price, "quantity": quantity or 0})
    return levels


class MiniAtsBridge:
    def __init__(
        self,
        *,
        gateway_host: str,
        gateway_port: int,
        market_data_host: str,
        market_data_port: int,
        initial_sequence: int,
        initial_order_id: int,
        default_instrument_id: int,
        default_reference_version: int,
        engine_path: Path,
        start_engine: bool,
        record_log: str | None,
        engine_start_timeout: float,
    ) -> None:
        self.gateway_host = gateway_host
        self.gateway_port = gateway_port
        self.market_data_host = market_data_host
        self.market_data_port = market_data_port
        self.default_instrument_id = default_instrument_id
        self.default_reference_version = default_reference_version
        self.engine_path = engine_path
        self.start_engine = start_engine
        self.record_log = record_log
        self.engine_start_timeout = engine_start_timeout

        self.hub = EventHub()
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._next_sequence = initial_sequence
        self._next_order_id = initial_order_id
        self._book = {
            "instrumentId": default_instrument_id,
            "bestBid": None,
            "bestAsk": None,
            "bids": [],
            "asks": [],
        }
        self._trades: collections.deque[dict[str, Any]] = collections.deque(maxlen=200)
        self._responses: collections.deque[dict[str, Any]] = collections.deque(maxlen=200)
        self._commands: collections.deque[dict[str, Any]] = collections.deque(maxlen=200)
        self._engine_logs: collections.deque[str] = collections.deque(maxlen=100)
        self._open_orders: dict[int, dict[str, Any]] = {}
        self._stats = {
            "commands": 0,
            "accepted": 0,
            "rejected": 0,
            "trades": 0,
            "tradedQuantity": 0,
            "tradedNotional": 0,
        }
        self._engine_process: subprocess.Popen[str] | None = None
        self._udp_thread: threading.Thread | None = None

    def start(self) -> None:
        self._start_udp_listener()
        if self.start_engine:
            self._start_engine_process()

    def close(self) -> None:
        self._stop.set()
        process = self._engine_process
        if process is not None and process.poll() is None:
            process.terminate()
            with contextlib.suppress(subprocess.TimeoutExpired):
                process.wait(timeout=3)
            if process.poll() is None:
                process.kill()

    def health(self) -> dict[str, Any]:
        process = self._engine_process
        process_running = process is not None and process.poll() is None
        return {
            "bridge": "OK",
            "engineConnected": self._can_connect_gateway(),
            "engineManaged": self.start_engine,
            "engineRunning": process_running if process is not None else None,
            "gatewayHost": self.gateway_host,
            "gatewayPort": self.gateway_port,
            "marketDataHost": self.market_data_host,
            "marketDataPort": self.market_data_port,
        }

    def snapshot(self) -> dict[str, Any]:
        with self._lock:
            return {
                "health": self.health(),
                "defaults": {
                    "instrumentId": self.default_instrument_id,
                    "referenceVersion": self.default_reference_version,
                },
                "nextSequence": self._next_sequence,
                "nextOrderId": self._next_order_id,
                "book": json.loads(json.dumps(self._book)),
                "trades": list(self._trades),
                "responses": list(self._responses),
                "commands": list(self._commands),
                "openOrders": list(self._open_orders.values()),
                "stats": dict(self._stats),
                "engineLogs": list(self._engine_logs),
            }

    def submit_order(self, payload: dict[str, Any]) -> dict[str, Any]:
        command, metadata = self._build_submit_command(payload)
        return self._send_and_record(command, metadata)

    def cancel_order(self, payload: dict[str, Any]) -> dict[str, Any]:
        command, metadata = self._build_cancel_command(payload)
        return self._send_and_record(command, metadata)

    def send_raw_command(self, payload: dict[str, Any]) -> dict[str, Any]:
        command = str(payload.get("command", "")).strip()
        if not command:
            raise BridgeError("command is required")
        return self._send_and_record(command, {"kind": "raw"})

    def _build_submit_command(self, payload: dict[str, Any]) -> tuple[str, dict[str, Any]]:
        side = self._enum(payload, "side", {"BUY", "SELL"}, "BUY")
        order_type = self._enum(payload, "orderType", {"LIMIT", "MARKET"}, "LIMIT")
        tif = self._enum(payload, "timeInForce", {"DAY", "IOC", "FOK"}, "DAY")
        quantity = self._positive_int(payload, "quantity")
        instrument_id = self._positive_int(payload, "instrumentId", self.default_instrument_id)
        reference_version = self._positive_int(
            payload, "referenceVersion", self.default_reference_version
        )
        price = self._optional_int(payload, "price")
        if order_type == "LIMIT" and (price is None or price <= 0):
            raise BridgeError("positive price is required for LIMIT orders")

        with self._lock:
            sequence = self._claim_sequence_locked(payload.get("sequence"))
            order_id = self._claim_order_id_locked(payload.get("orderId"))

        fields = [
            "SUBMIT",
            f"seq={sequence}",
            f"ref={reference_version}",
            f"order_id={order_id}",
            f"instrument_id={instrument_id}",
            f"side={side}",
            f"type={order_type}",
            f"tif={tif}",
            f"quantity={quantity}",
        ]
        if price is not None:
            fields.append(f"price={price}")

        return " ".join(fields), {
            "kind": "submit",
            "orderId": order_id,
            "instrumentId": instrument_id,
            "side": side,
            "orderType": order_type,
            "timeInForce": tif,
            "price": price,
            "quantity": quantity,
            "sequence": sequence,
        }

    def _build_cancel_command(self, payload: dict[str, Any]) -> tuple[str, dict[str, Any]]:
        order_id = self._positive_int(payload, "orderId")
        instrument_id = self._positive_int(payload, "instrumentId", self.default_instrument_id)
        reference_version = self._positive_int(
            payload, "referenceVersion", self.default_reference_version
        )

        with self._lock:
            sequence = self._claim_sequence_locked(payload.get("sequence"))

        command = (
            f"CANCEL seq={sequence} ref={reference_version} "
            f"order_id={order_id} instrument_id={instrument_id}"
        )
        return command, {
            "kind": "cancel",
            "orderId": order_id,
            "instrumentId": instrument_id,
            "sequence": sequence,
        }

    def _send_and_record(self, command: str, metadata: dict[str, Any]) -> dict[str, Any]:
        started_at = time.perf_counter()
        try:
            response_text = send_tcp_command(self.gateway_host, self.gateway_port, command)
        except OSError as exc:
            raise BridgeError(
                f"cannot reach Mini ATS TCP gateway at {self.gateway_host}:{self.gateway_port}: {exc}",
                HTTPStatus.BAD_GATEWAY,
            ) from exc

        elapsed_ms = round((time.perf_counter() - started_at) * 1000, 3)
        response = parse_gateway_response(response_text)
        response["latencyMs"] = elapsed_ms

        with self._lock:
            self._record_command_locked(command, metadata, response)

        payload = {
            "kind": "gateway_response",
            "command": command,
            "response": response,
            "snapshot": self.snapshot(),
        }
        self.hub.publish(payload)
        return payload

    def _record_command_locked(
        self,
        command: str,
        metadata: dict[str, Any],
        response: dict[str, Any],
    ) -> None:
        status = response.get("status")
        self._stats["commands"] += 1
        if status == "ACCEPTED":
            self._stats["accepted"] += 1
        else:
            self._stats["rejected"] += 1

        self._commands.appendleft(
            {
                "command": command,
                "metadata": metadata,
                "status": status,
                "sequence": response.get("sequence"),
                "at": round(time.time(), 3),
            }
        )
        self._responses.appendleft(response)
        self._advance_counters_from_response_locked(response)
        self._update_open_orders_locked(metadata, response)
        self._record_response_trades_locked(response)

    def _record_response_trades_locked(self, response: dict[str, Any]) -> None:
        for trade in response.get("trades", []):
            normalized = {
                "type": "TRADE",
                "sequence": trade.get("sequence"),
                "tradeId": trade.get("id"),
                "instrumentId": trade.get("instrument_id"),
                "restingOrderId": trade.get("resting_order_id"),
                "incomingOrderId": trade.get("incoming_order_id"),
                "aggressorSide": trade.get("aggressor_side"),
                "price": trade.get("price"),
                "quantity": trade.get("quantity"),
                "raw": response.get("raw"),
                "source": "gateway",
            }
            self._add_trade_locked(normalized)

    def _record_market_data(self, event: dict[str, Any]) -> None:
        with self._lock:
            if event.get("type") == "BOOK_UPDATE":
                self._book = {
                    "instrumentId": event.get("instrumentId"),
                    "bestBid": event.get("bestBid"),
                    "bestAsk": event.get("bestAsk"),
                    "bids": event.get("bids", []),
                    "asks": event.get("asks", []),
                }
            elif event.get("type") == "TRADE":
                event = dict(event)
                event["source"] = "market_data"
                self._add_trade_locked(event)

        self.hub.publish(
            {
                "kind": "market_data",
                "event": event,
                "snapshot": self.snapshot(),
            }
        )

    def _add_trade_locked(self, trade: dict[str, Any]) -> None:
        trade_key = (trade.get("tradeId"), trade.get("sequence"))
        for existing in self._trades:
            existing_key = (
                existing.get("tradeId"),
                existing.get("sequence"),
            )
            if existing_key == trade_key:
                return

        self._trades.appendleft(trade)
        quantity = int(trade.get("quantity") or 0)
        price = int(trade.get("price") or 0)
        self._stats["trades"] += 1
        self._stats["tradedQuantity"] += quantity
        self._stats["tradedNotional"] += price * quantity

    def _update_open_orders_locked(
        self,
        metadata: dict[str, Any],
        response: dict[str, Any],
    ) -> None:
        if metadata.get("kind") == "submit" and response.get("status") == "ACCEPTED":
            order_id = int(metadata["orderId"])
            self._open_orders[order_id] = {
                "orderId": order_id,
                "instrumentId": metadata.get("instrumentId"),
                "side": metadata.get("side"),
                "orderType": metadata.get("orderType"),
                "timeInForce": metadata.get("timeInForce"),
                "price": metadata.get("price"),
                "originalQuantity": metadata.get("quantity"),
                "remainingQuantity": metadata.get("quantity"),
                "sequence": metadata.get("sequence"),
            }

        for report in response.get("reports", []):
            order_id = report.get("order_id")
            if order_id is None:
                continue
            order_id = int(order_id)
            status = report.get("status")
            remaining = report.get("remaining_quantity")
            if remaining is not None and order_id in self._open_orders:
                self._open_orders[order_id]["remainingQuantity"] = remaining
            if status in {"FILLED", "CANCELED", "REJECTED"} or remaining == 0:
                self._open_orders.pop(order_id, None)

    def _advance_counters_from_response_locked(self, response: dict[str, Any]) -> None:
        sequence = response.get("sequence")
        if isinstance(sequence, int) and sequence >= self._next_sequence:
            self._next_sequence = sequence + 1
        for report in response.get("reports", []):
            order_id = report.get("order_id")
            if isinstance(order_id, int) and order_id >= self._next_order_id:
                self._next_order_id = order_id + 1

    def _claim_sequence_locked(self, requested: Any) -> int:
        sequence = self._positive_int_value(requested, "sequence") if requested else self._next_sequence
        if sequence >= self._next_sequence:
            self._next_sequence = sequence + 1
        return sequence

    def _claim_order_id_locked(self, requested: Any) -> int:
        order_id = self._positive_int_value(requested, "orderId") if requested else self._next_order_id
        if order_id >= self._next_order_id:
            self._next_order_id = order_id + 1
        return order_id

    def _start_udp_listener(self) -> None:
        def run() -> None:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            except OSError as exc:
                self._log_engine(f"UDP listener failed: {exc}")
                return

            try:
                try:
                    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                    sock.bind((self.market_data_host, self.market_data_port))
                except OSError as exc:
                    self._log_engine(f"UDP listener failed: {exc}")
                    return

                sock.settimeout(0.5)
                self._log_engine(
                    f"Bridge UDP market data listening on {self.market_data_host}:{self.market_data_port}"
                )
                while not self._stop.is_set():
                    try:
                        data, _ = sock.recvfrom(65535)
                    except socket.timeout:
                        continue
                    except OSError as exc:
                        self._log_engine(f"UDP listener stopped: {exc}")
                        break
                    line = data.decode("utf-8", errors="replace").strip()
                    if line:
                        self._record_market_data(parse_market_data(line))
            finally:
                sock.close()

        self._udp_thread = threading.Thread(target=run, name="market-data-listener", daemon=True)
        self._udp_thread.start()

    def _start_engine_process(self) -> None:
        if not self.engine_path.exists():
            self._log_engine(f"Engine binary not found: {self.engine_path}")
            return

        command = [
            str(self.engine_path),
            "--tcp",
            "--port",
            str(self.gateway_port),
            "--market-data",
            self.market_data_host,
            str(self.market_data_port),
            "--stats",
        ]
        if self.record_log:
            command.extend(["--record-log", self.record_log])

        self._engine_process = subprocess.Popen(
            command,
            cwd=str(REPO_ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._log_engine("Started engine: " + " ".join(command))
        self._start_log_reader(self._engine_process.stdout, "engine")
        self._start_log_reader(self._engine_process.stderr, "engine")

        deadline = time.time() + self.engine_start_timeout
        while time.time() < deadline:
            if self._engine_process.poll() is not None:
                self._log_engine(f"Engine exited with code {self._engine_process.returncode}")
                return
            if self._can_connect_gateway():
                self._log_engine(
                    f"Engine TCP gateway reachable at {self.gateway_host}:{self.gateway_port}"
                )
                return
            time.sleep(0.1)

        self._log_engine("Engine start timeout; bridge will keep serving the dashboard")

    def _start_log_reader(self, stream: Any, source: str) -> None:
        if stream is None:
            return

        def run() -> None:
            for line in stream:
                self._log_engine(f"{source}: {line.rstrip()}")

        threading.Thread(target=run, name=f"{source}-log-reader", daemon=True).start()

    def _log_engine(self, line: str) -> None:
        with self._lock:
            self._engine_logs.appendleft(line)
        self.hub.publish({"kind": "engine_log", "line": line, "snapshot": self.snapshot()})

    def _can_connect_gateway(self) -> bool:
        try:
            with socket.create_connection(
                (self.gateway_host, self.gateway_port), timeout=0.25
            ):
                return True
        except OSError:
            return False

    @staticmethod
    def _enum(
        payload: dict[str, Any],
        key: str,
        allowed: set[str],
        default: str,
    ) -> str:
        value = str(payload.get(key) or default).upper()
        if value not in allowed:
            raise BridgeError(f"{key} must be one of {', '.join(sorted(allowed))}")
        return value

    def _positive_int(
        self,
        payload: dict[str, Any],
        key: str,
        default: int | None = None,
    ) -> int:
        value = payload.get(key, default)
        return self._positive_int_value(value, key)

    @staticmethod
    def _positive_int_value(value: Any, key: str) -> int:
        try:
            parsed = int(value)
        except (TypeError, ValueError) as exc:
            raise BridgeError(f"{key} must be a positive integer") from exc
        if parsed <= 0:
            raise BridgeError(f"{key} must be a positive integer")
        return parsed

    @staticmethod
    def _optional_int(payload: dict[str, Any], key: str) -> int | None:
        value = payload.get(key)
        if value in (None, ""):
            return None
        try:
            return int(value)
        except (TypeError, ValueError) as exc:
            raise BridgeError(f"{key} must be an integer") from exc


def send_tcp_command(host: str, port: int, command: str) -> str:
    with socket.create_connection((host, port), timeout=2.0) as sock:
        sock.settimeout(2.0)
        sock.sendall(command.encode("utf-8") + b"\n")
        chunks: list[bytes] = []
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
            if b"\n" in chunk:
                break
    response = b"".join(chunks).decode("utf-8", errors="replace").strip()
    if not response:
        raise OSError("empty response from gateway")
    return response


class MiniAtsRequestHandler(BaseHTTPRequestHandler):
    bridge: MiniAtsBridge
    static_root: Path

    server_version = "MiniAtsWebBridge/1.0"

    def log_message(self, format: str, *args: Any) -> None:
        sys.stderr.write("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), format % args))

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path == "/api/health":
            self._send_json(self.bridge.health())
            return
        if path == "/api/snapshot":
            self._send_json(self.bridge.snapshot())
            return
        if path == "/events":
            self._send_events()
            return
        self._send_static(path)

    def do_POST(self) -> None:
        try:
            payload = self._read_json()
            path = urlparse(self.path).path
            if path == "/api/order":
                result = self.bridge.submit_order(payload)
            elif path == "/api/cancel":
                result = self.bridge.cancel_order(payload)
            elif path == "/api/raw":
                result = self.bridge.send_raw_command(payload)
            else:
                raise BridgeError("unknown API endpoint", HTTPStatus.NOT_FOUND)
            self._send_json(result)
        except BridgeError as exc:
            self._send_json({"ok": False, "error": str(exc)}, exc.status)
        except json.JSONDecodeError as exc:
            self._send_json({"ok": False, "error": f"invalid JSON: {exc}"}, HTTPStatus.BAD_REQUEST)

    def do_OPTIONS(self) -> None:
        self.send_response(HTTPStatus.NO_CONTENT)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def _read_json(self) -> dict[str, Any]:
        length_text = self.headers.get("Content-Length", "0")
        try:
            length = int(length_text)
        except ValueError as exc:
            raise BridgeError("invalid Content-Length") from exc
        if length > 65536:
            raise BridgeError("request body is too large", HTTPStatus.REQUEST_ENTITY_TOO_LARGE)
        body = self.rfile.read(length)
        if not body:
            return {}
        data = json.loads(body.decode("utf-8"))
        if not isinstance(data, dict):
            raise BridgeError("JSON object is required")
        return data

    def _send_json(self, payload: dict[str, Any], status: HTTPStatus = HTTPStatus.OK) -> None:
        body = json.dumps(payload, ensure_ascii=True).encode("utf-8")
        self.send_response(status)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_events(self) -> None:
        subscriber = self.bridge.hub.subscribe()
        self.send_response(HTTPStatus.OK)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        try:
            self._write_sse({"kind": "snapshot", "snapshot": self.bridge.snapshot()})
            while True:
                try:
                    payload = subscriber.get(timeout=15)
                    self._write_sse(payload)
                except queue.Empty:
                    self.wfile.write(b": heartbeat\n\n")
                    self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.bridge.hub.unsubscribe(subscriber)

    def _write_sse(self, payload: dict[str, Any]) -> None:
        data = json.dumps(payload, ensure_ascii=True)
        self.wfile.write(f"data: {data}\n\n".encode("utf-8"))
        self.wfile.flush()

    def _send_static(self, path: str) -> None:
        if path in {"", "/"}:
            path = "/index.html"
        relative = unquote(path.lstrip("/"))
        target = (self.static_root / relative).resolve()
        try:
            target.relative_to(self.static_root)
        except ValueError:
            self.send_error(HTTPStatus.FORBIDDEN)
            return

        if not target.is_file():
            self.send_error(HTTPStatus.NOT_FOUND)
            return

        content_type, _ = mimetypes.guess_type(str(target))
        body = target.read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type or "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the Mini ATS web dashboard bridge")
    parser.add_argument("--host", default="127.0.0.1", help="HTTP bind address")
    parser.add_argument("--port", type=int, default=8080, help="HTTP dashboard port")
    parser.add_argument("--gateway-host", default="127.0.0.1", help="Mini ATS TCP host")
    parser.add_argument("--gateway-port", type=int, default=9001, help="Mini ATS TCP port")
    parser.add_argument("--market-data-host", default="127.0.0.1", help="UDP market data bind host")
    parser.add_argument("--market-data-port", type=int, default=9100, help="UDP market data bind port")
    parser.add_argument("--instrument-id", type=int, default=1001, help="Default instrument id")
    parser.add_argument("--reference-version", type=int, default=7, help="Default reference version")
    parser.add_argument("--initial-sequence", type=int, default=1, help="First command sequence")
    parser.add_argument("--initial-order-id", type=int, default=10000, help="First UI order id")
    parser.add_argument("--engine", type=Path, default=DEFAULT_ENGINE, help="Path to mini_ats binary")
    parser.add_argument("--start-engine", action="store_true", help="Start the C++ TCP gateway as a child process")
    parser.add_argument("--record-log", default=None, help="Accepted input replay log path for managed engine")
    parser.add_argument("--engine-start-timeout", type=float, default=3.0, help="Seconds to wait for managed engine")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    bridge = MiniAtsBridge(
        gateway_host=args.gateway_host,
        gateway_port=args.gateway_port,
        market_data_host=args.market_data_host,
        market_data_port=args.market_data_port,
        initial_sequence=args.initial_sequence,
        initial_order_id=args.initial_order_id,
        default_instrument_id=args.instrument_id,
        default_reference_version=args.reference_version,
        engine_path=args.engine,
        start_engine=args.start_engine,
        record_log=args.record_log,
        engine_start_timeout=args.engine_start_timeout,
    )
    MiniAtsRequestHandler.bridge = bridge
    MiniAtsRequestHandler.static_root = WEB_ROOT

    bridge.start()
    server = ThreadingHTTPServer((args.host, args.port), MiniAtsRequestHandler)
    print(f"Mini ATS web console: http://{args.host}:{args.port}")
    print(f"Gateway target: {args.gateway_host}:{args.gateway_port}")
    print(f"Market data UDP: {args.market_data_host}:{args.market_data_port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping Mini ATS web console")
    finally:
        server.shutdown()
        server.server_close()
        bridge.close()
    return 0


if __name__ == "__main__":
    os.chdir(REPO_ROOT)
    raise SystemExit(main())
