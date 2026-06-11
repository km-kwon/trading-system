# 프로토콜

프로토콜 상세는 matching core가 안정된 뒤 확정합니다. 현재 단계에서는 TCP 주문 접수와 UDP 이벤트 분배의 역할을 정의하고, 주문 접수/replay에서 공통으로 사용할 text command format과 gateway response text format, 선택적 UDP market data publish 경로를 구현했습니다.

## 예정 범위

- TCP 주문 접수 메시지
- UDP 체결 이벤트
- UDP 호가 이벤트
- replay 가능한 안정적인 message sequence number

## 기본 방향

주문 접수 경로는 TCP를 사용합니다. 주문 입력의 순서와 수신 결과를 명확히 관리하기 위해서입니다.

체결 이벤트와 호가 이벤트는 UDP 분배를 목표로 합니다. market data 성격의 이벤트를 core와 분리해 publish하는 구조를 연습하기 위해서입니다.

프로토콜 필드는 사람이 읽기 쉬운 text format으로 시작하고, core가 안정된 뒤 binary format을 검토합니다.

## 현재 text command format

현재 parser는 공백으로 구분된 `KEY=VALUE` 형식을 사용합니다.

신규 주문:

```text
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
```

취소:

```text
CANCEL seq=2 ref=7 order_id=10 instrument_id=1001
```

지원 값:

- `side`: `BUY`, `SELL`
- `type`: `LIMIT`, `MARKET`
- `tif`: `DAY`, `IOC`, `FOK`

필드 alias:

- `seq` 또는 `input_sequence`
- `ref` 또는 `reference_version`
- `order_id` 또는 `id`
- `instrument_id` 또는 `instrument`
- `type` 또는 `order_type`
- `tif` 또는 `time_in_force`
- `quantity` 또는 `qty`

`SUBMIT`은 `ReplayEvent`의 신규 주문 command로 변환됩니다. `CANCEL`은 취소 command로 변환됩니다. parser는 text command를 matching core에 직접 적용하지 않고 replay input model로 변환합니다.

Replay log 파일에서는 빈 줄과 `#`로 시작하는 주석 줄을 건너뜁니다. 그 외 줄은 같은 text command parser를 통과합니다.

## 현재 gateway response format

Gateway는 text command 한 줄을 받아 `GatewayResponse`로 변환한 뒤 canonical text response로 format할 수 있습니다. TCP skeleton은 newline-delimited command를 읽고 newline-delimited response를 돌려주는 얇은 socket adapter로 구현했습니다.

현재 실행 파일은 `--gateway` 모드에서 stdin으로 들어온 text command를 한 줄씩 처리합니다. 빈 줄과 `#` 주석 줄은 건너뜁니다. `--record-log <path>`를 함께 주면 accepted command만 canonical replay log line으로 append합니다.

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway
```

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway --record-log accepted-input.log
```

`--stats`를 함께 주면 처리된 command count, trade 집계, latency percentile을 stderr에 한 줄로 출력합니다.

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3' \
  'SUBMIT seq=2 ref=7 order_id=11 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=3' \
  | ./build/mini_ats --gateway --stats
```

PostgreSQL 기준정보를 matching engine에 주입하려면 `--load-reference-data --instrument-id <id>`를 지정합니다. 이때 command의 `ref` 값은 DB row의 `reference_version`과 같아야 합니다.

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway --load-reference-data --instrument-id 1001
```

TCP skeleton 실행:

```bash
./build/mini_ats --tcp --port 9001 --record-log accepted-input.log
```

TCP gateway에서 market data를 함께 publish하려면 UDP destination을 지정합니다.

```bash
./build/mini_ats --tcp --port 9001 --record-log accepted-input.log --market-data 127.0.0.1 9100
```

TCP gateway도 `--stats`를 지원합니다. TCP server는 장시간 실행되므로 정상 실행 중에는 계속 serve하고, 종료 또는 오류 경로에서 stats snapshot을 stderr로 출력합니다.

```bash
./build/mini_ats --tcp --port 9001 --record-log accepted-input.log --stats
```

TCP gateway에서도 같은 기준정보 loader 옵션을 사용할 수 있습니다.

```bash
./build/mini_ats --tcp --port 9001 --load-reference-data --instrument-id 1001 \
  --db-name mini_ats --db-user "$USER" --psql psql
```

Deterministic benchmark runner는 고정 gateway 입력 시나리오를 반복 실행하고, 결과를 한 줄 payload로 출력합니다.

```bash
./build/mini_ats --benchmark --iterations 1000
```

결과를 파일에도 남기려면 `--output <path>`를 지정합니다. 같은 payload가 stdout으로 출력되고 파일에는 append됩니다.

```bash
./build/mini_ats --benchmark --iterations 1000 --output benchmark-results.log
```

단일 instrument 기준정보를 PostgreSQL에서 읽어오는 CLI도 제공합니다. 이 경로는 `psql` 실행 파일을 사용하고, 성공 시 `INSTRUMENT ...` 한 줄을 출력합니다.

```bash
./build/mini_ats --load-instrument --instrument-id 1001
```

