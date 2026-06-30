# Interview Guide

이 문서는 Mini ATS Matching System을 면접에서 설명하기 위한 요약입니다.
핵심은 "거래소 매칭 시스템의 핵심 경계를 작게 구현했고, 체결 정확성/결정성/검증 가능성을 중심으로 설계했다"는 점입니다.

## 1분 요약

Mini ATS는 단일 종목 기준의 작은 매매체결 시스템입니다.
C++20로 가격-시간 우선 order book과 matching engine을 구현했고, 주문 접수는 text command/TCP gateway 경계를 통해 받습니다.
주문은 sequence와 reference version을 가진 replay event로 변환되어 deterministic하게 처리됩니다.

체결 core는 외부 I/O와 분리했습니다.
가격/수량은 부동소수점이 아니라 정수 타입으로 표현했고, tick size, 가격 제한폭, 시장 세션, 중복 order id, quantity 같은 기준정보 검증을 matching 전에 수행합니다.
DAY/IOC/FOK, limit/market, 부분 체결, resting order cancel을 지원합니다.

운영 관측을 위해 gateway response, execution report, trade, book update, UDP market data publisher, stats collector, benchmark runner를 붙였고,
웹 콘솔은 Python bridge가 C++ TCP gateway와 UDP market data를 받아 Cloudflare Pages UI에 SSE로 전달하는 구조입니다.

## 전체 구조

```text
Browser UI
  -> Cloudflare Pages Functions
  -> Cloudflare Tunnel
  -> Python web/bridge.py
  -> C++ TCP gateway
  -> protocol parser
  -> replay validator
  -> matching engine
  -> order book / trades / execution reports
  -> market data / stats / web snapshot
```

모듈별 책임:

- `domain`: Order, Trade, ExecutionReport, InstrumentReference 같은 도메인 모델
- `engine`: OrderBook과 MatchingEngine
- `protocol`: `SUBMIT key=value...`, `CANCEL key=value...` text command parser
- `replay`: input sequence/reference version 검증과 deterministic replay 경계
- `gateway`: parse/replay/engine 결과를 canonical response로 변환
- `marketdata`: TradeEvent, BookUpdateEvent, UDP publisher
- `stats`: command count, accept/reject, trade count, traded quantity/notional, VWAP, latency percentiles
- `web`: local bridge와 Trading Console UI
- `functions`: Cloudflare Pages proxy functions

## 핵심 알고리즘

### 가격-시간 우선

매수는 높은 가격이 우선이고, 같은 가격이면 먼저 들어온 주문이 우선입니다.
매도는 낮은 가격이 우선이고, 같은 가격이면 먼저 들어온 주문이 우선입니다.

이를 위해 `OrderBook`은 side별 가격 level을 분리합니다.

- bid side: best bid를 빠르게 찾을 수 있는 가격 정렬 map
- ask side: best ask를 빠르게 찾을 수 있는 가격 정렬 map
- 각 가격 level: FIFO queue/list로 주문 순서 유지
- `locations_`: order id에서 실제 level/list iterator로 가는 index

이 구조 덕분에:

- best order 조회는 가격 map의 첫 level에서 front order를 보면 됩니다.
- 같은 가격의 시간 우선은 list push-back/front로 유지합니다.
- cancel은 order id로 `locations_`를 찾아 list iterator를 바로 지웁니다.
- 부분 체결은 `BookOrder.remaining_quantity`만 줄이고 같은 위치에 남깁니다.

### 체결 루프

신규 주문이 들어오면 `MatchingEngine::submit_order()`가 처리합니다.

1. 기준정보와 주문 유효성 검증
2. 중복 order id 검사
3. FOK이면 사전에 executable quantity 계산
4. incoming 잔량이 있고 반대 best order와 crossing이면 체결
5. 체결 가격은 incoming 가격이 아니라 resting order 가격으로 결정
6. resting order 잔량 감소, 필요하면 book에서 제거
7. trade와 execution report 생성
8. incoming 잔량이 남으면 DAY limit만 book에 resting
9. IOC/market 잔량은 cancel report로 종료

Crossing 조건:

- market order: 가격 조건 없이 반대 side best부터 체결
- limit buy: incoming price >= best ask
- limit sell: incoming price <= best bid

### DAY / IOC / FOK

- DAY: 즉시 체결 후 남은 잔량이 있으면 order book에 남김
- IOC: 즉시 체결 가능한 수량만 체결하고 남은 잔량은 취소
- FOK: 전체 수량이 즉시 체결 가능할 때만 체결, 아니면 book 변경 없이 거부

FOK는 실제 체결 루프 전에 `executable_quantity()`로 반대 side snapshot을 훑어 전체 수량을 채울 수 있는지 확인합니다.
부족하면 state mutation 없이 `WouldNotExecute`로 reject합니다.

## 결정성

matching core는 같은 입력 순서가 들어오면 같은 결과가 나와야 합니다.
이를 위해 core 내부에서 wall-clock time, random, thread scheduling에 의존하지 않았습니다.

결정성을 위해 입력에는 다음이 포함됩니다.

- input sequence
- reference data version
- order/cancel command payload

`protocol` parser는 text command를 `ReplayEvent`로 변환하고, `replay` 모듈은 sequence/reference version mismatch를 검증합니다.
accepted command는 replay log로 기록할 수 있어서 같은 입력을 다시 적용해 결과를 검증할 수 있습니다.

## 검증과 거부 처리

