const state = {
  snapshot: null,
  side: "BUY",
  orderType: "LIMIT",
  eventSource: null,
  settingsInitialized: false,
};

const els = {
  eventStatus: document.querySelector("#eventStatus"),
  engineStatus: document.querySelector("#engineStatus"),
  instrumentId: document.querySelector("#instrumentId"),
  referenceVersion: document.querySelector("#referenceVersion"),
  nextIds: document.querySelector("#nextIds"),
  orderForm: document.querySelector("#orderForm"),
  cancelForm: document.querySelector("#cancelForm"),
  rawForm: document.querySelector("#rawForm"),
  timeInForce: document.querySelector("#timeInForce"),
  price: document.querySelector("#price"),
  quantity: document.querySelector("#quantity"),
  orderId: document.querySelector("#orderId"),
  cancelOrderId: document.querySelector("#cancelOrderId"),
  rawCommand: document.querySelector("#rawCommand"),
  submitOrder: document.querySelector("#submitOrder"),
  message: document.querySelector("#message"),
  bestBid: document.querySelector("#bestBid"),
  bestAsk: document.querySelector("#bestAsk"),
  spread: document.querySelector("#spread"),
  asks: document.querySelector("#asks"),
  bids: document.querySelector("#bids"),
  statsGrid: document.querySelector("#statsGrid"),
  vwap: document.querySelector("#vwap"),
  tradeCount: document.querySelector("#tradeCount"),
  tradesBody: document.querySelector("#tradesBody"),
  openOrderCount: document.querySelector("#openOrderCount"),
  openOrders: document.querySelector("#openOrders"),
  responseStream: document.querySelector("#responseStream"),
  refreshSnapshot: document.querySelector("#refreshSnapshot"),
};

const numberFormat = new Intl.NumberFormat("en-US");

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function formatNumber(value) {
  if (value === null || value === undefined || value === "") {
    return "-";
  }
  return numberFormat.format(Number(value));
}

function formatPrice(value) {
  return formatNumber(value);
}

function formatSide(side) {
  return side === "BUY" ? "Buy" : side === "SELL" ? "Sell" : "-";
}

function api(path, body) {
  return fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  }).then(async (response) => {
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    return payload;
  });
}

function setMessage(text, kind = "") {
  els.message.textContent = text;
  els.message.className = `message ${kind}`.trim();
}

function getSettings() {
  return {
    instrumentId: Number(els.instrumentId.value),
    referenceVersion: Number(els.referenceVersion.value),
  };
}

function setControl(control, value) {
  state[control] = value;
  document.querySelectorAll(`[data-control="${control}"]`).forEach((button) => {
    button.classList.toggle("active", button.dataset.value === value);
  });
  syncTicketState();
}

function syncTicketState() {
  const isBuy = state.side === "BUY";
  els.submitOrder.textContent = isBuy ? "Submit Buy" : "Submit Sell";
  els.submitOrder.classList.toggle("buy-action", isBuy);
  els.submitOrder.classList.toggle("sell-action", !isBuy);
  els.price.disabled = state.orderType === "MARKET";
}

function applySnapshot(snapshot) {
  state.snapshot = snapshot;
  if (!state.settingsInitialized && snapshot.defaults) {
    els.instrumentId.value = snapshot.defaults.instrumentId ?? 1001;
    els.referenceVersion.value = snapshot.defaults.referenceVersion ?? 7;
    state.settingsInitialized = true;
  }
  render();
}

function render() {
  const snapshot = state.snapshot;
  if (!snapshot) {
    return;
  }
  renderStatus(snapshot);
  renderBook(snapshot.book || {});
  renderStats(snapshot.stats || {});
  renderTrades(snapshot.trades || []);
  renderOpenOrders(snapshot.openOrders || []);
  renderResponses(snapshot.responses || []);
  els.nextIds.textContent = `seq ${snapshot.nextSequence ?? "-"} / order ${snapshot.nextOrderId ?? "-"}`;
}

function renderStatus(snapshot) {
  const health = snapshot.health || {};
  const online = Boolean(health.engineConnected);
  els.engineStatus.textContent = online ? "Gateway Online" : "Gateway Offline";
  els.engineStatus.className = `status-pill ${online ? "on" : "off"}`;
}

