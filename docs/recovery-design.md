# 복구 설계

복구 설계는 replay 기반 복구 방향을 정리하기 위한 문서입니다. 현재는 입력 로그 모델, replay log I/O, matching core 재적용 경계, gateway 접수 응답 경계, accepted input recorder, market data publish 경계, 운영 통계 집계 model을 구현했습니다.

## 예정 방향

- 접수 완료된 주문 입력 스트림을 저장합니다.
- 저장된 입력을 deterministic replay하여 in-memory state를 재구성합니다.
- replay 결과로 생성된 체결 이벤트와 기존 기록을 비교합니다.
- PostgreSQL 기준정보와 volatile matching state를 분리합니다.
- replay 시점에 사용한 기준정보 version을 함께 기록합니다.

## 핵심 전제

복구가 가능하려면 matching core가 결정적이어야 합니다. 즉, 입력 순서와 기준정보가 같으면 order book 상태와 체결 결과도 같아야 합니다.

이를 위해 주문 sequence, 기준정보 version, 체결 이벤트 sequence를 명확히 기록하는 구조를 목표로 합니다.

## 현재 기준정보 계약

현재 matching core는 `InstrumentReference`를 통해 다음 값을 주입받습니다.

- 종목 id
- tick size
- 가격 제한폭
- 시장 세션
- 기준정보 version

`reference_data` 모듈은 PostgreSQL의 `mini_ats.instruments` row를 `InstrumentReference`로 변환합니다. 현재는 순수 row mapping과 `psql` 기반 단일 instrument loader adapter를 제공하며, `--load-instrument --instrument-id <id>` CLI로 해당 경계를 확인할 수 있습니다. stdin/TCP gateway는 `--load-reference-data --instrument-id <id>` 옵션으로 같은 loader 결과를 matching core에 주입할 수 있습니다.

복구 replay에서는 주문 입력 sequence만 같아서는 충분하지 않습니다. 같은 주문이라도 tick size, 가격 제한폭, 시장 세션이 다르면 접수/거부 결과가 달라질 수 있기 때문입니다. 따라서 이후 입력 로그에는 주문 sequence와 함께 기준정보 version을 기록하고, replay 전에 동일한 version의 기준정보를 core에 주입하는 구조로 확장합니다.

## 현재 replay 입력 모델

현재 `ReplayEvent`는 다음 값을 기록합니다.

- 입력 로그 sequence
- 기준정보 version
- 실제 명령: 신규 주문 또는 취소 요청

Replay 적용 전에는 다음 조건을 확인합니다.

- 입력 로그 sequence가 0이 아닌지
- 기준정보 version이 0이 아닌지
- 입력 로그 sequence와 명령 내부 sequence가 같은지
- 입력 기준정보 version과 MatchingEngine의 `InstrumentReference.version`이 같은지

검증에 실패하면 matching state를 변경하지 않고 replay를 중단합니다. 검증을 통과하면 `MatchingEngine::submit_order()` 또는 `MatchingEngine::cancel_order()`를 호출하고, 생성된 `Trade`와 `ExecutionReport`를 replay 결과에 모읍니다.

## 현재 replay log I/O

현재 replay log reader는 text command format을 사용합니다.

```text
# comment
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3
CANCEL seq=2 ref=7 order_id=10 instrument_id=1001
```

reader 동작:

- 빈 줄과 `#` 주석 줄은 건너뜁니다.
- 나머지 줄은 text command parser로 `ReplayEvent`로 변환합니다.
- parse 실패 시 line number, parser error, field, 원문 line을 반환합니다.

writer 동작:

- `ReplayEvent`를 canonical text command 한 줄로 변환합니다.
- stream 또는 file path에 기록할 수 있습니다.

## 현재 gateway와 복구 경계

`gateway` 모듈은 text command를 `ReplayEvent`로 변환한 뒤 matching core에 적용하고, 결과를 `GatewayResponse`로 반환합니다. 이 응답은 `format_gateway_response()`로 한 줄짜리 canonical text response로 변환할 수 있습니다. 이때 parse 실패와 replay validation 실패는 matching state를 변경하지 않습니다.

`handle_recorded_text_command()`는 gateway response가 accepted인 command만 `format_replay_event()` 결과로 replay log stream에 기록합니다. parser reject, replay validation reject, matching engine reject는 accepted input log에 남기지 않습니다.

`handle_published_text_command()`는 같은 accepted command 적용 결과에서 market data event를 만든 뒤 주입된 publisher로 전송합니다. rejected command는 accepted input log에도, market data stream에도 남기지 않습니다.

`TcpOrderServer`도 같은 recorder/publisher 경계를 호출하므로 stdin runner와 TCP 접수 경로의 accepted input 기록 규칙이 같습니다. TCP gateway에 `--market-data <addr> <port>`를 지정하면 accepted matching 결과가 UDP market data payload로도 전송됩니다. `--stats`를 지정하면 TCP command 처리 결과도 `OperationalStatistics`에 기록됩니다.

`marketdata` 모듈은 replay로 다시 얻은 `SubmitOrderResult`/`CancelOrderResult`와 최종 `OrderBookSnapshot`에서 `TradeEvent`/`BookUpdateEvent`를 다시 만들 수 있는 순수 adapter입니다. `format_market_data_event()`도 deterministic text payload를 만들기 때문에 같은 accepted input log와 같은 기준정보를 replay하면 market data event stream과 UDP payload도 결정적으로 재생할 수 있습니다.

`stats` 모듈의 거래량, 거래대금, VWAP 같은 trade 기반 지표도 replay 결과의 trade 목록에서 다시 집계할 수 있습니다. 반면 command 처리 latency는 실제 운영 시점의 측정값이므로 replay만으로 복구되는 matching state와는 분리해 별도 운영 지표로 다룹니다.

이 구조 덕분에 TCP 주문 접수 서버는 accepted input을 replay log writer에 기록하고, 같은 text command stream을 복구 replay에 재사용할 수 있습니다. 같은 기준정보와 같은 accepted input log를 replay하면 market data event와 trade 기반 통계도 같은 순서와 값으로 다시 만들 수 있습니다.