주문 처리 전 검증:

- instrument id mismatch
- market closed
- invalid quantity
- invalid price
- tick size / price band 위반
- duplicate order id
- invalid order id

gateway 단계 거부:

- parse error
- replay validation error
- engine rejected

이 거부 이유를 `GatewayResponse`에 canonical text로 내려서 테스트와 운영 로그에서 같은 형식으로 읽을 수 있게 했습니다.

## Market Data와 Web Console

C++ gateway는 accepted 결과에서 trade/book update market data event를 생성하고 UDP로 publish합니다.
Python `web/bridge.py`는 다음 역할을 합니다.

- HTTP API를 받아 text command 생성
- TCP gateway로 line-delimited command 전송
- gateway response를 JSON으로 파싱
- UDP market data를 수신해 book/trade snapshot 유지
- browser에 `/events` SSE로 snapshot push
- Cloudflare Pages 배포 시 `/api/*`, `/events`는 Pages Functions가 `ATS_BRIDGE_ORIGIN`으로 proxy

웹 콘솔은 order ticket, order book, session stats, trade tape, open orders, gateway stream을 보여줍니다.

## 운영 지표

`OperationalStatistics`는 다음을 집계합니다.

- command received / accepted / rejected
- trade count
- traded quantity
- traded notional
- exact VWAP
- latency min/max/p50/p95/p99

VWAP은 double 누적이 아니라 `notional / quantity` 구조로 보관해 정수 기반 가격/수량 모델과 일관성을 유지했습니다.

## 면접 질문별 답변

### 왜 이 프로젝트를 만들었나?

매매체결 IT 직무는 예측 모델보다 정확한 주문 처리, 결정성, 프로토콜 경계, 장애 복구 가능성이 중요하다고 봤습니다.
그래서 작은 범위지만 matching core, replay, gateway, market data, stats까지 연결된 시스템을 만들었습니다.

### 가장 중요한 구현은?

가격-시간 우선 order book과 matching engine입니다.
side별 가격 level과 level 내부 FIFO를 두고, order id index를 별도로 유지해서 best matching과 cancel을 모두 처리했습니다.
부분 체결은 report를 재삽입하지 않고 book 안의 `remaining_quantity`를 줄이는 방식으로 구현했습니다.

### FOK는 어떻게 구현했나?

FOK는 실제 주문장을 변경하기 전에 현재 반대 side에서 체결 가능한 수량을 snapshot 기준으로 계산합니다.
주문 수량보다 작으면 아무 trade도 만들지 않고 reject합니다.
충분하면 일반 matching loop로 들어갑니다.
이렇게 해서 FOK 실패 시 state mutation이 없도록 했습니다.

### IOC와 market order는 어떻게 다르나?

둘 다 잔량을 book에 남기지 않습니다.
IOC limit은 가격 조건을 만족하는 범위에서만 즉시 체결하고, market은 가격 조건 없이 반대 side best부터 가능한 만큼 체결합니다.
남은 수량은 cancel report로 종료합니다.

### 체결 가격은 어떻게 정했나?

체결 가격은 incoming order 가격이 아니라 resting order의 가격입니다.
실제 시장에서 aggressive order가 기존 book liquidity를 때리는 구조를 반영했습니다.

### 결정성은 어떻게 보장했나?

입력 순서를 sequence로 명시하고, reference version도 함께 검증합니다.
matching core는 시간/난수/비결정 thread scheduling에 의존하지 않습니다.
accepted input을 canonical replay log로 남겨 같은 순서로 재적용할 수 있습니다.

### 네트워크는 어떻게 붙였나?

core는 I/O를 모르게 했습니다.
gateway adapter가 text command를 받고, parser/replay/matching을 호출한 뒤 canonical response를 반환합니다.
TCP server는 line-delimited command를 읽는 얇은 adapter이고, market data는 UDP datagram 하나에 event 하나를 담아 publish합니다.

### 웹은 어떻게 붙였나?

C++ engine은 TCP/UDP 경계를 유지하고, Python bridge가 HTTP/SSE 친화적인 형태로 감쌉니다.
Cloudflare Pages에는 정적 UI와 proxy functions만 배포했고, Pages Functions가 tunnel을 통해 local bridge로 요청을 넘깁니다.

### 한계는?

현재는 단일 종목, in-memory state, 단일 프로세스 중심입니다.
실제 운영 수준의 persistence, HA, partitioning, multi-instrument routing, 인증/권한, binary protocol, lock-free queue, memory pool은 확장 후보로 남겼습니다.
다만 core correctness와 deterministic replay를 먼저 잡는 것이 우선이라고 판단했습니다.

## 30초 버전

"단일 종목 기준의 mini matching engine을 C++20로 구현했습니다. 핵심은 가격-시간 우선 order book이고, bid/ask 가격 level과 level 내부 FIFO, order id index로 부분 체결과 cancel을 처리했습니다. MatchingEngine은 limit/market, DAY/IOC/FOK를 지원하고, tick size와 가격 제한폭 같은 기준정보 검증을 먼저 수행합니다. 입력은 sequence와 reference version을 가진 replay event로 변환해서 결정성을 유지했고, gateway는 parse/replay/engine reject를 구분해 canonical response를 반환합니다. 이후 TCP gateway, UDP market data, operational stats, Python web bridge, Cloudflare Pages UI까지 붙여서 주문 접수부터 체결/호가/로그 시각화까지 end-to-end로 볼 수 있게 만들었습니다."