function renderBook(book) {
  const bestBid = book.bestBid;
  const bestAsk = book.bestAsk;
  els.bestBid.textContent = bestBid ? formatPrice(bestBid.price) : "-";
  els.bestAsk.textContent = bestAsk ? formatPrice(bestAsk.price) : "-";

  if (bestBid && bestAsk) {
    els.spread.textContent = `spread ${formatNumber(Number(bestAsk.price) - Number(bestBid.price))}`;
  } else {
    els.spread.textContent = "spread -";
  }

  const levels = [...(book.bids || []), ...(book.asks || [])];
  const maxQuantity = Math.max(1, ...levels.map((level) => Number(level.quantity || 0)));
  renderLevels(els.asks, book.asks || [], "ask", maxQuantity);
  renderLevels(els.bids, book.bids || [], "bid", maxQuantity);
}

function renderLevels(container, levels, side, maxQuantity) {
  if (!levels.length) {
    container.innerHTML = `<div class="level-row empty"><span>No ${side}s</span><span>-</span></div>`;
    return;
  }

  container.innerHTML = levels
    .map((level) => {
      const quantity = Number(level.quantity || 0);
      const depth = Math.max(4, Math.min(100, (quantity / maxQuantity) * 100));
      return `
        <div class="level-row">
          <div class="level-fill" style="--depth: ${depth}%"></div>
          <span>${formatNumber(quantity)}</span>
          <span class="level-price ${side}">${formatPrice(level.price)}</span>
        </div>
      `;
    })
    .join("");
}

function renderStats(stats) {
  const tradedQuantity = Number(stats.tradedQuantity || 0);
  const tradedNotional = Number(stats.tradedNotional || 0);
  const vwap = tradedQuantity > 0 ? Math.floor(tradedNotional / tradedQuantity) : null;
  els.vwap.textContent = `VWAP ${formatPrice(vwap)}`;

  const rows = [
    ["Commands", stats.commands || 0],
    ["Accepted", stats.accepted || 0],
    ["Rejected", stats.rejected || 0],
    ["Trades", stats.trades || 0],
    ["Quantity", tradedQuantity],
    ["Notional", tradedNotional],
  ];

  els.statsGrid.innerHTML = rows
    .map(
      ([label, value]) => `
        <div class="stat">
          <span>${escapeHtml(label)}</span>
          <strong>${formatNumber(value)}</strong>
        </div>
      `,
    )
    .join("");
}

function renderTrades(trades) {
  els.tradeCount.textContent = `${trades.length} trades`;
  if (!trades.length) {
    els.tradesBody.innerHTML = `<tr><td colspan="5" class="empty-state">No trades</td></tr>`;
    return;
  }

  els.tradesBody.innerHTML = trades
    .slice(0, 80)
    .map((trade) => {
      const side = trade.aggressorSide;
      const sideClass = side === "BUY" ? "side-buy" : side === "SELL" ? "side-sell" : "";
      return `
        <tr>
          <td>${escapeHtml(trade.sequence ?? "-")}</td>
          <td class="${sideClass}">${escapeHtml(formatSide(side))}</td>
          <td>${formatPrice(trade.price)}</td>
          <td>${formatNumber(trade.quantity)}</td>
          <td>${escapeHtml(trade.incomingOrderId ?? "-")}</td>
        </tr>
      `;
    })
    .join("");
}

function renderOpenOrders(orders) {
  els.openOrderCount.textContent = `${orders.length} open`;
  if (!orders.length) {
    els.openOrders.innerHTML = `<div class="empty-state">No open orders</div>`;
    return;
  }

  els.openOrders.innerHTML = orders
    .map((order) => {
      const sideClass = order.side === "BUY" ? "side-buy" : "side-sell";
      const price = order.orderType === "MARKET" ? "MKT" : formatPrice(order.price);
      return `
        <div class="order-row">
          <div class="order-main">
            <div class="order-title">
              <span class="${sideClass}">#${escapeHtml(order.orderId)}</span>
              <span>${escapeHtml(formatSide(order.side))}</span>
              <span>${escapeHtml(order.orderType || "-")}</span>
            </div>
            <div class="order-meta">
              ${price} / remaining ${formatNumber(order.remainingQuantity)}
            </div>
          </div>
          <button type="button" data-cancel-order="${escapeHtml(order.orderId)}">Cancel</button>
        </div>
      `;
    })
    .join("");
}

