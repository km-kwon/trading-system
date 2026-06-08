# 아키텍처

Mini ATS Matching System은 결정적인 matching core를 중심으로 구성합니다. 같은 순서의 주문 입력이 들어오면 항상 같은 체결 결과가 나와야 하며, 이 특성을 기준으로 모듈 경계를 나눕니다.

## 예정 모듈

- `domain`: 정수 기반 가격/수량 타입, 주문, 체결, 이벤트 모델
- `reference_data`: PostgreSQL 기준정보 row를 matching core 입력 모델로 변환하는 adapter 경계
- `engine`: in-memory order book과 가격-시간 우선 matching engine
- `replay`: 주문 입력 로그를 deterministic matching core에 재적용하고, replay log를 읽고 쓰는 복구/replay 경계
- `protocol`: 사람이 읽기 쉬운 text command를 replay input model로 변환하는 parser
- `gateway`: 주문 접수 request/response와 TCP adapter 전 단계의 reject mapping 경계
- `marketdata`: UDP 기반 체결/호가 이벤트 분배기
- `stats`: 거래량, 거래대금, VWAP, latency 통계

## 현재 범위

현재 단계에서는 in-memory matching core, 기준정보 검증 모델, PostgreSQL instrument row mapping adapter, replay input model, text command parser, replay log file I/O, gateway request/response model, response text formatter, accepted input recorder, TCP order gateway skeleton, market data event model, UDP market data publisher skeleton, gateway와 publisher 연결 경계, 운영 통계 model, stdin gateway stats 출력을 제공합니다. 실제 DB connection adapter와 TCP/benchmark 통계 노출은 이후 단계에서 순서대로 추가합니다.

## 현재 구현된 domain model

- `Order`: 주문 id, 종목 id, 매수/매도, 주문 유형, TIF, 가격, 수량, sequence
- `InstrumentReference`: 종목 id, 호가단위, 가격 제한폭, 시장 세션, 기준정보 version
- `Trade`: 체결 id, 종목 id, resting/incoming 주문 id, aggressor side, 가격, 수량, sequence
- `ExecutionReport`: 주문 처리 결과, 주문 상태, 체결 수량, 잔량, 마지막 체결 가격/수량, 거부 사유
- `CancelRequest`: 취소 대상 주문 id, 종목 id, sequence

## 현재 구현된 engine model

- `BookOrder`: 주문장 내부에 저장되는 주문 상태입니다. 원 주문인 `Order`와 현재 잔량인 `remaining_quantity`를 함께 보관합니다.
- `OrderBook`: 종목별 in-memory 주문장입니다. 매수/매도 side를 분리하고, 가격 level 안에서는 FIFO 순서를 유지합니다.
- `MatchingEngine`: 신규 주문을 받아 기준정보를 검증한 뒤 반대 side best order와 crossing 여부를 판단하고, 체결 시 `Trade`와 `ExecutionReport`를 생성합니다. 지정가 DAY, 시장가, IOC, FOK, resting order 취소를 지원합니다.

부분 체결이 발생하면 `ExecutionReport`가 다시 주문장 queue에 들어가는 것이 아닙니다. 주문장에 남는 것은 `remaining_quantity`가 감소한 `BookOrder`입니다.

`OrderBookSnapshot`은 현재 주문장 상태를 외부에서 읽기 위한 복사본입니다. `format_order_book()`은 이 snapshot을 사람이 읽기 쉬운 텍스트로 변환합니다.

시장가 주문과 IOC 주문은 즉시 체결 가능한 수량만 처리하고 잔량을 주문장에 남기지 않습니다. FOK 주문은 사전에 체결 가능 수량을 확인해 전체 수량을 채울 수 없으면 주문장을 변경하지 않고 거부합니다.

MatchingEngine은 `InstrumentReference`를 생성자에서 주입받습니다. 기본 생성자는 기존 단일 종목 테스트를 위해 tick size 1, 열린 시장, 사실상 제한 없는 가격 band를 사용합니다. 이후 PostgreSQL adapter는 `mini_ats.instruments` 기준정보를 읽어 이 구조체로 변환하는 얇은 경계가 됩니다.