Instrument load text 예시:

```text
INSTRUMENT instrument_id=1001 tick_size=5 lower_price_limit=70000 upper_price_limit=80000 session=OPEN reference_version=7
```

TCP client는 한 command를 `\n`으로 끝내야 하며, server는 처리한 command마다 response text 한 줄을 돌려줍니다. 현재 server는 `127.0.0.1`에 bind하고 client를 순차 처리하는 skeleton입니다.

응답 status:

- `ACCEPTED`: parser, replay validation, matching engine 적용을 통과한 입력
- `REJECTED`: 접수 또는 처리 경계에서 거부된 입력

거부 reason:

- `PARSE_ERROR`: text command 형식 오류, 필수 필드 누락, enum/number 변환 실패
- `REPLAY_VALIDATION_ERROR`: 입력 sequence 또는 기준정보 version 검증 실패
- `ENGINE_REJECTED`: matching engine이 가격 단위, 가격 제한폭, 시장 세션 등 domain rule로 거부

`GatewayResponse`는 command type, input sequence, 상세 reason text, trade 목록, execution report 목록을 함께 보관합니다.

Stats text 예시:

```text
STATS commands_received=2 commands_accepted=2 commands_rejected=0 trades=1 traded_quantity=3 traded_notional=221100 vwap_notional=221100 vwap_quantity=3 vwap_floor_price=73700 latency_samples=2 latency_min_ns=10000 latency_max_ns=50000 latency_p50_ns=10000 latency_p95_ns=50000 latency_p99_ns=50000
```

Benchmark text 예시:

```text
BENCHMARK scenario=deterministic_gateway iterations=2 commands=6 elapsed_ns=861707 commands_per_second_floor=6962 compiler=gcc-13.3.0 cpp_standard=202002 build_mode=debug os=linux architecture=x86_64 hardware_threads=28 STATS commands_received=6 commands_accepted=4 commands_rejected=2 trades=2 traded_quantity=6 traded_notional=442200 vwap_notional=442200 vwap_quantity=6 vwap_floor_price=73700 latency_samples=6 latency_min_ns=7619 latency_max_ns=783971 latency_p50_ns=8338 latency_p95_ns=783971 latency_p99_ns=783971
```

Accepted input recorder는 `ACCEPTED` 응답만 replay log에 기록합니다. `PARSE_ERROR`, `REPLAY_VALIDATION_ERROR`, `ENGINE_REJECTED` 응답은 복구 입력 스트림에 남기지 않습니다.

응답 text 예시:

```text
ACCEPTED reason=NONE seq=1 command=SUBMIT detail=ACCEPTED trades=0 reports=1 report0_order_id=10 report0_instrument_id=1001 report0_type=ACCEPTED report0_status=ACCEPTED report0_filled_quantity=0 report0_remaining_quantity=10 report0_last_price=0 report0_last_quantity=0 report0_reject_reason=NONE report0_sequence=1
```

Parser reject 예시:

```text
REJECTED reason=PARSE_ERROR seq=0 command=NONE detail=MISSING_FIELD:price trades=0 reports=0
```

## 현재 market data event model

현재 단계에서는 UDP publisher가 사용할 event model, matching result adapter, canonical text payload formatter, UDP publisher skeleton을 구현했습니다.

`TradeEvent` 필드:

- `trade_id`
- `instrument_id`
- `resting_order_id`
- `incoming_order_id`
- `aggressor_side`
- `price`
- `quantity`
- `sequence`

`BookUpdateEvent` 필드:

- `instrument_id`
- `sequence`
- `best_bid`
- `best_ask`
- `bids`
- `asks`

`bids`와 `asks`는 `OrderBookSnapshot`의 price-time priority 순서에서 지정 depth만큼 잘라낸 가격 level입니다. 기본 depth는 1입니다.

Event 생성 규칙:

- `Trade`는 `TradeEvent`로 변환합니다.
- resting 주문 추가, 체결, 취소처럼 book 상태가 바뀐 accepted result는 최종 book snapshot으로 `BookUpdateEvent`를 만듭니다.
- parser/replay/engine reject와 체결 없는 IOC cancel은 market data event를 만들지 않습니다.

UDP payload format:

```text
TRADE seq=2 trade_id=1 instrument_id=1001 resting_order_id=20 incoming_order_id=21 aggressor_side=BUY price=73700 quantity=3
```

```text
BOOK_UPDATE seq=4 instrument_id=1001 best_bid_price=73700 best_bid_quantity=2 best_ask_price=NONE best_ask_quantity=0 bids=1 asks=0 bid0_price=73700 bid0_quantity=2
```

`UdpMarketDataPublisher`는 event 하나를 datagram 하나로 전송합니다. 현재 publisher는 IPv4 remote address와 port를 받아 `sendto()`로 payload를 보냅니다. TCP gateway는 `--market-data <addr> <port>`가 지정된 경우 accepted matching 결과에서 생성된 `TradeEvent`/`BookUpdateEvent`를 이 publisher로 전달합니다. parser/replay/engine reject는 market data payload를 만들지 않습니다.