function renderResponses(responses) {
  if (!responses.length) {
    els.responseStream.innerHTML = `<div class="empty-state">No gateway responses</div>`;
    return;
  }

  els.responseStream.innerHTML = responses
    .slice(0, 80)
    .map((response) => {
      const accepted = response.status === "ACCEPTED";
      const label = accepted ? "accepted" : "rejected";
      const detail = response.detail && response.detail !== "NONE" ? response.detail : response.reason;
      return `
        <div class="stream-row ${label}">
          <div class="stream-main">
            <div class="stream-title">
              <span>${escapeHtml(response.status || "UNKNOWN")}</span>
              <span>seq ${escapeHtml(response.sequence ?? "-")}</span>
              <span>${escapeHtml(response.commandType || "-")}</span>
              <span>${escapeHtml(detail || "")}</span>
            </div>
            <div class="stream-raw">${escapeHtml(response.raw || "")}</div>
          </div>
        </div>
      `;
    })
    .join("");
}

async function submitOrder(event) {
  event.preventDefault();
  const settings = getSettings();
  const body = {
    ...settings,
    side: state.side,
    orderType: state.orderType,
    timeInForce: els.timeInForce.value,
    price: els.price.value,
    quantity: els.quantity.value,
    orderId: els.orderId.value || undefined,
  };

  if (state.orderType === "MARKET") {
    body.price = undefined;
  }

  await sendAction(() => api("/api/order", body));
  els.orderId.value = "";
}

async function cancelOrder(event) {
  event.preventDefault();
  await cancelOrderId(els.cancelOrderId.value);
}

async function cancelOrderId(orderId) {
  const settings = getSettings();
  await sendAction(() =>
    api("/api/cancel", {
      ...settings,
      orderId,
    }),
  );
  els.cancelOrderId.value = "";
}

async function sendRaw(event) {
  event.preventDefault();
  await sendAction(() => api("/api/raw", { command: els.rawCommand.value }));
}

async function sendAction(action) {
  setMessage("Sending...", "");
  setBusy(true);
  try {
    const payload = await action();
    if (payload.snapshot) {
      applySnapshot(payload.snapshot);
    }
    const response = payload.response || {};
    const ok = response.status === "ACCEPTED";
    setMessage(response.raw || "OK", ok ? "ok" : "error");
  } catch (error) {
    setMessage(error.message || String(error), "error");
  } finally {
    setBusy(false);
  }
}

function setBusy(isBusy) {
  els.submitOrder.disabled = isBusy;
  document.querySelectorAll(".secondary-action").forEach((button) => {
    button.disabled = isBusy;
  });
}

async function fetchSnapshot() {
  try {
    const response = await fetch("/api/snapshot");
    const snapshot = await response.json();
    applySnapshot(snapshot);
  } catch (error) {
    els.engineStatus.textContent = "Bridge Offline";
    els.engineStatus.className = "status-pill off";
  }
}

function connectEvents() {
  if (state.eventSource) {
    state.eventSource.close();
  }
  const source = new EventSource("/events");
  state.eventSource = source;

  source.onopen = () => {
    els.eventStatus.textContent = "Live";
    els.eventStatus.className = "status-pill on";
  };

  source.onerror = () => {
    els.eventStatus.textContent = "Reconnecting";
    els.eventStatus.className = "status-pill warn";
  };

  source.onmessage = (event) => {
    try {
      const payload = JSON.parse(event.data);
      if (payload.snapshot) {
        applySnapshot(payload.snapshot);
      }
    } catch (error) {
      setMessage(error.message || String(error), "error");
    }
  };
}

document.querySelectorAll("[data-control]").forEach((button) => {
  button.addEventListener("click", () => {
    setControl(button.dataset.control, button.dataset.value);
  });
});

els.orderForm.addEventListener("submit", submitOrder);
els.cancelForm.addEventListener("submit", cancelOrder);
els.rawForm.addEventListener("submit", sendRaw);
els.refreshSnapshot.addEventListener("click", fetchSnapshot);

els.openOrders.addEventListener("click", (event) => {
  const button = event.target.closest("[data-cancel-order]");
  if (!button) {
    return;
  }
  cancelOrderId(button.dataset.cancelOrder);
});

setControl("side", "BUY");
setControl("orderType", "LIMIT");
fetchSnapshot();
connectEvents();
setInterval(fetchSnapshot, 5000);