`reference_data` 모듈은 DB 연결 세부사항을 matching core에서 분리합니다. 현재는 `InstrumentRecord`를 `InstrumentReference`로 변환하는 순수 mapping과 parameterized SQL query를 제공합니다. 실제 PostgreSQL connection 구현은 이 mapping 뒤에 붙습니다.

`replay` 모듈은 입력 로그 한 줄을 `ReplayEvent`로 표현합니다. 각 event는 input sequence, reference data version, 실제 명령(`Order` 또는 `CancelRequest`)을 함께 갖고, replay 전에 sequence와 기준정보 version이 matching engine 상태와 맞는지 검증합니다.

`replay_log_io`는 text command 여러 줄을 stream/file에서 읽어 `ReplayEvent` 목록으로 변환하고, `ReplayEvent` 목록을 canonical text command로 다시 기록합니다. 빈 줄과 `#` 주석은 skip하고, parse 실패 시 line number, field, 원문 line을 반환합니다.

`protocol` 모듈은 `SUBMIT key=value...`, `CANCEL key=value...` 형식의 text command를 `ReplayEvent`로 변환합니다. 이 parser는 이후 TCP 주문 접수와 replay log file reader가 공유할 입력 경계입니다.

`gateway` 모듈은 text command를 접수해 parser, replay validation, matching engine 적용 결과를 `GatewayResponse`로 변환합니다. parse 실패는 `PARSE_ERROR`, replay sequence/reference 검증 실패는 `REPLAY_VALIDATION_ERROR`, matching engine의 주문 거부 report는 `ENGINE_REJECTED`로 분류합니다. `format_gateway_response()`는 이 응답을 한 줄짜리 canonical text로 변환합니다. `handle_recorded_text_command()`는 accepted command만 canonical replay log line으로 기록합니다. `handle_published_text_command()`는 같은 적용 결과에서 market data event를 생성하고 주입된 publisher로 전송합니다. `TcpOrderServer`는 line-delimited TCP command를 읽고 같은 gateway recorder/formatter 경계를 호출하며, publisher가 주입된 경우 accepted result를 UDP market data 경로로도 전달하는 얇은 socket adapter입니다.

`marketdata` 모듈은 matching 결과를 외부 분배용 이벤트로 변환하는 adapter 경계입니다. `TradeEvent`는 `Trade`의 public market data 필드를 복사하고, `BookUpdateEvent`는 최종 `OrderBookSnapshot`에서 best bid/ask와 지정 depth의 가격 level을 담습니다. reject result는 market data event를 만들지 않습니다. submit result는 trade가 있거나 resting order가 추가된 경우 book update를 만들고, cancel result는 resting order가 제거된 경우 book update를 만듭니다. `format_market_data_event()`는 이벤트를 한 줄짜리 canonical text payload로 변환하고, `UdpMarketDataPublisher`는 이벤트 하나를 UDP datagram 하나로 전송합니다. 현재 실행 파일은 TCP gateway 모드에서 `--market-data <addr> <port>` 옵션으로 이 publisher를 선택적으로 연결할 수 있습니다.

`stats` 모듈은 matching/gateway 경로에서 나온 결과를 운영 지표로 집계하는 순수 model입니다. `OperationalStatistics`는 command 수신/accepted/rejected count, trade count, 거래량, 거래대금, exact VWAP, latency min/max/p50/p95/p99 snapshot을 제공합니다. VWAP은 `double` 가격이 아니라 `notional / quantity` 비율인 `ExactVwap`으로 보관합니다. `record_submit_result()`는 `SubmitOrderResult`의 trade를 집계하고, gateway 경로에서는 response accepted 여부, `response.trades`, 측정 latency를 `record_command_result()`에 넘겨 같은 model을 사용할 수 있습니다. `format_operational_statistics()`는 snapshot을 한 줄짜리 canonical text로 변환합니다. 현재 stdin gateway runner는 `--stats` 옵션이 지정된 경우 처리 종료 시 stats snapshot을 stderr로 출력합니다.

## 설계 방향

- matching core는 외부 I/O와 분리합니다.
- 주문 입력 순서는 명시적인 sequence로 관리합니다.
- 가격과 수량은 `double`이 아닌 정수 타입으로 표현합니다.
- PostgreSQL 기준정보와 in-memory matching state를 분리합니다.
- TCP/UDP network I/O는 core/gateway model이 안정된 뒤 얇은 adapter로 붙입니다.
