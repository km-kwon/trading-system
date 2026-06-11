# 개발 기록

이 문서는 단계별 구현이 끝날 때마다 입력, 실행 명령, 출력, 결과 판단을 순차적으로 남기기 위한 기록입니다.

## 기록 원칙

- 각 단계는 `목표`, `입력`, `변경 내용`, `실행 명령`, `출력`, `결과 판단`, `다음 단계` 순서로 기록합니다.
- 테스트 출력은 핵심 라인 위주로 남깁니다.
- 실패한 명령도 숨기지 않고 원인과 후속 조치를 기록합니다.
- 같은 입력을 replay했을 때 같은 결과가 나오는지 확인할 수 있게, 주문/이벤트 입력은 이후 단계부터 별도 예제로 남깁니다.

## 2026-06-05 1단계: 프로젝트 골격 생성

### 목표

CMake 기반 C++20 프로젝트 골격을 만들고, GoogleTest로 최소 domain stub을 검증합니다.

### 입력

현재 단계에는 matching input이 없습니다. 검증 대상은 다음 최소 주문 객체입니다.

```cpp
Order{
    .id = OrderId{1},
    .instrument_id = InstrumentId{1001},
    .side = Side::Buy,
    .type = OrderType::Limit,
    .time_in_force = TimeInForce::Day,
    .price = Price{73500},
    .quantity = Quantity{10},
    .sequence = SequenceNumber{1},
}
```

### 변경 내용

- CMake 기반 C++20 프로젝트 구성
- `domain` 타입과 `Order` stub 작성
- `mini_ats` 실행 파일 추가
- GoogleTest 기반 샘플 unit test 추가
- 문서와 DB/script placeholder 추가

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/mini_ats
```

### 출력

`cmake -S . -B build`

```text
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
```

`cmake --build build`

```text
[ 33%] Built target mini_ats_domain
[ 66%] Built target mini_ats
[100%] Built target mini_ats_unit_tests
```

`ctest --test-dir build`

```text
Start 1: OrderTest.LimitOrderUsesIntegerPriceAndQuantity
1/1 Test #1: OrderTest.LimitOrderUsesIntegerPriceAndQuantity ...   Passed
100% tests passed, 0 tests failed out of 1
```

`./build/mini_ats`

```text
Mini ATS Matching System bootstrap
```

### 결과 판단

성공입니다.

- CMake configure 성공
- C++20 build 성공
- GoogleTest test discovery 성공
- unit test 1개 통과
- application entry point 실행 성공

### 다음 단계

다음 단계에서는 domain model을 확장합니다.

- `Trade`
- `ExecutionReport`
- `OrderStatus`
- `RejectReason`
- `CancelRequest`
- 이후 `OrderBook`, `MatchingEngine` 순서로 구현

## 2026-06-08 2단계: domain model 확장

### 목표

OrderBook과 MatchingEngine을 구현하기 전에 주문 처리 결과를 표현할 domain model을 확장합니다.

이번 단계에서는 실제 matching logic을 구현하지 않습니다. 대신 이후 matching 결과로 반환할 `Trade`, `ExecutionReport`, `CancelRequest` 타입과 상태 enum을 준비합니다.

### 입력

검증에 사용한 입력은 다음 domain 객체입니다.

`Trade`

```cpp
Trade{
    .id = TradeId{1},
    .instrument_id = InstrumentId{1001},
    .resting_order_id = OrderId{10},
    .incoming_order_id = OrderId{20},
    .aggressor_side = Side::Buy,
    .price = Price{73500},
    .quantity = Quantity{3},
    .sequence = SequenceNumber{7},
}
```

`ExecutionReport`: 부분 체결

```cpp
ExecutionReport{
    .order_id = OrderId{20},
    .instrument_id = InstrumentId{1001},
    .type = ExecutionType::Trade,
    .status = OrderStatus::PartiallyFilled,
    .filled_quantity = Quantity{3},
    .remaining_quantity = Quantity{7},
    .last_price = Price{73500},
    .last_quantity = Quantity{3},
    .reject_reason = RejectReason::None,
    .sequence = SequenceNumber{8},
}
```

`ExecutionReport`: 거부

```cpp
ExecutionReport{
    .order_id = OrderId{21},
    .instrument_id = InstrumentId{1001},
    .type = ExecutionType::Rejected,
    .status = OrderStatus::Rejected,
    .filled_quantity = Quantity{0},
    .remaining_quantity = Quantity{0},
    .last_price = Price{0},
    .last_quantity = Quantity{0},
    .reject_reason = RejectReason::InvalidQuantity,
    .sequence = SequenceNumber{9},
}
```

`CancelRequest`

```cpp
CancelRequest{
    .order_id = OrderId{20},
    .instrument_id = InstrumentId{1001},
    .sequence = SequenceNumber{10},
}
```

### 변경 내용

- `TradeId`, `OrderStatus`, `ExecutionType`, `RejectReason` 추가
- `Trade` domain model 추가
- `ExecutionReport` domain model 추가
- `CancelRequest` domain model 추가
- 시장가 주문과 잘못된 지정가 주문 검증 test 추가
- domain model unit test 추가
- README와 architecture 문서에 현재 구현 모델 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/mini_ats
```

### 출력

`cmake -S . -B build`

```text
-- Configuring done (0.1s)
-- Generating done (0.1s)
-- Build files have been written to: .../build
```

`cmake --build build`

```text
[ 10%] Building CXX object CMakeFiles/mini_ats_domain.dir/src/domain/cancel_request.cpp.o
[ 20%] Building CXX object CMakeFiles/mini_ats_domain.dir/src/domain/execution_report.cpp.o
[ 30%] Building CXX object CMakeFiles/mini_ats_domain.dir/src/domain/order.cpp.o
[ 40%] Building CXX object CMakeFiles/mini_ats_domain.dir/src/domain/trade.cpp.o
[ 50%] Linking CXX static library libmini_ats_domain.a
[ 50%] Built target mini_ats_domain
[ 60%] Linking CXX executable mini_ats
[ 70%] Built target mini_ats
[ 80%] Building CXX object CMakeFiles/mini_ats_unit_tests.dir/tests/unit/domain_model_test.cpp.o
[ 90%] Building CXX object CMakeFiles/mini_ats_unit_tests.dir/tests/unit/order_test.cpp.o
[100%] Linking CXX executable mini_ats_unit_tests
[100%] Built target mini_ats_unit_tests
```

`ctest --test-dir build`

```text
1/7 Test #1: DomainModelTest.TradeCalculatesIntegerNotional ..............   Passed
2/7 Test #2: DomainModelTest.ExecutionReportRepresentsPartialFill ........   Passed
3/7 Test #3: DomainModelTest.ExecutionReportRepresentsRejectedOrder ......   Passed
4/7 Test #4: DomainModelTest.CancelRequestRequiresOrderAndInstrumentId ...   Passed
5/7 Test #5: OrderTest.LimitOrderUsesIntegerPriceAndQuantity .............   Passed
6/7 Test #6: OrderTest.MarketOrderDoesNotRequirePrice ....................   Passed
7/7 Test #7: OrderTest.LimitOrderRequiresPositivePriceAndQuantity ........   Passed

100% tests passed, 0 tests failed out of 7
```

`./build/mini_ats`

```text
Mini ATS Matching System bootstrap
```

### 결과 판단

성공입니다.

- CMake configure 성공
- domain source 4개 컴파일 성공
- unit test가 1개에서 7개로 증가
- 모든 테스트 통과
- 가격, 수량, 거래대금 계산이 정수 기반으로 유지됨

### 다음 단계

다음 단계에서는 `OrderBook`의 최소 구조를 구현합니다.

- 매수/매도 side 분리
- 가격 level 관리
- 같은 가격 안에서 FIFO 순서 유지
- best bid / best ask 조회
- 아직 matching은 하지 않고 주문장 적재와 조회부터 검증

## 2026-06-08 3단계: OrderBook 최소 구조 구현

### 목표

부분 체결 후 잔량이 `ExecutionReport`가 아니라 주문장 내부의 `BookOrder.remaining_quantity`로 유지되는 구조를 구현합니다.

이번 단계에서는 matching logic을 구현하지 않습니다. 대신 이후 MatchingEngine이 사용할 in-memory OrderBook을 준비합니다.

### 입력

가격 우선 검증 입력:

```cpp
Order{id=1, side=Buy,  price=73500, quantity=10, sequence=1}
Order{id=2, side=Buy,  price=73600, quantity=5,  sequence=2}
Order{id=3, side=Sell, price=73800, quantity=4,  sequence=3}
Order{id=4, side=Sell, price=73700, quantity=6,  sequence=4}
```

FIFO 검증 입력:

```cpp
Order{id=10, side=Buy, price=73500, quantity=10, sequence=1}
Order{id=11, side=Buy, price=73500, quantity=5,  sequence=2}
```

부분 체결 후 잔량 검증 입력:

```cpp
Order{id=20, side=Buy, price=73500, quantity=10, sequence=1}
reduce_order(order_id=20, executed_quantity=3)
```

취소 검증 입력:

```cpp
Order{id=40, side=Sell, price=74000, quantity=8, sequence=1}
CancelRequest{order_id=40, instrument_id=1001, sequence=3}
```

### 변경 내용

- `mini_ats_engine` CMake library 추가
- `BookOrder` 추가
- `OrderBook` 추가
- 매수/매도 side별 가격 level 관리 구현
- 매수는 높은 가격 우선, 매도는 낮은 가격 우선 조회 구현
- 같은 가격 level 안에서 FIFO 유지
- `reduce_order()`로 부분 체결 잔량 차감 구현
- `cancel_order()`로 resting order 제거 구현
- OrderBook unit test 7개 추가

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/mini_ats
```

### 출력

`cmake -S . -B build`

```text
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
```

`cmake --build build`

```text
[ 38%] Built target mini_ats_domain
[ 46%] Building CXX object CMakeFiles/mini_ats_engine.dir/src/engine/order_book.cpp.o
[ 53%] Linking CXX static library libmini_ats_engine.a
[ 53%] Built target mini_ats_engine
[ 61%] Linking CXX executable mini_ats
[ 69%] Built target mini_ats
[ 76%] Building CXX object CMakeFiles/mini_ats_unit_tests.dir/tests/unit/order_book_test.cpp.o
[ 84%] Linking CXX executable mini_ats_unit_tests
[100%] Built target mini_ats_unit_tests
```

`ctest --test-dir build`

```text
 8/14 Test  #8: OrderBookTest.AddsLimitOrdersAndFindsBestPrices .............   Passed
 9/14 Test  #9: OrderBookTest.KeepsFifoOrderWithinSamePriceLevel ............   Passed
10/14 Test #10: OrderBookTest.PartialReduceKeepsRemainingQuantityInBook .....   Passed
11/14 Test #11: OrderBookTest.FullReduceRemovesOrderAndEmptyPriceLevel ......   Passed
12/14 Test #12: OrderBookTest.CancelRequestRemovesRestingOrder ..............   Passed
13/14 Test #13: OrderBookTest.RejectsOrdersThatShouldNotRest ................   Passed
14/14 Test #14: OrderBookTest.RejectsDuplicateOrderIds ......................   Passed

100% tests passed, 0 tests failed out of 14
```

`./build/mini_ats`

```text
Mini ATS Matching System bootstrap
```

### 결과 판단

성공입니다.

- OrderBook에 limit DAY 주문만 resting order로 저장됨
- 매수 best price는 가장 높은 가격으로 결정됨
- 매도 best price는 가장 낮은 가격으로 결정됨
- 같은 가격에서는 먼저 들어온 주문이 먼저 조회됨
- 부분 체결 후 `BookOrder.remaining_quantity`가 10에서 7로 감소하고 주문장에 유지됨
- 전량 체결 또는 취소 시 주문과 빈 가격 level이 제거됨
- 전체 unit test 14개 통과

### 다음 단계

다음 단계에서는 `MatchingEngine`의 첫 버전을 구현합니다.

- 신규 limit order 접수
- 반대편 best price와 crossing 여부 판단
- 가격-시간 우선 체결
- `Trade` 생성
- `ExecutionReport` 생성
- 잔량이 있으면 OrderBook에 resting order로 저장

## 2026-06-08 4단계: OrderBook 상태 시각화

### 목표

OrderBook 내부 상태를 눈으로 확인할 수 있도록 snapshot과 텍스트 formatter를 추가합니다.

이 단계의 목적은 `ExecutionReport`와 `BookOrder`의 차이, 가격 level, FIFO 순서, 잔량 상태를 실행 결과로 이해할 수 있게 만드는 것입니다.

### 입력

데모 실행에 사용한 주문 입력:

```cpp
Order{id=1, side=Buy,  price=73500, quantity=10, sequence=1}
Order{id=2, side=Buy,  price=73600, quantity=5,  sequence=2}
Order{id=3, side=Sell, price=73800, quantity=4,  sequence=3}
Order{id=4, side=Sell, price=73700, quantity=6,  sequence=4}
Order{id=5, side=Buy,  price=73500, quantity=5,  sequence=5}
reduce_order(order_id=1, executed_quantity=3)
```

### 변경 내용

- `BookOrderSnapshot` 추가
- `PriceLevelSnapshot` 추가
- `OrderBookSnapshot` 추가
- `OrderBook::snapshot()` 추가
- `format_order_book()` 추가
- `./build/mini_ats` 실행 시 샘플 주문장 상태 출력
- snapshot/formatter unit test 추가
- OrderBook 상태 설명 문서 추가

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/mini_ats
```

### 출력

`ctest --test-dir build`

```text
15/16 Test #15: OrderBookTest.SnapshotShowsBookStateInPriorityOrder .........   Passed
16/16 Test #16: OrderBookTest.FormatsOrderBookSnapshotForHumans .............   Passed

100% tests passed, 0 tests failed out of 16
```

`./build/mini_ats`

```text
OrderBook instrument=1001
ASK best-first
  73700 | total=6 | #4(rem=6,seq=4)
  73800 | total=4 | #3(rem=4,seq=3)
----- spread -----
BID best-first
  73600 | total=5 | #2(rem=5,seq=2)
  73500 | total=12 | #1(rem=7,seq=1) -> #5(rem=5,seq=5)
```

### 결과 판단

성공입니다.

- OrderBook 상태를 snapshot으로 외부에 노출할 수 있음
- 매도는 낮은 가격부터, 매수는 높은 가격부터 best-first로 출력됨
- 같은 가격 level 안의 FIFO 순서가 `->`로 표현됨
- `order_id=1`의 부분 체결 후 잔량이 `rem=7`로 출력됨
- 전체 unit test 16개 통과

### 다음 단계

다음 단계에서는 `MatchingEngine` 첫 버전을 구현합니다.

- 신규 limit order가 들어오면 반대 side best price 확인
- crossing이면 `Trade` 생성
- 주문별 `ExecutionReport` 생성
- 잔량이 있으면 현재 구현한 `OrderBook`에 저장

## 2026-06-08 5단계: MatchingEngine 첫 버전 구현

### 목표

지정가 DAY 주문 기준으로 가격-시간 우선 체결을 수행하는 MatchingEngine을 구현합니다.

이번 단계에서는 시장가 주문, IOC/FOK, TCP/UDP, PostgreSQL 연동은 구현하지 않습니다.

### 입력

대표 데모 입력:

```cpp
Order{id=10, side=Sell, price=73700, quantity=3,  sequence=1}
Order{id=11, side=Sell, price=73800, quantity=4,  sequence=2}
Order{id=20, side=Buy,  price=73800, quantity=10, sequence=3}
```

주문 `#20`은 매수 가격이 `73800`이므로 매도 `#10` 가격 `73700`, 매도 `#11` 가격 `73800`과 crossing됩니다.

### 변경 내용

- `MatchingEngine` 추가
- `SubmitOrderResult` 추가
- 신규 지정가 DAY 주문 접수 구현
- 매수/매도 crossing 판단 구현
- resting order 가격 기준 `Trade` 생성
- resting/incoming 주문별 `ExecutionReport` 생성
- incoming 잔량이 남으면 `OrderBook`에 resting order로 저장
- 전량 체결된 주문 id도 중복 거부할 수 있도록 accepted order id 추적
- matching engine unit test 6개 추가
- 데모 실행을 실제 matching 흐름으로 변경

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/mini_ats
```

### 출력

`ctest --test-dir build`

```text
 5/22 Test  #5: MatchingEngineTest.NonCrossingLimitOrderRestsInBook ..........   Passed
 6/22 Test  #6: MatchingEngineTest.BuyOrderCrossesBestAskAtRestingPrice ......   Passed
 7/22 Test  #7: MatchingEngineTest.SellOrderCrossesBestBidAtRestingPrice .....   Passed
 8/22 Test  #8: MatchingEngineTest.MatchingUsesFifoWithinSamePriceLevel ......   Passed
 9/22 Test  #9: MatchingEngineTest.DuplicateOrderIdIsRejectedEvenAfterFill ...   Passed
10/22 Test #10: MatchingEngineTest.InvalidOrderIsRejected ....................   Passed

100% tests passed, 0 tests failed out of 22
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- 신규 매수 주문이 best ask와 crossing됨
- 체결 가격은 incoming 가격이 아니라 resting order 가격으로 결정됨
- 같은 가격 level에서는 FIFO 순서대로 체결됨
- incoming 주문 10주 중 7주가 체결되고 3주가 주문장에 남음
- `Trade`와 `ExecutionReport`가 생성됨
- 전체 unit test 22개 통과

### 다음 단계

다음 단계에서는 MatchingEngine 범위를 넓힙니다.

- 시장가 주문
- IOC 주문
- FOK 주문
- cancel request를 MatchingEngine API로 통합

## 2026-06-08 6단계: MatchingEngine 주문 조건 확장

### 목표

MatchingEngine이 지정가 DAY 주문뿐 아니라 시장가, IOC, FOK, cancel request를 처리하도록 확장합니다.

이번 단계에서도 TCP/UDP, PostgreSQL 연동은 구현하지 않습니다. matching core 내부의 결정적 처리 범위만 넓힙니다.

### 입력

시장가 주문 검증 입력:

```cpp
Order{id=70, side=Sell, type=Limit,  tif=Day, price=73700, quantity=2, sequence=1}
Order{id=71, side=Sell, type=Limit,  tif=Day, price=73800, quantity=2, sequence=2}
Order{id=72, side=Buy,  type=Market, tif=IOC, price=0,     quantity=5, sequence=3}
```

IOC 주문 검증 입력:

```cpp
Order{id=80, side=Sell, type=Limit, tif=Day, price=73700, quantity=3, sequence=1}
Order{id=81, side=Buy,  type=Limit, tif=IOC, price=73800, quantity=5, sequence=2}
```

FOK 주문 검증 입력:

```cpp
Order{id=100, side=Sell, type=Limit, tif=Day, price=73700, quantity=3, sequence=1}
Order{id=101, side=Sell, type=Limit, tif=Day, price=73900, quantity=3, sequence=2}
Order{id=102, side=Buy,  type=Limit, tif=FOK, price=73800, quantity=5, sequence=3}
```

취소 검증 입력:

```cpp
Order{id=120, side=Buy, type=Limit, tif=Day, price=73600, quantity=5, sequence=1}
CancelRequest{order_id=120, instrument_id=1001, sequence=2}
```

### 변경 내용

- `CancelOrderResult` 추가
- `MatchingEngine::cancel_order()` 추가
- 시장가 주문이 가격 조건 없이 반대 side best-first로 체결되도록 구현
- IOC 주문이 즉시 체결 가능한 수량만 체결하고 잔량은 주문장에 남기지 않도록 구현
- FOK 주문이 사전 유동성 확인 후 전체 체결이 불가능하면 주문장을 변경하지 않고 거부하도록 구현
- cancel request가 resting order를 제거하고 canceled execution report를 생성하도록 구현
- cancel 이후 같은 order id 재사용을 duplicate로 거부하도록 accepted order id 유지
- matching engine unit test 7개 추가
- README, architecture, matching-rules 문서에 현재 지원 범위 반영

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats
```

### 출력

`cmake --build build`

```text
[ 46%] Linking CXX static library libmini_ats_engine.a
[ 66%] Built target mini_ats
[100%] Built target mini_ats_unit_tests
```

`ctest --test-dir build --output-on-failure`

```text
11/29 Test #11: MatchingEngineTest.MarketOrderSweepsAvailableLiquidityAndCancelsRemainder ...............   Passed
12/29 Test #12: MatchingEngineTest.IocLimitOrderPartiallyFillsAndDoesNotRest ............................   Passed
13/29 Test #13: MatchingEngineTest.IocLimitOrderCancelsWhenItDoesNotCross ...............................   Passed
14/29 Test #14: MatchingEngineTest.FokLimitOrderRejectsWithoutMutatingBookWhenLiquidityIsInsufficient ...   Passed
15/29 Test #15: MatchingEngineTest.FokLimitOrderFullyExecutesWhenLiquidityIsAvailable ...................   Passed
16/29 Test #16: MatchingEngineTest.CancelOrderRemovesRestingOrderAndReportsCanceled .....................   Passed
17/29 Test #17: MatchingEngineTest.CancelOrderRejectsMissingOrder .......................................   Passed

100% tests passed, 0 tests failed out of 29
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- 시장가 주문은 반대 side 유동성을 best-first로 소진하고 미체결 잔량을 주문장에 남기지 않음
- IOC 지정가 주문은 체결 가능한 수량만 체결하고 잔량을 canceled report로 종료함
- FOK 지정가 주문은 전체 수량을 즉시 체결할 수 없으면 기존 주문장을 변경하지 않음
- FOK 지정가 주문은 충분한 유동성이 있으면 여러 가격 level을 거쳐 전량 체결됨
- cancel request는 resting order를 제거하고 canceled execution report를 생성함
- 전체 unit test 29개 통과

### 다음 단계

다음 단계에서는 기준정보와 운영 경계를 준비합니다.

- 종목별 tick size, 가격 제한폭, 시장 세션 기준정보 모델링
- PostgreSQL 기준정보를 matching core에 주입하는 adapter 설계
- TCP 주문 접수 프로토콜 초안 구체화
- UDP 체결/호가 이벤트 분배 모델 구체화

## 2026-06-08 7단계: 기준정보 검증 모델 추가

### 목표

종목별 tick size, 가격 제한폭, 시장 세션을 matching core에 주입할 수 있는 기준정보 모델을 추가합니다.

이번 단계에서는 PostgreSQL에 직접 접속하는 adapter를 구현하지 않습니다. 대신 DB에서 읽어온 값이 core에 들어왔을 때 어떤 구조와 규칙으로 검증되는지 먼저 고정합니다.

### 입력

기준정보 검증 입력:

```cpp
InstrumentReference{
    .id = InstrumentId{1001},
    .tick_size = Price{5},
    .lower_price_limit = Price{70000},
    .upper_price_limit = Price{80000},
    .session = MarketSession::Open,
    .version = SequenceNumber{1},
}
```

거부 주문 예시:

```cpp
Order{id=70, side=Buy, type=Limit, tif=Day, price=73502, quantity=10, sequence=1}
Order{id=71, side=Buy, type=Limit, tif=Day, price=69000, quantity=10, sequence=1}
```

시장 세션 닫힘 예시:

```cpp
InstrumentReference{..., session=MarketSession::Closed}
Order{id=72, side=Buy, type=Limit, tif=Day, price=73500, quantity=10, sequence=1}
```

### 변경 내용

- `MarketSession` enum 추가
- `RejectReason::MarketClosed` 추가
- `InstrumentReference` domain model 추가
- `default_instrument_reference()` 추가
- MatchingEngine에 `InstrumentReference` 생성자 추가
- MatchingEngine 주문 검증에 시장 세션, tick size, 가격 제한폭 확인 추가
- PostgreSQL `mini_ats.instruments` 기준정보 테이블 초안 추가
- seed 데이터로 demo 종목 `1001` 추가
- domain model unit test 2개 추가
- matching engine 기준정보 unit test 4개 추가
- README, architecture, matching-rules, recovery-design 문서 갱신

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
 5/35 Test  #5: DomainModelTest.InstrumentReferenceValidatesTickSizePriceLimitsAndSession ...............   Passed
 6/35 Test  #6: DomainModelTest.DefaultInstrumentReferenceKeepsLegacySingleInstrumentOpen ...............   Passed
13/35 Test #13: MatchingEngineTest.ReferenceDataRejectsOffTickLimitPrice ................................   Passed
14/35 Test #14: MatchingEngineTest.ReferenceDataRejectsPriceOutsideLimitBand ............................   Passed
15/35 Test #15: MatchingEngineTest.ReferenceDataRejectsOrdersWhenMarketIsClosed .........................   Passed
16/35 Test #16: MatchingEngineTest.ReferenceDataAllowsValidLimitOrder ...................................   Passed

100% tests passed, 0 tests failed out of 35
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- 지정가 가격이 tick size 배수가 아니면 `InvalidPrice`로 거부됨
- 지정가 가격이 가격 제한폭 밖이면 `InvalidPrice`로 거부됨
- 시장 세션이 닫혀 있으면 `MarketClosed`로 거부됨
- 기준정보 검증을 통과한 지정가 주문은 기존 MatchingEngine 흐름으로 접수됨
- 기존 단일 종목 테스트는 기본 기준정보로 계속 동작함
- 전체 unit test 35개 통과

### 다음 단계

다음 단계에서는 PostgreSQL adapter 또는 gateway 경계를 선택해 구현합니다.

- PostgreSQL에서 `mini_ats.instruments`를 읽어 `InstrumentReference`로 변환
- 기준정보 version을 주문 입력 로그와 함께 기록하는 replay 입력 구조 설계
- TCP 주문 접수 프로토콜 초안 구체화

## 2026-06-08 8단계: PostgreSQL 기준정보 row adapter 경계 추가

### 목표

PostgreSQL `mini_ats.instruments` row를 matching core가 사용하는 `InstrumentReference`로 변환하는 adapter 경계를 추가합니다.

현재 개발 환경에는 `psql`/`pg_config`가 없으므로 C++ 빌드에 PostgreSQL client library 의존성을 강제하지 않습니다. 이번 단계에서는 DB connection이 아니라 row mapping, SQL query, script entry point를 먼저 고정합니다.

### 입력

PostgreSQL row 형태:

```cpp
InstrumentRecord{
    .instrument_id = InstrumentId{1001},
    .symbol = "DEMO",
    .tick_size = Price{5},
    .lower_price_limit = Price{70000},
    .upper_price_limit = Price{80000},
    .session = "OPEN",
    .reference_version = SequenceNumber{7},
}
```

변환 결과:

```cpp
InstrumentReference{
    .id = InstrumentId{1001},
    .tick_size = Price{5},
    .lower_price_limit = Price{70000},
    .upper_price_limit = Price{80000},
    .session = MarketSession::Open,
    .version = SequenceNumber{7},
}
```

### 변경 내용

- `mini_ats_reference_data` CMake library 추가
- `InstrumentRecord` 추가
- `InstrumentLoadResult` 추가
- `InstrumentLoadError` 추가
- `parse_market_session()` 추가
- `map_instrument_record()` 추가
- `instrument_reference_query()` 추가
- reference data unit test 4개 추가
- PostgreSQL setup/reset script를 실제 `psql` 호출 구조로 변경
- README와 architecture 문서에 `reference_data` 모듈 범위 반영

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
bash -n scripts/setup_postgres.sh scripts/reset_db.sh
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
36/39 Test #36: ReferenceDataTest.ParsesMarketSessionFromPostgresValue ............   Passed
37/39 Test #37: ReferenceDataTest.MapsInstrumentRecordToInstrumentReference .......   Passed
38/39 Test #38: ReferenceDataTest.RejectsInvalidInstrumentRecordFields ............   Passed
39/39 Test #39: ReferenceDataTest.ExposesParameterizedInstrumentReferenceQuery ....   Passed

100% tests passed, 0 tests failed out of 39
```

`bash -n scripts/setup_postgres.sh scripts/reset_db.sh`

```text
(no output)
```

### 결과 판단

성공입니다.

- DB row 형태의 `InstrumentRecord`를 `InstrumentReference`로 변환할 수 있음
- PostgreSQL session 문자열 `OPEN`, `CLOSED`를 domain enum으로 변환함
- 잘못된 tick size, 가격 제한폭, session, 기준정보 version을 adapter 경계에서 거부함
- SQL query는 `mini_ats.instruments`와 parameter `$1` 기반으로 고정됨
- 전체 unit test 39개 통과
- 현재 환경에 `psql`이 없어 DB script는 실제 실행하지 않고 문법만 검증함

### 다음 단계

다음 단계에서는 실제 외부 I/O 경계를 더 구체화합니다.

- `psql`/libpq 설치 후 PostgreSQL connection adapter 구현
- 주문 입력 로그와 기준정보 version을 함께 담는 replay input model 구현
- TCP 주문 접수 메시지 parser 구현

## 2026-06-08 9단계: Replay input model 추가

### 목표

주문 입력 로그를 deterministic matching core에 다시 적용할 수 있도록 replay input model을 추가합니다.

이번 단계에서는 파일/DB에서 로그를 읽는 I/O는 구현하지 않습니다. 대신 한 줄의 replay 입력이 어떤 metadata와 command를 가져야 하는지, replay 전에 어떤 조건을 검증해야 하는지, MatchingEngine에 어떻게 적용되는지를 고정합니다.

### 입력

신규 주문 replay event:

```cpp
ReplayEvent{
    .input_sequence = SequenceNumber{1},
    .reference_version = SequenceNumber{7},
    .command = Order{id=40, side=Sell, price=73700, quantity=3, sequence=1},
}
```

취소 replay event:

```cpp
ReplayEvent{
    .input_sequence = SequenceNumber{2},
    .reference_version = SequenceNumber{7},
    .command = CancelRequest{order_id=40, instrument_id=1001, sequence=2},
}
```

### 변경 내용

- `mini_ats_replay` CMake library 추가
- `ReplayCommandType` 추가
- `ReplayEventStatus` 추가
- `ReplayEvent` 추가
- `ReplayApplyResult` 추가
- `ReplayRunResult` 추가
- `apply_replay_event()` 추가
- `replay_events()` 추가
- replay unit test 5개 추가
- README, architecture, recovery-design 문서에 replay input model 반영

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
40/44 Test #40: ReplayTest.ReplayEventStoresSubmitOrderMetadata .........................   Passed
41/44 Test #41: ReplayTest.AppliesSubmitAndCancelEventsInOrder ..........................   Passed
42/44 Test #42: ReplayTest.RejectsReferenceVersionMismatchWithoutMutatingBook ...........   Passed
43/44 Test #43: ReplayTest.RejectsCommandSequenceMismatchWithoutMutatingBook ............   Passed
44/44 Test #44: ReplayTest.ReplaysSameInputStreamDeterministically ......................   Passed

100% tests passed, 0 tests failed out of 44
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- replay event가 input sequence와 command 내부 sequence를 함께 기록함
- replay event가 기준정보 version을 함께 기록함
- 기준정보 version mismatch는 matching state를 변경하지 않고 거부됨
- command sequence mismatch도 matching state를 변경하지 않고 거부됨
- 신규 주문과 취소 요청을 같은 replay stream에서 순서대로 적용할 수 있음
- 같은 input stream을 같은 기준정보로 replay하면 같은 trade와 order book 상태가 생성됨
- 전체 unit test 44개 통과

### 다음 단계

다음 단계에서는 replay input model 바깥의 입출력 형식을 구현합니다.

- 사람이 읽기 쉬운 text command parser 구현
- TCP 주문 접수 메시지 parser와 replay event 변환
- replay log 파일 reader/writer 초안 구현

## 2026-06-08 10단계: Text command parser 추가

### 목표

사람이 읽기 쉬운 text command를 `ReplayEvent`로 변환하는 parser를 추가합니다.

이번 단계에서는 TCP 서버나 파일 reader를 구현하지 않습니다. 대신 이후 TCP 주문 접수와 replay log reader가 공통으로 사용할 text command format을 먼저 고정합니다.

### 입력

신규 주문 command:

```text
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
```

시장가 주문 command:

```text
SUBMIT input_sequence=2 reference_version=7 id=11 instrument=1001 side=SELL order_type=MARKET time_in_force=IOC qty=5
```

취소 command:

```text
CANCEL seq=3 ref=7 order_id=10 instrument_id=1001
```

### 변경 내용

- `mini_ats_protocol` CMake library 추가
- `TextCommandParseError` 추가
- `TextCommandParseResult` 추가
- `parse_text_command()` 추가
- `SUBMIT` command parsing 구현
- `CANCEL` command parsing 구현
- `side`, `type`, `tif` enum parsing 구현
- field alias 지원
- text command parser unit test 6개 추가
- README, architecture, protocol 문서에 text command parser 반영

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
45/50 Test #45: TextCommandParserTest.ParsesLimitSubmitCommand .................   Passed
46/50 Test #46: TextCommandParserTest.ParsesMarketSubmitCommandWithAliases .....   Passed
47/50 Test #47: TextCommandParserTest.ParsesCancelCommand ......................   Passed
48/50 Test #48: TextCommandParserTest.ReportsMalformedAndUnknownInput ..........   Passed
49/50 Test #49: TextCommandParserTest.ReportsMissingAndInvalidFields ...........   Passed
50/50 Test #50: TextCommandParserTest.ParsedCommandsCanDriveReplay .............   Passed

100% tests passed, 0 tests failed out of 50
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- `SUBMIT` text command를 신규 주문 `ReplayEvent`로 변환함
- `CANCEL` text command를 취소 `ReplayEvent`로 변환함
- limit/market, DAY/IOC/FOK, BUY/SELL 값을 domain enum으로 변환함
- malformed token, unknown command, missing field, invalid enum, invalid number를 parser 경계에서 식별함
- parser 결과를 replay stream에 넣어 실제 MatchingEngine 체결까지 연결할 수 있음
- 전체 unit test 50개 통과

### 다음 단계

다음 단계에서는 text command를 외부 입력과 연결합니다.

- replay log file reader/writer 초안 구현
- TCP 주문 접수 server skeleton 구현
- parser error를 주문 접수 reject response로 변환하는 gateway model 구현

## 2026-06-08 11단계: Replay log file reader/writer 추가

### 목표

Text command 여러 줄을 stream/file에서 읽어 `ReplayEvent` 목록으로 변환하고, `ReplayEvent` 목록을 canonical text command로 다시 기록하는 replay log I/O 경계를 추가합니다.

이번 단계에서는 네트워크 I/O는 구현하지 않습니다. 파일과 stream 기반 replay log를 먼저 고정해서 이후 TCP 수신 로그와 복구 replay가 같은 format을 공유할 수 있게 합니다.

### 입력

Replay log 예시:

```text
# demo replay log
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3

SUBMIT seq=2 ref=7 order_id=11 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=5
CANCEL seq=3 ref=7 order_id=11 instrument_id=1001
```

### 변경 내용

- `ReplayLogIoError` 추가
- `ReplayLogParseFailure` 추가
- `ReplayLogReadResult` 추가
- `format_replay_event()` 추가
- `read_replay_log()` 추가
- `read_replay_log_file()` 추가
- `write_replay_log()` 추가
- `write_replay_log_file()` 추가
- `mini_ats_replay_io` CMake library 추가
- replay log I/O unit test 6개 추가
- README, architecture, protocol, recovery-design 문서에 replay log I/O 반영

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
40/56 Test #40: ReplayLogIoTest.ReadsTextLogFromStreamSkippingBlankLinesAndComments ...   Passed
41/56 Test #41: ReplayLogIoTest.ReportsParseFailureWithLineNumberAndOriginalLine .......   Passed
42/56 Test #42: ReplayLogIoTest.FormatsReplayEventsAsCanonicalTextCommands .............   Passed
43/56 Test #43: ReplayLogIoTest.WritesAndReadsReplayLogFromStream ......................   Passed
44/56 Test #44: ReplayLogIoTest.WritesAndReadsReplayLogFile ............................   Passed
45/56 Test #45: ReplayLogIoTest.ReadEventsCanDriveReplay ...............................   Passed

100% tests passed, 0 tests failed out of 56
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- replay log reader가 빈 줄과 `#` 주석을 건너뜀
- replay log reader가 text command를 `ReplayEvent` 목록으로 변환함
- parse 실패 시 line number, parser error, field, 원문 line을 반환함
- replay event를 canonical text command로 format할 수 있음
- stream과 file path 모두 읽고 쓸 수 있음
- 읽어온 replay events를 MatchingEngine에 적용해 실제 체결까지 연결할 수 있음
- 전체 unit test 56개 통과

### 다음 단계

다음 단계에서는 주문 접수 gateway 쪽 model을 준비합니다.

- parser/replay validation error를 주문 접수 response로 변환
- TCP server skeleton 구현 전 `GatewayRequest`/`GatewayResponse` model 추가
- replay log writer를 주문 접수 accepted input stream과 연결

## 2026-06-08 12단계: Gateway request/response model 추가

### 목표

TCP server skeleton을 붙이기 전에 주문 접수 경계에서 사용할 request/response model을 먼저 추가합니다.

이번 단계에서는 socket I/O를 구현하지 않습니다. 대신 text command 한 줄이 들어왔을 때 parser error, replay validation error, matching engine reject를 어떤 gateway response로 돌려줄지 고정합니다.

### 입력

정상 신규 주문 command:

```text
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
```

정상 취소 command:

```text
CANCEL seq=2 ref=7 order_id=10 instrument_id=1001
```

Parser reject 예시:

```text
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY quantity=10
```

Replay validation reject 예시:

```text
SUBMIT seq=1 ref=6 order_id=20 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
```

Engine reject 예시:

```text
SUBMIT seq=1 ref=7 order_id=30 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73502 quantity=10
```

### 변경 내용

- `mini_ats_gateway` CMake library 추가
- `GatewayRequest` 추가
- `GatewayResponse` 추가
- `GatewayResponseStatus` 추가
- `GatewayRejectReason` 추가
- parser/replay/engine 상태를 text reason으로 변환하는 `to_text()` 추가
- `make_parse_reject_response()` 추가
- `make_replay_reject_response()` 추가
- `make_engine_response()` 추가
- `handle_text_command()` 추가
- `handle_gateway_request()` 추가
- gateway unit test 7개 추가
- README, architecture, protocol, recovery-design 문서에 gateway request/response model 반영

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
7/63 Test  #7: GatewayTest.AcceptsValidSubmitCommandAndReturnsEngineReports .......   Passed
8/63 Test  #8: GatewayTest.RejectsMalformedTextCommandBeforeMutatingEngine ........   Passed
9/63 Test  #9: GatewayTest.RejectsReplayValidationErrorBeforeMutatingEngine .......   Passed
10/63 Test #10: GatewayTest.ConvertsEngineRejectToGatewayRejectResponse ...........   Passed
11/63 Test #11: GatewayTest.AcceptsCancelCommandAfterRestingOrder .................   Passed
12/63 Test #12: GatewayTest.HandlesGatewayRequestWrapper ..........................   Passed
13/63 Test #13: GatewayTest.ConvertsEmptyLineToParseRejectText ....................   Passed

100% tests passed, 0 tests failed out of 63
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- 정상 `SUBMIT` command를 `GatewayResponseStatus::Accepted`로 변환함
- 정상 `CANCEL` command를 `GatewayResponseStatus::Accepted`로 변환함
- parser 실패를 `GatewayRejectReason::ParseError`로 변환하고 matching state를 변경하지 않음
- replay validation 실패를 `GatewayRejectReason::ReplayValidationError`로 변환하고 matching state를 변경하지 않음
- matching engine의 domain reject report를 `GatewayRejectReason::EngineRejected`로 변환함
- 응답에 command type, input sequence, trades, execution reports를 함께 담음
- 전체 unit test 63개 통과

### 다음 단계

다음 단계에서는 gateway model을 외부 I/O와 연결합니다.

- gateway response text formatter 구현
- TCP server skeleton 추가
- accepted input stream을 replay log writer와 연결

## 2026-06-08 13단계: Gateway response text formatter 추가

### 목표

`GatewayResponse`를 TCP client나 운영 로그에 그대로 내보낼 수 있는 한 줄짜리 canonical text response로 변환합니다.

이번 단계에서도 socket I/O는 구현하지 않습니다. 대신 네트워크 adapter가 붙기 전에 response serialization 계약을 먼저 고정합니다.

### 입력

정상 접수 응답 예시:

```text
ACCEPTED reason=NONE seq=1 command=SUBMIT detail=ACCEPTED trades=0 reports=1 report0_order_id=10 report0_instrument_id=1001 report0_type=ACCEPTED report0_status=ACCEPTED report0_filled_quantity=0 report0_remaining_quantity=10 report0_last_price=0 report0_last_quantity=0 report0_reject_reason=NONE report0_sequence=1
```

Parser reject 응답 예시:

```text
REJECTED reason=PARSE_ERROR seq=0 command=NONE detail=MISSING_FIELD:price trades=0 reports=0
```

Trade 포함 응답 예시:

```text
ACCEPTED reason=NONE seq=2 command=SUBMIT detail=ACCEPTED trades=1 reports=2 trade0_id=1 trade0_instrument_id=1001 trade0_resting_order_id=60 trade0_incoming_order_id=61 trade0_aggressor_side=BUY trade0_price=73700 trade0_quantity=3 trade0_sequence=2 report0_order_id=60 report0_instrument_id=1001 report0_type=TRADE report0_status=FILLED report0_filled_quantity=3 report0_remaining_quantity=0 report0_last_price=73700 report0_last_quantity=3 report0_reject_reason=NONE report0_sequence=3 report1_order_id=61 report1_instrument_id=1001 report1_type=TRADE report1_status=FILLED report1_filled_quantity=3 report1_remaining_quantity=0 report1_last_price=73700 report1_last_quantity=3 report1_reject_reason=NONE report1_sequence=4
```

### 변경 내용

- `format_gateway_response()` 추가
- `ReplayCommandType` text 변환 추가
- gateway response의 status, reject reason, sequence, command type, detail, trade/report count 출력
- trade detail 출력 추가
- execution report detail 출력 추가
- domain enum을 gateway response text로 변환하는 내부 helper 추가
- gateway formatter unit test 3개 추가
- README, architecture, protocol, recovery-design 문서에 response text formatter 반영

### 실행 명령

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
14/66 Test #14: GatewayTest.FormatsAcceptedGatewayResponseAsCanonicalText ........   Passed
15/66 Test #15: GatewayTest.FormatsParseRejectGatewayResponseAsCanonicalText .....   Passed
16/66 Test #16: GatewayTest.FormatsTradeResponseWithTradeAndReportDetails ........   Passed

100% tests passed, 0 tests failed out of 66
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- `GatewayResponse`를 deterministic 한 줄 text로 변환함
- 정상 접수, parser reject, trade 발생 응답을 모두 format할 수 있음
- trade와 execution report 상세 필드를 response text에 포함함
- command type이 없는 parser reject는 `command=NONE`, `seq=0`으로 표현함
- 전체 unit test 66개 통과

### 다음 단계

다음 단계에서는 gateway model과 replay log I/O를 연결합니다.

- accepted input만 replay log writer로 흘리는 recorder model 추가
- parser/replay reject는 state와 input log를 변경하지 않는지 검증
- 이후 TCP server skeleton이 recorder와 response formatter를 같이 사용하도록 준비

## 2026-06-08 14단계: Gateway accepted input recorder와 stdin runner 연결

### 목표

TCP server skeleton을 붙이기 전에 gateway model을 실제 입력/출력 경계와 한 번 연결합니다.

이번 단계에서는 socket I/O를 구현하지 않습니다. 대신 stdin으로 들어온 text command를 gateway에 적용하고, accepted command만 replay log stream에 남기는 recorder model을 추가합니다.

### 입력

Accepted 신규 주문:

```text
SUBMIT seq=1 ref=7 order_id=100 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
```

Engine reject 주문:

```text
SUBMIT seq=2 ref=7 order_id=101 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73502 quantity=1
```

Accepted 취소:

```text
CANCEL seq=3 ref=7 order_id=100 instrument_id=1001
```

### 변경 내용

- `gateway_recorder` module 추가
- `RecordedGatewayCommandResult` 추가
- `handle_recorded_text_command()` 추가
- accepted gateway response만 canonical replay log line으로 기록
- parser reject, replay validation reject, engine reject는 replay log에 기록하지 않도록 test 추가
- 기록된 accepted input log를 다시 replay해 같은 order book 상태를 만들 수 있는지 test 추가
- `mini_ats --gateway` stdin runner 추가
- `mini_ats --gateway --record-log <path>` accepted input append 옵션 추가
- README, architecture, protocol, recovery-design 문서에 recorder와 runner 사용법 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats --help
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=100 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  'SUBMIT seq=2 ref=7 order_id=101 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73502 quantity=1' \
  'CANCEL seq=3 ref=7 order_id=100 instrument_id=1001' \
  | ./build/mini_ats --gateway --record-log /tmp/mini_ats_gateway_record_test.S8wwDQ
sed -n '1,80p' /tmp/mini_ats_gateway_record_test.S8wwDQ
./build/mini_ats
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
17/69 Test #17: GatewayTest.RecordsAcceptedCommandsAsCanonicalReplayLogLines ...   Passed
18/69 Test #18: GatewayTest.DoesNotRecordRejectedCommands ......................   Passed
19/69 Test #19: GatewayTest.RecordedAcceptedLogCanReplaySameState ..............   Passed

100% tests passed, 0 tests failed out of 69
```

`./build/mini_ats --help`

```text
Usage:
  ./build/mini_ats              Run deterministic matching demo
  ./build/mini_ats --gateway    Read text commands from stdin
  ./build/mini_ats --gateway --record-log <path>
                         Append accepted input commands to replay log
  ./build/mini_ats --help       Show this help
```

`./build/mini_ats --gateway --record-log ...`

```text
ACCEPTED reason=NONE seq=1 command=SUBMIT detail=ACCEPTED trades=0 reports=1 ...
REJECTED reason=ENGINE_REJECTED seq=2 command=SUBMIT detail=ENGINE_REJECTED trades=0 reports=1 ...
ACCEPTED reason=NONE seq=3 command=CANCEL detail=ACCEPTED trades=0 reports=1 ...
```

기록된 replay log:

```text
SUBMIT seq=1 ref=7 order_id=100 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
CANCEL seq=3 ref=7 order_id=100 instrument_id=1001
```

`./build/mini_ats`

```text
Mini ATS Matching System demo
Input
  resting sell #10 price=73700 qty=3
  resting sell #11 price=73800 qty=4
  incoming buy #20 price=73800 qty=10

Trades
  trade#1 resting#10 incoming#20 price=73700 qty=3
  trade#2 resting#11 incoming#20 price=73800 qty=4

Final book
OrderBook instrument=1001
ASK best-first
  (empty)
----- spread -----
BID best-first
  73800 | total=3 | #20(rem=3,seq=3)
```

### 결과 판단

성공입니다.

- accepted `SUBMIT`과 `CANCEL`만 replay log에 기록됨
- off-tick engine reject는 gateway response로는 반환되지만 accepted input log에는 남지 않음
- 기록된 accepted input log를 다시 읽고 replay할 수 있음
- `mini_ats --gateway`가 canonical gateway response를 stdout으로 출력함
- `mini_ats --gateway --record-log <path>`가 accepted input을 append함
- 전체 unit test 69개 통과

### 다음 단계

다음 단계에서는 TCP 주문 접수 skeleton을 추가합니다.

- line-delimited text command framing
- gateway recorder와 response formatter를 TCP client adapter에 연결
- TCP adapter 수준의 integration test 또는 loopback smoke test 추가

## 2026-06-08 15단계: TCP 주문 접수 skeleton 추가

### 목표

stdin runner로 검증한 gateway recorder/formatter 경계를 localhost TCP adapter에 연결합니다.

이번 단계에서는 production-grade TCP server를 구현하지 않습니다. 대신 line-delimited text command를 읽고, 같은 `GatewayResponse` canonical text를 한 줄씩 돌려주는 skeleton을 구현합니다.

### 입력

TCP smoke test 입력:

```text
SUBMIT seq=1 ref=7 order_id=200 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3
SUBMIT seq=2 ref=7 order_id=201 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73702 quantity=1
SUBMIT seq=3 ref=7 order_id=202 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=5
```

### 변경 내용

- `TcpOrderServerStatus` 추가
- `TcpOrderServerRunResult` 추가
- `TcpOrderServer` 추가
- IPv4 `127.0.0.1` bind/listen/accept 처리 추가
- TCP client에서 받은 newline-delimited command framing 처리
- command마다 canonical gateway response 한 줄 전송
- accepted command만 replay log stream에 기록
- `mini_ats --tcp --port <port>` CLI 추가
- `mini_ats --tcp --port <port> --record-log <path>` CLI 추가
- TCP loopback smoke test 추가
- socket 사용이 제한된 sandbox에서는 loopback smoke test를 skip하도록 처리
- README, architecture, protocol, recovery-design 문서에 TCP skeleton 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_ats --help
./build/mini_ats_unit_tests --gtest_filter=TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback
```

### 출력

`ctest --test-dir build --output-on-failure`

```text
64/71 Test #64: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback ...***Skipped
65/71 Test #65: TcpOrderServerTest.ReportsInvalidBindAddress ....................   Passed

100% tests passed, 0 tests failed out of 71
```

기본 sandbox에서는 loopback socket 생성이 제한되어 TCP smoke가 skip됐습니다. 동일 테스트를 sandbox 밖에서 실행했을 때는 통과했습니다.

```text
[ RUN      ] TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback
[       OK ] TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback (0 ms)
[  PASSED  ] 1 test.
```

`./build/mini_ats --help`

```text
Usage:
  ./build/mini_ats              Run deterministic matching demo
  ./build/mini_ats --gateway    Read text commands from stdin
  ./build/mini_ats --gateway --record-log <path>
                         Append accepted input commands to replay log
  ./build/mini_ats --tcp --port <port> [--record-log <path>]
                         Serve line-delimited text commands over TCP
  ./build/mini_ats --help       Show this help
```

### 결과 판단

성공입니다.

- TCP skeleton이 line-delimited command를 처리함
- response formatter를 통해 client에 canonical response 한 줄을 반환함
- TCP 경로에서도 accepted input만 replay log에 기록함
- engine reject는 response로 반환되지만 replay log에는 기록하지 않음
- invalid bind address를 명시적인 status로 반환함
- 전체 unit test 71개 기준 70개 통과, 1개 sandbox skip
- loopback smoke test는 sandbox 밖에서 통과

### 다음 단계

다음 단계에서는 market data event model을 추가합니다.

- 체결 이벤트와 order book update 이벤트 모델 정의
- MatchingEngine 결과를 market data event로 변환하는 adapter 추가
- 이후 UDP publisher skeleton이 이 event model을 사용하도록 준비

## 2026-06-08 16단계: Market data event model 추가

### 목표

UDP publisher skeleton을 붙이기 전에 matching result를 외부 분배용 market data event로 변환하는 순수 model과 adapter를 추가합니다.

이번 단계에서는 UDP socket I/O를 구현하지 않습니다. 대신 `TradeEvent`와 `BookUpdateEvent`를 정의하고, `SubmitOrderResult`/`CancelOrderResult`와 최종 `OrderBookSnapshot`에서 이벤트 목록을 생성합니다.

### 입력

Resting bid 입력:

```text
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
```

Trade + residual bid 입력:

```text
SUBMIT seq=1 ref=7 order_id=20 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3
SUBMIT seq=2 ref=7 order_id=21 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=5
```

Reject / no-fill IOC 입력:

```text
SUBMIT seq=1 ref=7 order_id=30 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73502 quantity=10
SUBMIT seq=2 ref=7 order_id=31 instrument_id=1001 side=BUY type=LIMIT tif=IOC price=73500 quantity=10
```

Cancel 입력:

```text
CANCEL seq=2 ref=7 order_id=40 instrument_id=1001
```

### 변경 내용

- `mini_ats_marketdata` CMake library 추가
- `MarketDataEventType` 추가
- `MarketDataLevel` 추가
- `TradeEvent` 추가
- `BookUpdateEvent` 추가
- `MarketDataEvent` variant 추가
- `to_trade_event()` 추가
- `to_book_update_event()` 추가
- `market_data_events_for(SubmitOrderResult, OrderBookSnapshot)` 추가
- `market_data_events_for(CancelOrderResult, OrderBookSnapshot)` 추가
- resting limit order가 top-of-book update를 만드는지 test 추가
- trade result가 `TradeEvent`와 최종 `BookUpdateEvent`를 만드는지 test 추가
- reject와 no-fill IOC가 market data event를 만들지 않는지 test 추가
- cancel result가 book update를 만드는지 test 추가
- README, architecture, protocol, recovery-design 문서에 market data event model 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
./build/mini_ats_unit_tests --gtest_filter=MarketDataTest.*
ctest --test-dir build --output-on-failure
```

### 출력

`./build/mini_ats_unit_tests --gtest_filter=MarketDataTest.*`

```text
[ RUN      ] MarketDataTest.LimitOrderRestingCreatesTopOfBookUpdate
[       OK ] MarketDataTest.LimitOrderRestingCreatesTopOfBookUpdate (0 ms)
[ RUN      ] MarketDataTest.TradeCreatesTradeEventAndFinalBookUpdate
[       OK ] MarketDataTest.TradeCreatesTradeEventAndFinalBookUpdate (0 ms)
[ RUN      ] MarketDataTest.RejectedAndNoFillIocSubmitProduceNoMarketData
[       OK ] MarketDataTest.RejectedAndNoFillIocSubmitProduceNoMarketData (0 ms)
[ RUN      ] MarketDataTest.CancelCreatesBookUpdateAfterRemovingRestingOrder
[       OK ] MarketDataTest.CancelCreatesBookUpdateAfterRemovingRestingOrder (0 ms)
```

`ctest --test-dir build --output-on-failure`

```text
20/75 Test #20: MarketDataTest.LimitOrderRestingCreatesTopOfBookUpdate ..........   Passed
21/75 Test #21: MarketDataTest.TradeCreatesTradeEventAndFinalBookUpdate .........   Passed
22/75 Test #22: MarketDataTest.RejectedAndNoFillIocSubmitProduceNoMarketData ....   Passed
23/75 Test #23: MarketDataTest.CancelCreatesBookUpdateAfterRemovingRestingOrder .   Passed
68/75 Test #68: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback ...***Skipped

100% tests passed, 0 tests failed out of 75
```

### 결과 판단

성공입니다.

- `Trade`를 외부 분배용 `TradeEvent`로 변환함
- 최종 `OrderBookSnapshot`에서 best bid/ask와 depth 기반 price level을 담은 `BookUpdateEvent`를 생성함
- resting order 추가, trade, cancel은 book update를 생성함
- engine reject와 체결 없는 IOC cancel은 market data event를 생성하지 않음
- market data event 생성은 socket I/O 없이 deterministic pure adapter로 유지됨
- 전체 unit test 75개 기준 74개 통과, 1개 sandbox skip

### 다음 단계

다음 단계에서는 UDP market data publisher skeleton을 추가합니다.

- market data event를 text payload로 serialize
- UDP publisher adapter 추가
- loopback UDP smoke test 또는 sandbox-safe serializer test 추가

## 2026-06-08 17단계: UDP market data publisher skeleton 추가

### 목표

Market data event model을 UDP 분배 adapter에 연결할 수 있도록 canonical text payload formatter와 UDP publisher skeleton을 추가합니다.

이번 단계에서는 matching engine과 publisher를 자동 연결하지 않습니다. 대신 event 하나를 text payload 하나로 직렬화하고, UDP datagram 하나로 전송하는 얇은 adapter를 구현합니다.

### 입력

Trade event:

```text
TRADE seq=2 trade_id=1 instrument_id=1001 resting_order_id=20 incoming_order_id=21 aggressor_side=BUY price=73700 quantity=3
```

Book update event:

```text
BOOK_UPDATE seq=4 instrument_id=1001 best_bid_price=73700 best_bid_quantity=2 best_ask_price=NONE best_ask_quantity=0 bids=1 asks=0 bid0_price=73700 bid0_quantity=2
```

UDP loopback smoke event:

```text
TRADE seq=9 trade_id=7 instrument_id=1001 resting_order_id=50 incoming_order_id=51 aggressor_side=SELL price=73500 quantity=4
```

### 변경 내용

- `MarketDataPublishStatus` 추가
- `MarketDataPublishResult` 추가
- `UdpMarketDataPublisher` 추가
- `format_trade_event()` 추가
- `format_book_update_event()` 추가
- `format_market_data_event()` 추가
- UDP publisher의 invalid address / not open / send result status 추가
- event 하나를 datagram 하나로 보내는 `publish(event)` 추가
- event vector를 순차 전송하는 `publish(events)` 추가
- market data formatter unit test 추가
- UDP loopback smoke test 추가
- README, architecture, protocol, recovery-design 문서에 UDP publisher skeleton 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
./build/mini_ats_unit_tests --gtest_filter=MarketDataPublisherTest.*
ctest --test-dir build --output-on-failure
```

### 출력

`./build/mini_ats_unit_tests --gtest_filter=MarketDataPublisherTest.*`

```text
[ RUN      ] MarketDataPublisherTest.FormatsTradeEventAsCanonicalTextPayload
[       OK ] MarketDataPublisherTest.FormatsTradeEventAsCanonicalTextPayload (0 ms)
[ RUN      ] MarketDataPublisherTest.FormatsBookUpdateEventAsCanonicalTextPayload
[       OK ] MarketDataPublisherTest.FormatsBookUpdateEventAsCanonicalTextPayload (0 ms)
[ RUN      ] MarketDataPublisherTest.PublishRequiresOpenSocket
[       OK ] MarketDataPublisherTest.PublishRequiresOpenSocket (0 ms)
[ RUN      ] MarketDataPublisherTest.ReportsInvalidRemoteAddress
[       OK ] MarketDataPublisherTest.ReportsInvalidRemoteAddress (0 ms)
[ RUN      ] MarketDataPublisherTest.PublishesSingleDatagramOverLoopback
[       OK ] MarketDataPublisherTest.PublishesSingleDatagramOverLoopback (0 ms)
```

`ctest --test-dir build --output-on-failure`

```text
24/80 Test #24: MarketDataPublisherTest.FormatsTradeEventAsCanonicalTextPayload .   Passed
25/80 Test #25: MarketDataPublisherTest.FormatsBookUpdateEventAsCanonicalTextPayload  Passed
26/80 Test #26: MarketDataPublisherTest.PublishRequiresOpenSocket ...............   Passed
27/80 Test #27: MarketDataPublisherTest.ReportsInvalidRemoteAddress .............   Passed
28/80 Test #28: MarketDataPublisherTest.PublishesSingleDatagramOverLoopback .....***Skipped
73/80 Test #73: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback ...***Skipped

100% tests passed, 0 tests failed out of 80
```

### 결과 판단

성공입니다.

- `TradeEvent`와 `BookUpdateEvent`를 deterministic text payload로 변환함
- UDP publisher는 IPv4 address/port를 받아 event 하나를 datagram 하나로 전송함
- publisher가 열리지 않은 상태와 invalid address를 명시적 status로 반환함
- UDP loopback smoke는 직접 실행 시 통과함
- 전체 ctest는 80개 기준 78개 통과, UDP/TCP loopback 2개 sandbox skip

### 다음 단계

다음 단계에서는 gateway/matching 처리 결과와 market data publisher를 연결합니다.

- accepted order 처리 후 생성된 market data event를 publisher로 전달
- TCP gateway path에서 market data publisher를 선택적으로 사용할 수 있는 adapter 추가
- replay와 publisher 연결 규칙 문서화

## 2026-06-08 18단계: Gateway와 market data publisher 연결

### 목표

TCP 주문 접수 경로에서 accepted matching 결과를 market data event로 변환하고, 선택적으로 주입된 UDP publisher로 전송합니다.

이번 단계에서는 binary protocol이나 비동기 fan-out은 구현하지 않습니다. 대신 기존 gateway recorder 경계에 publish adapter를 추가하고, CLI에서 `--market-data <addr> <port>`로 publisher를 켤 수 있게 합니다.

### 입력

Resting bid 입력:

```text
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10
```

Trade + final book update 입력:

```text
SUBMIT seq=1 ref=7 order_id=20 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3
SUBMIT seq=2 ref=7 order_id=21 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=5
```

Rejected command 입력:

```text
SUBMIT seq=1 ref=7 order_id=30 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73502 quantity=10
```

CLI invalid address 입력:

```bash
./build/mini_ats --tcp --port 9001 --market-data not-an-ip 9002
```

### 변경 내용

- `PublishedGatewayCommandResult` 추가
- `handle_published_text_command()` 추가
- accepted command 처리 결과에서 market data event 생성
- rejected command는 market data event/payload를 만들지 않도록 연결
- accepted input recorder와 publisher를 함께 사용할 수 있는 gateway 경계 추가
- `TcpOrderServer`에 선택적 `UdpMarketDataPublisher` 주입
- TCP server publish 실패 status 추가
- `mini_ats --tcp --port <port> --market-data <addr> <port>` CLI 추가
- gateway publish adapter unit test 추가
- README, architecture, protocol, recovery-design 문서에 gateway-publisher 연결 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
./build/mini_ats_unit_tests --gtest_filter=GatewayTest.PublishedCommand*
ctest --test-dir build --output-on-failure
./build/mini_ats --help
./build/mini_ats --tcp --port 9001 --market-data not-an-ip 9002
git diff --check
```

### 출력

`./build/mini_ats_unit_tests --gtest_filter=GatewayTest.PublishedCommand*`

```text
[ RUN      ] GatewayTest.PublishedCommandBuildsMarketDataEventsFromEngineResult
[       OK ] GatewayTest.PublishedCommandBuildsMarketDataEventsFromEngineResult (0 ms)
[ RUN      ] GatewayTest.PublishedCommandRecordsAcceptedInputAndSkipsRejectedMarketData
[       OK ] GatewayTest.PublishedCommandRecordsAcceptedInputAndSkipsRejectedMarketData (0 ms)
[  PASSED  ] 2 tests.
```

`ctest --test-dir build --output-on-failure`

```text
20/82 Test #20: GatewayTest.PublishedCommandBuildsMarketDataEventsFromEngineResult ........   Passed
21/82 Test #21: GatewayTest.PublishedCommandRecordsAcceptedInputAndSkipsRejectedMarketData   Passed
30/82 Test #30: MarketDataPublisherTest.PublishesSingleDatagramOverLoopback ...............***Skipped
75/82 Test #75: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback .............***Skipped

100% tests passed, 0 tests failed out of 82
```

`./build/mini_ats --help`

```text
Usage:
  ./build/mini_ats              Run deterministic matching demo
  ./build/mini_ats --gateway    Read text commands from stdin
  ./build/mini_ats --gateway --record-log <path>
                         Append accepted input commands to replay log
  ./build/mini_ats --tcp --port <port> [--record-log <path>]
                         [--market-data <addr> <port>]
                         Serve TCP orders and optionally publish market data
  ./build/mini_ats --help       Show this help
```

`./build/mini_ats --tcp --port 9001 --market-data not-an-ip 9002`

```text
failed to open market data publisher: INVALID_ADDRESS
```

`git diff --check`

```text
(no output)
```

### 결과 판단

성공입니다.

- gateway가 accepted matching 결과에서 market data event를 생성함
- trade 발생 시 `TradeEvent`와 최종 `BookUpdateEvent`가 publisher 경계로 전달됨
- resting order/cancel은 book update event로 전달됨
- parser/replay/engine reject는 market data event를 만들지 않음
- accepted input recording과 publish 경로를 동시에 사용할 수 있음
- TCP gateway에서 `--market-data <addr> <port>`로 UDP publisher를 선택적으로 켤 수 있음
- 전체 ctest는 82개 기준 80개 통과, UDP/TCP loopback 2개 sandbox skip

### 다음 단계

다음 단계에서는 운영 통계 model을 추가합니다.

- 거래량, 거래대금, VWAP 집계 model 추가
- command 처리 latency sample과 p50/p95/p99 계산 경계 추가
- matching/gateway 결과에서 통계 update adapter 추가

## 2026-06-08 19단계: 운영 통계 model 및 formatter 추가

### 목표

Matching/gateway 처리 결과에서 운영 지표를 집계할 수 있는 `stats` 모듈을 추가합니다.

이번 단계에서는 통계를 live TCP server에 노출하지 않습니다. 대신 core 결과와 gateway response에서 재사용할 수 있는 순수 집계 model, latency percentile 계산, canonical text formatter를 먼저 구현합니다.

### 입력

VWAP 검증용 matching 입력:

```text
SUBMIT seq=1 ref=1 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=100 quantity=10
SUBMIT seq=2 ref=1 order_id=11 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=110 quantity=4
SUBMIT seq=3 ref=1 order_id=20 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=110 quantity=14
```

Latency percentile sample:

```text
10us, 20us, 30us, 40us, 50us
```

Stats formatter 기대 출력:

```text
STATS commands_received=5 commands_accepted=3 commands_rejected=2 trades=2 traded_quantity=5 traded_notional=510 vwap_notional=510 vwap_quantity=5 vwap_floor_price=102 latency_samples=5 latency_min_ns=10000 latency_max_ns=50000 latency_p50_ns=30000 latency_p95_ns=50000 latency_p99_ns=50000
```

### 변경 내용

- `mini_ats_stats` CMake library 추가
- `OperationalStatistics` 추가
- command received/accepted/rejected count snapshot 추가
- trade count, 거래량, 거래대금 집계 추가
- `ExactVwap` 추가
- VWAP을 `double`이 아닌 `notional / quantity` ratio로 보관
- latency min/max/p50/p95/p99 snapshot 추가
- nearest-rank percentile 계산 추가
- negative latency를 0ns로 clamp
- invalid trade는 통계에서 제외
- `format_operational_statistics()` canonical text formatter 추가
- stats unit test 3개 추가
- README, architecture, benchmark, recovery-design 문서에 운영 통계 model 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
./build/mini_ats_unit_tests --gtest_filter=OperationalStatsTest.*
ctest --test-dir build --output-on-failure
git diff --check
```

### 출력

`./build/mini_ats_unit_tests --gtest_filter=OperationalStatsTest.*`

```text
[ RUN      ] OperationalStatsTest.AccumulatesTradeVolumeNotionalAndExactVwapFromEngineResults
[       OK ] OperationalStatsTest.AccumulatesTradeVolumeNotionalAndExactVwapFromEngineResults (0 ms)
[ RUN      ] OperationalStatsTest.RecordsCommandCountsLatencyPercentilesAndAcceptedTrades
[       OK ] OperationalStatsTest.RecordsCommandCountsLatencyPercentilesAndAcceptedTrades (0 ms)
[ RUN      ] OperationalStatsTest.IgnoresInvalidTradesClampsNegativeLatencyAndResets
[       OK ] OperationalStatsTest.IgnoresInvalidTradesClampsNegativeLatencyAndResets (0 ms)
[  PASSED  ] 3 tests.
```

`ctest --test-dir build --output-on-failure`

```text
48/85 Test #48: OperationalStatsTest.AccumulatesTradeVolumeNotionalAndExactVwapFromEngineResults   Passed
49/85 Test #49: OperationalStatsTest.RecordsCommandCountsLatencyPercentilesAndAcceptedTrades       Passed
50/85 Test #50: OperationalStatsTest.IgnoresInvalidTradesClampsNegativeLatencyAndResets            Passed
30/85 Test #30: MarketDataPublisherTest.PublishesSingleDatagramOverLoopback .....................***Skipped
78/85 Test #78: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback ...................***Skipped

100% tests passed, 0 tests failed out of 85
```

### 결과 판단

성공입니다.

- matching result trade에서 거래량과 거래대금을 누적함
- `100 * 10 + 110 * 4 = 1440`, 수량 `14`의 exact VWAP ratio를 보존함
- VWAP floor price는 `102`로 계산됨
- gateway 경로에서 사용할 수 있도록 accepted 여부, trade 목록, latency를 함께 기록하는 adapter를 제공함
- latency sample은 nearest-rank 규칙으로 p50/p95/p99를 계산함
- stats snapshot을 canonical text 한 줄로 format할 수 있음
- 전체 ctest는 85개 기준 83개 통과, UDP/TCP loopback 2개 sandbox skip

### 다음 단계

다음 단계에서는 운영 통계를 실제 실행 경로에 연결합니다.

- `--gateway` stdin runner에 선택적 stats 출력 옵션 추가
- TCP gateway command 처리 latency를 `OperationalStatistics`로 기록
- benchmark runner 또는 deterministic stats demo 추가

## 2026-06-08 20단계: Stdin gateway stats 출력 옵션 추가

### 목표

19단계에서 만든 운영 통계 model을 실제 실행 경로 중 stdin gateway runner에 연결합니다.

이번 단계에서는 TCP server의 장시간 실행 통계 노출이나 benchmark runner는 구현하지 않습니다. 대신 `--gateway --stats`를 추가해 stdin command 처리가 끝난 뒤 stats snapshot을 한 줄로 출력합니다.

### 입력

```text
SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3
SUBMIT seq=2 ref=7 order_id=11 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=3
```

### 변경 내용

- `mini_ats` executable에 `mini_ats_stats` link 추가
- `--gateway [--record-log <path>] [--stats]` CLI parsing 추가
- stdin gateway runner에서 command별 처리 latency 측정
- accepted 여부, response trade 목록, latency를 `OperationalStatistics`에 기록
- 처리 종료 시 `format_operational_statistics()` 결과를 stderr로 출력
- stdout의 canonical gateway response와 stderr의 stats snapshot을 분리
- README, architecture, protocol, benchmark 문서에 `--stats` 실행 경로 반영

### 실행 명령

```bash
cmake --build build
./build/mini_ats --help
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3' \
  'SUBMIT seq=2 ref=7 order_id=11 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=3' \
  | ./build/mini_ats --gateway --stats
ctest --test-dir build --output-on-failure
git diff --check
```

### 출력

`./build/mini_ats --help`

```text
Usage:
  ./build/mini_ats              Run deterministic matching demo
  ./build/mini_ats --gateway [--record-log <path>] [--stats]
                         Read text commands from stdin
                         Append accepted input commands and/or print stats
  ./build/mini_ats --tcp --port <port> [--record-log <path>]
                         [--market-data <addr> <port>]
                         Serve TCP orders and optionally publish market data
  ./build/mini_ats --help       Show this help
```

`./build/mini_ats --gateway --stats`

stdout:

```text
ACCEPTED reason=NONE seq=1 command=SUBMIT detail=ACCEPTED trades=0 reports=1 ...
ACCEPTED reason=NONE seq=2 command=SUBMIT detail=ACCEPTED trades=1 reports=2 ...
```

stderr:

```text
STATS commands_received=2 commands_accepted=2 commands_rejected=0 trades=1 traded_quantity=3 traded_notional=221100 vwap_notional=221100 vwap_quantity=3 vwap_floor_price=73700 latency_samples=2 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
```

Latency 값은 실제 실행 시간 측정값이므로 실행마다 달라집니다.

`ctest --test-dir build --output-on-failure`

```text
48/85 Test #48: OperationalStatsTest.AccumulatesTradeVolumeNotionalAndExactVwapFromEngineResults   Passed
49/85 Test #49: OperationalStatsTest.RecordsCommandCountsLatencyPercentilesAndAcceptedTrades       Passed
50/85 Test #50: OperationalStatsTest.IgnoresInvalidTradesClampsNegativeLatencyAndResets            Passed
30/85 Test #30: MarketDataPublisherTest.PublishesSingleDatagramOverLoopback .....................***Skipped
78/85 Test #78: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback ...................***Skipped

100% tests passed, 0 tests failed out of 85
```

### 결과 판단

성공입니다.

- `--gateway --stats`로 stdin runner의 command count와 latency를 집계함
- accepted trade만 거래량/거래대금/VWAP 통계에 반영함
- gateway response stream은 stdout에 유지하고 stats snapshot은 stderr에 분리함
- `--record-log`와 `--stats`를 같은 CLI parser에서 함께 사용할 수 있게 됨
- 전체 ctest는 85개 기준 83개 통과, UDP/TCP loopback 2개 sandbox skip

### 다음 단계

다음 단계에서는 장시간 실행 경로와 측정 시나리오를 보강합니다.

- TCP gateway에 stats collector를 주입하고 종료/오류 시 snapshot 출력
- deterministic benchmark runner 추가
- benchmark 문서에 고정 입력 시나리오와 측정 결과 기록

## 2026-06-09 21단계: TCP stats collector와 deterministic benchmark runner 추가

### 목표

운영 통계 model을 TCP gateway와 deterministic benchmark runner에 연결합니다.

이번 단계에서는 DB 기반 기준정보 로딩이나 외부 benchmark 저장소는 구현하지 않습니다. 대신 장시간 실행 경로인 TCP server에 stats collector를 주입하고, 재현 가능한 고정 gateway 입력 시나리오를 반복 실행하는 benchmark runner를 추가합니다.

### 입력

Benchmark iteration 1회 입력:

```text
SUBMIT seq=1 ref=7 order_id=1000 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3
SUBMIT seq=2 ref=7 order_id=1001 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=3
SUBMIT seq=3 ref=7 order_id=1002 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73702 quantity=1
```

Iteration 1회 기대 집계:

```text
commands=3 accepted=2 rejected=1 trades=1 traded_quantity=3 traded_notional=221100 vwap_floor_price=73700
```

### 변경 내용

- `TcpOrderServer`에 선택적 `OperationalStatistics*` 주입
- TCP command 처리 latency 측정
- TCP response accepted 여부와 trade 목록을 stats collector에 기록
- TCP CLI에 `--stats` 옵션 추가
- TCP 종료/오류 경로에서 stats snapshot을 stderr에 출력
- `mini_ats_benchmark` CMake library 추가
- `run_deterministic_gateway_benchmark()` 추가
- `format_deterministic_benchmark_result()` 추가
- `mini_ats --benchmark [--iterations <n>]` CLI 추가
- deterministic benchmark unit test 2개 추가
- TCP loopback test에 stats snapshot 검증 추가
- README, architecture, protocol, benchmark 문서에 TCP stats와 benchmark runner 반영

### 실행 명령

```bash
cmake -S . -B build
cmake --build build
./build/mini_ats_unit_tests --gtest_filter=DeterministicBenchmarkTest.*:OperationalStatsTest.*
./build/mini_ats_unit_tests --gtest_filter=TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback
./build/mini_ats --help
./build/mini_ats --benchmark --iterations 2
./build/mini_ats --benchmark --iterations 0
ctest --test-dir build --output-on-failure
git diff --check
```

### 출력

`./build/mini_ats_unit_tests --gtest_filter=DeterministicBenchmarkTest.*:OperationalStatsTest.*`

```text
[ RUN      ] DeterministicBenchmarkTest.RunsFixedGatewayScenarioAndCollectsStats
[       OK ] DeterministicBenchmarkTest.RunsFixedGatewayScenarioAndCollectsStats (0 ms)
[ RUN      ] DeterministicBenchmarkTest.FormatsBenchmarkSummaryWithStatsPayload
[       OK ] DeterministicBenchmarkTest.FormatsBenchmarkSummaryWithStatsPayload (0 ms)
[ RUN      ] OperationalStatsTest.AccumulatesTradeVolumeNotionalAndExactVwapFromEngineResults
[       OK ] OperationalStatsTest.AccumulatesTradeVolumeNotionalAndExactVwapFromEngineResults (0 ms)
[ RUN      ] OperationalStatsTest.RecordsCommandCountsLatencyPercentilesAndAcceptedTrades
[       OK ] OperationalStatsTest.RecordsCommandCountsLatencyPercentilesAndAcceptedTrades (0 ms)
[ RUN      ] OperationalStatsTest.IgnoresInvalidTradesClampsNegativeLatencyAndResets
[       OK ] OperationalStatsTest.IgnoresInvalidTradesClampsNegativeLatencyAndResets (0 ms)
[  PASSED  ] 5 tests.
```

`./build/mini_ats_unit_tests --gtest_filter=TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback`

```text
[ RUN      ] TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback
[       OK ] TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback (0 ms)
[  PASSED  ] 1 test.
```

`./build/mini_ats --help`

```text
Usage:
  ./build/mini_ats              Run deterministic matching demo
  ./build/mini_ats --gateway [--record-log <path>] [--stats]
                         Read text commands from stdin
                         Append accepted input commands and/or print stats
  ./build/mini_ats --tcp --port <port> [--record-log <path>]
                         [--market-data <addr> <port>] [--stats]
                         Serve TCP orders and optionally publish market data
  ./build/mini_ats --benchmark [--iterations <n>]
                         Run deterministic gateway benchmark scenario
  ./build/mini_ats --help       Show this help
```

`./build/mini_ats --benchmark --iterations 2`

```text
BENCHMARK scenario=deterministic_gateway iterations=2 commands=6 elapsed_ns=<runtime> STATS commands_received=6 commands_accepted=4 commands_rejected=2 trades=2 traded_quantity=6 traded_notional=442200 vwap_notional=442200 vwap_quantity=6 vwap_floor_price=73700 latency_samples=6 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
```

`./build/mini_ats --benchmark --iterations 0`

```text
invalid benchmark iterations: 0
```

`ctest --test-dir build --output-on-failure`

```text
1/87 Test #1: DeterministicBenchmarkTest.RunsFixedGatewayScenarioAndCollectsStats ......   Passed
2/87 Test #2: DeterministicBenchmarkTest.FormatsBenchmarkSummaryWithStatsPayload .......   Passed
32/87 Test #32: MarketDataPublisherTest.PublishesSingleDatagramOverLoopback ............***Skipped
80/87 Test #80: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback ..........***Skipped

100% tests passed, 0 tests failed out of 87
```

### 결과 판단

성공입니다.

- TCP gateway가 command별 처리 latency와 accepted/rejected count를 stats collector에 기록함
- TCP loopback smoke에서 stats snapshot이 command 3개, accepted 2개, rejected 1개, trade 1개로 집계됨
- deterministic benchmark runner가 고정 gateway 시나리오를 반복 실행함
- iteration 2회 기준 command 6개, accepted 4개, rejected 2개, trade 2개, 거래대금 442200으로 집계됨
- invalid benchmark iteration은 명시적 오류로 거부함
- 전체 ctest는 87개 기준 85개 통과, UDP/TCP loopback 2개 sandbox skip
- TCP loopback 단독 실행은 통과

### 다음 단계

다음 단계에서는 PostgreSQL connection adapter 또는 benchmark 결과 기록을 추가합니다.

- `reference_data`의 SQL/query model 뒤에 실제 PostgreSQL loader adapter 추가
- benchmark runner 결과를 파일로 기록하는 옵션 추가
- README에 end-to-end 실행 시나리오 정리

## 2026-06-09 22단계: psql 기반 PostgreSQL instrument loader adapter 추가

### 목표

`reference_data`의 순수 row mapping 뒤에 PostgreSQL에서 단일 instrument 기준정보를 읽어오는 loader adapter를 추가합니다.

현재 개발 환경에는 `psql` 실행 파일과 libpq header가 없으므로, libpq에 직접 link하는 대신 `psql` CLI adapter를 먼저 구현합니다. 이 구조는 빌드 의존성을 늘리지 않으면서도 DB query, TSV output parsing, mapping 실패, command 실패를 명확히 분리합니다.

### 입력

psql TSV output:

```text
1001	DEMO	5	70000	80000	OPEN	7
```

CLI 성공 시 출력 형식:

```text
INSTRUMENT instrument_id=1001 tick_size=5 lower_price_limit=70000 upper_price_limit=80000 session=OPEN reference_version=7
```

CLI 실패 경로:

```bash
./build/mini_ats --load-instrument --instrument-id 1001 --psql definitely-missing-psql-binary
```

### 변경 내용

- `PostgresInstrumentLoadError` 추가
- `PostgresInstrumentRepositoryConfig` 추가
- `PostgresInstrumentLoadResult` 추가
- `instrument_reference_psql_query()` 추가
- `build_psql_instrument_command()` 추가
- `parse_psql_instrument_result()` 추가
- `load_instrument_reference_from_postgres()` 추가
- `format_instrument_reference()` 추가
- `InstrumentLoadError` / `PostgresInstrumentLoadError` text formatter 추가
- `mini_ats --load-instrument --instrument-id <id>` CLI 추가
- `--db-name`, `--db-user`, `--psql` 옵션 추가
- `MINI_ATS_DB_NAME`, `MINI_ATS_DB_USER`, `USER` env fallback 추가
- DB seed의 `reference_version`을 gateway/replay 예제와 맞게 `7`로 조정
- psql query/command builder, TSV parser, malformed output, command failure unit test 추가
- README, architecture, protocol, recovery-design 문서에 psql loader adapter 반영

### 실행 명령

```bash
cmake --build build
./build/mini_ats_unit_tests --gtest_filter=ReferenceDataTest.*
./build/mini_ats --help
./build/mini_ats --load-instrument --instrument-id 0
./build/mini_ats --load-instrument --instrument-id 1001 --psql definitely-missing-psql-binary
ctest --test-dir build --output-on-failure
git diff --check
```

### 출력

`./build/mini_ats_unit_tests --gtest_filter=ReferenceDataTest.*`

```text
[ RUN      ] ReferenceDataTest.ParsesMarketSessionFromPostgresValue
[       OK ] ReferenceDataTest.ParsesMarketSessionFromPostgresValue (0 ms)
[ RUN      ] ReferenceDataTest.MapsInstrumentRecordToInstrumentReference
[       OK ] ReferenceDataTest.MapsInstrumentRecordToInstrumentReference (0 ms)
[ RUN      ] ReferenceDataTest.RejectsInvalidInstrumentRecordFields
[       OK ] ReferenceDataTest.RejectsInvalidInstrumentRecordFields (0 ms)
[ RUN      ] ReferenceDataTest.ExposesParameterizedInstrumentReferenceQuery
[       OK ] ReferenceDataTest.ExposesParameterizedInstrumentReferenceQuery (0 ms)
[ RUN      ] ReferenceDataTest.BuildsPsqlInstrumentQueryAndCommand
[       OK ] ReferenceDataTest.BuildsPsqlInstrumentQueryAndCommand (0 ms)
[ RUN      ] ReferenceDataTest.ParsesPsqlInstrumentResultRow
[       OK ] ReferenceDataTest.ParsesPsqlInstrumentResultRow (0 ms)
[ RUN      ] ReferenceDataTest.RejectsMalformedPsqlInstrumentResults
[       OK ] ReferenceDataTest.RejectsMalformedPsqlInstrumentResults (0 ms)
[ RUN      ] ReferenceDataTest.ReportsPostgresCommandFailureWithoutMutatingState
[       OK ] ReferenceDataTest.ReportsPostgresCommandFailureWithoutMutatingState (41 ms)
[  PASSED  ] 8 tests.
```

`./build/mini_ats --help`

```text
Usage:
  ./build/mini_ats              Run deterministic matching demo
  ./build/mini_ats --gateway [--record-log <path>] [--stats]
                         Read text commands from stdin
                         Append accepted input commands and/or print stats
  ./build/mini_ats --tcp --port <port> [--record-log <path>]
                         [--market-data <addr> <port>] [--stats]
                         Serve TCP orders and optionally publish market data
  ./build/mini_ats --benchmark [--iterations <n>]
                         Run deterministic gateway benchmark scenario
  ./build/mini_ats --load-instrument --instrument-id <id>
                         [--db-name <name>] [--db-user <user>] [--psql <path>]
                         Load one instrument reference from PostgreSQL via psql
  ./build/mini_ats --help       Show this help
```

`./build/mini_ats --load-instrument --instrument-id 0`

```text
invalid instrument id: 0
```

`./build/mini_ats --load-instrument --instrument-id 1001 --psql definitely-missing-psql-binary`

```text
failed to load instrument: COMMAND_FAILED detail=psql command failed
```

`ctest --test-dir build --output-on-failure`

```text
69/91 Test #69: ReferenceDataTest.BuildsPsqlInstrumentQueryAndCommand .............   Passed
70/91 Test #70: ReferenceDataTest.ParsesPsqlInstrumentResultRow ...................   Passed
71/91 Test #71: ReferenceDataTest.RejectsMalformedPsqlInstrumentResults ...........   Passed
72/91 Test #72: ReferenceDataTest.ReportsPostgresCommandFailureWithoutMutatingState Passed
32/91 Test #32: MarketDataPublisherTest.PublishesSingleDatagramOverLoopback .......***Skipped
84/91 Test #84: TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback .....***Skipped

100% tests passed, 0 tests failed out of 91
```

### 결과 판단

성공입니다.

- PostgreSQL row mapping 뒤에 psql 기반 loader adapter를 추가함
- psql TSV output 한 줄을 `InstrumentReference`로 변환함
- malformed output, multiple rows, empty result, numeric parse failure를 구분함
- psql command 실패를 명시적 `COMMAND_FAILED`로 반환함
- `--load-instrument` CLI로 DB 기준정보 조회 경계를 실행할 수 있음
- 현재 환경에는 `psql`이 없어 실제 DB 성공 경로는 실행하지 못했고, 실패 경로를 검증함
- 전체 ctest는 91개 기준 89개 통과, UDP/TCP loopback 2개 sandbox skip

### 다음 단계

다음 단계에서는 기준정보 loader를 gateway 실행 경로 또는 benchmark 기록 경로에 연결합니다.

- `--gateway` / `--tcp`에서 `--instrument-id`와 `--load-reference-data` 옵션으로 DB 기준정보 주입
- benchmark 결과를 파일로 기록하는 옵션 추가
- `psql`/PostgreSQL 설치 환경에서 end-to-end smoke 실행

## 2026-06-09 23단계: Gateway 실행 경로에 기준정보 loader 연결

### 목표

22단계에서 만든 psql 기반 PostgreSQL instrument loader를 실제 주문 접수 실행 경로에 연결합니다.

이번 단계에서는 libpq 직접 연결이나 여러 종목 라우팅은 구현하지 않습니다. 대신 `--gateway`와 `--tcp` 실행 모드에서 단일 instrument 기준정보를 DB에서 읽어 `MatchingEngine` 생성자에 주입할 수 있게 합니다.

### 입력

psql TSV output smoke:

```text
1001	DEMO	10	70000	80000	OPEN	9
```

Gateway command smoke:

```text
SUBMIT seq=1 ref=9 order_id=300 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=2
```

### 변경 내용

- `run_gateway()`와 `run_tcp_gateway()`가 `InstrumentReference`를 인자로 받도록 변경
- `--gateway`에 `--load-reference-data --instrument-id <id>` 옵션 추가
- `--tcp`에 `--load-reference-data --instrument-id <id>` 옵션 추가
- gateway/TCP 모드에서 `--db-name`, `--db-user`, `--psql` 옵션 재사용
- `--load-reference-data` 없이 기준정보 옵션만 지정하면 명시적으로 실패
- loader 실패 시 `COMMAND_FAILED`, mapping error, detail을 stderr에 출력
- `--help` usage에 gateway/TCP 기준정보 옵션 반영
- README, architecture, protocol, recovery-design 문서에 gateway/TCP DB 기준정보 주입 경로 반영

### 실행 명령

```bash
cmake --build build
./build/mini_ats --help
./build/mini_ats --gateway --instrument-id 1001
./build/mini_ats --tcp --port 0 --load-reference-data --instrument-id 1001 --psql definitely-missing-psql-binary
printf '%s\n' 'SUBMIT seq=1 ref=9 order_id=300 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=2' \
  | ./build/mini_ats --gateway --load-reference-data --instrument-id 1001 --psql /tmp/mini_ats_fake_psql --stats
./build/mini_ats_unit_tests --gtest_filter=ReferenceDataTest.*:TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback
ctest --test-dir build --output-on-failure
git diff --check
```

### 출력

`./build/mini_ats --help`

```text
Usage:
  ./build/mini_ats              Run deterministic matching demo
  ./build/mini_ats --gateway [--record-log <path>] [--stats]
                         [--load-reference-data --instrument-id <id>]
                         [--db-name <name>] [--db-user <user>] [--psql <path>]
                         Read text commands from stdin
                         Append accepted input commands and/or print stats
  ./build/mini_ats --tcp --port <port> [--record-log <path>]
                         [--market-data <addr> <port>] [--stats]
                         [--load-reference-data --instrument-id <id>]
                         [--db-name <name>] [--db-user <user>] [--psql <path>]
                         Serve TCP orders and optionally publish market data
```

`./build/mini_ats --gateway --instrument-id 1001`

```text
reference data options require --load-reference-data
```

`./build/mini_ats --tcp --port 0 --load-reference-data --instrument-id 1001 --psql definitely-missing-psql-binary`

```text
failed to load reference data: COMMAND_FAILED detail=psql command failed
```

Fake psql 기반 gateway smoke:

```text
STATS commands_received=1 commands_accepted=1 commands_rejected=0 trades=0 traded_quantity=0 traded_notional=0 vwap=NONE latency_samples=1 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
ACCEPTED reason=NONE seq=1 command=SUBMIT detail=ACCEPTED trades=0 reports=1 report0_order_id=300 report0_instrument_id=1001 report0_type=ACCEPTED report0_status=ACCEPTED report0_filled_quantity=0 report0_remaining_quantity=2 report0_last_price=0 report0_last_quantity=0 report0_reject_reason=NONE report0_sequence=1
```

`./build/mini_ats_unit_tests --gtest_filter=ReferenceDataTest.*:TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback`

```text
[  PASSED  ] 9 tests.
```

`ctest --test-dir build --output-on-failure`

```text
100% tests passed, 0 tests failed out of 91

The following tests did not run:
	 32 - MarketDataPublisherTest.PublishesSingleDatagramOverLoopback (Skipped)
	 84 - TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback (Skipped)
```

### 결과 판단

성공입니다.

- stdin gateway와 TCP gateway가 기본 demo 기준정보 대신 DB loader 결과를 matching engine에 주입할 수 있음
- `ref=9` command가 fake psql의 `reference_version=9` 기준정보로 accepted됨
- `--load-reference-data` 없이 DB 기준정보 옵션을 쓰는 실수를 명시적으로 차단함
- TCP 모드에서도 loader 실패가 server listen 전에 명확한 오류로 반환됨
- 전체 ctest는 91개 기준 89개 통과, UDP/TCP loopback 2개 sandbox skip
- TCP loopback 단독 실행은 통과

### 다음 단계

다음 단계에서는 benchmark 결과 축적 또는 실제 PostgreSQL smoke를 추가합니다.

- benchmark 결과를 파일로 기록하는 옵션 추가
- `psql`/PostgreSQL 설치 환경에서 gateway/TCP end-to-end smoke 실행
- libpq 기반 connection adapter 검토

## 2026-06-09 24단계: Benchmark 결과 파일 기록 옵션 추가

### 목표

Deterministic benchmark runner의 한 줄 결과 payload를 파일에 축적할 수 있게 합니다.

이번 단계에서는 별도 결과 DB나 JSON schema를 만들지 않습니다. 기존 canonical text payload를 stdout에 유지하면서, `--output <path>`가 지정된 경우 같은 한 줄을 파일 끝에 append합니다.

### 입력

Benchmark CLI:

```bash
./build/mini_ats --benchmark --iterations 2 --output /tmp/mini_ats_benchmark_continue.log
./build/mini_ats --benchmark --iterations 1 --output /tmp/mini_ats_benchmark_continue.log
```

실패 경로:

```bash
./build/mini_ats --benchmark --iterations 1 --output /tmp
```

### 변경 내용

- `--benchmark` 모드에 `--output <path>` 옵션 추가
- output 파일은 `std::ios::app`으로 열어 결과를 append
- stdout benchmark payload 출력은 유지
- output 파일 open 실패 시 benchmark 실행 전에 명시적 오류 반환
- `scripts/run_benchmark.sh`가 실제 benchmark runner를 빌드 후 실행하도록 갱신
- README, architecture, protocol, benchmark 문서에 결과 파일 기록 옵션 반영

### 실행 명령

```bash
cmake --build build
./build/mini_ats --benchmark --iterations 2 --output /tmp/mini_ats_benchmark_continue.log
./build/mini_ats --benchmark --iterations 1 --output /tmp/mini_ats_benchmark_continue.log
tail -n 2 /tmp/mini_ats_benchmark_continue.log
./build/mini_ats --benchmark --iterations 1 --output /tmp
./build/mini_ats_unit_tests
ctest --test-dir build
git diff --check
```

### 출력

`tail -n 2 /tmp/mini_ats_benchmark_continue.log`

```text
BENCHMARK scenario=deterministic_gateway iterations=2 commands=6 elapsed_ns=<runtime> STATS commands_received=6 commands_accepted=4 commands_rejected=2 trades=2 traded_quantity=6 traded_notional=442200 vwap_notional=442200 vwap_quantity=6 vwap_floor_price=73700 latency_samples=6 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
BENCHMARK scenario=deterministic_gateway iterations=1 commands=3 elapsed_ns=<runtime> STATS commands_received=3 commands_accepted=2 commands_rejected=1 trades=1 traded_quantity=3 traded_notional=221100 vwap_notional=221100 vwap_quantity=3 vwap_floor_price=73700 latency_samples=3 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
```

`./build/mini_ats --benchmark --iterations 1 --output /tmp`

```text
failed to open benchmark output: /tmp
```

`./build/mini_ats_unit_tests`

```text
[==========] Running 91 tests from 14 test suites.
[  PASSED  ] 91 tests.
```

`ctest --test-dir build`

```text
100% tests passed, 0 tests failed out of 91

The following tests did not run:
	 32 - MarketDataPublisherTest.PublishesSingleDatagramOverLoopback (Skipped)
	 84 - TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback (Skipped)
```

### 결과 판단

성공입니다.

- benchmark payload가 stdout에 계속 출력됨
- `--output` 지정 시 같은 payload가 파일에 append됨
- 반복 실행하면 파일 마지막에 실행 결과가 순서대로 쌓임
- 디렉터리처럼 열 수 없는 output path는 명시적 오류로 실패함
- 전체 unit test와 ctest가 통과함

### 다음 단계

다음 단계에서는 실제 PostgreSQL smoke 또는 결과 기록 포맷을 더 구조화합니다.

- `psql`/PostgreSQL 설치 환경에서 gateway/TCP end-to-end smoke 실행
- benchmark 결과에 실행 환경 metadata를 함께 남기는 옵션 검토
- libpq 기반 connection adapter 검토

## 2026-06-09 25단계: Benchmark 실행 환경 metadata 출력

### 목표

Benchmark 결과 payload에 실행 환경 metadata를 포함해 여러 실행 결과를 비교할 수 있게 합니다.

이번 단계에서는 별도 JSON이나 DB schema를 만들지 않고, 기존 `BENCHMARK ... STATS ...` 한 줄 형식을 유지합니다. 새 metadata는 `key=value` field로 추가합니다.

### 입력

Benchmark CLI:

```bash
./build/mini_ats --benchmark --iterations 1
```

### 변경 내용

- `BenchmarkEnvironment` 추가
- `collect_benchmark_environment()` 추가
- compiler, C++ standard, build mode, OS, architecture, hardware thread count 수집
- benchmark 결과 payload에 `commands_per_second_floor` 추가
- deterministic benchmark unit test에 environment metadata 검증 추가
- README, architecture, protocol, benchmark 문서에 metadata 출력 반영

### 실행 명령

```bash
cmake --build build
./build/mini_ats_unit_tests --gtest_filter=DeterministicBenchmarkTest.*
./build/mini_ats --benchmark --iterations 1
./build/mini_ats_unit_tests
ctest --test-dir build
git diff --check
```

### 출력

`./build/mini_ats_unit_tests --gtest_filter=DeterministicBenchmarkTest.*`

```text
[==========] Running 3 tests from 1 test suite.
[ RUN      ] DeterministicBenchmarkTest.RunsFixedGatewayScenarioAndCollectsStats
[       OK ] DeterministicBenchmarkTest.RunsFixedGatewayScenarioAndCollectsStats (0 ms)
[ RUN      ] DeterministicBenchmarkTest.CollectsBenchmarkEnvironmentMetadata
[       OK ] DeterministicBenchmarkTest.CollectsBenchmarkEnvironmentMetadata (0 ms)
[ RUN      ] DeterministicBenchmarkTest.FormatsBenchmarkSummaryWithStatsPayload
[       OK ] DeterministicBenchmarkTest.FormatsBenchmarkSummaryWithStatsPayload (0 ms)
[  PASSED  ] 3 tests.
```

`./build/mini_ats --benchmark --iterations 1`

```text
BENCHMARK scenario=deterministic_gateway iterations=1 commands=3 elapsed_ns=<runtime> commands_per_second_floor=<throughput> compiler=gcc-13.3.0 cpp_standard=202002 build_mode=debug os=linux architecture=x86_64 hardware_threads=28 STATS commands_received=3 commands_accepted=2 commands_rejected=1 trades=1 traded_quantity=3 traded_notional=221100 vwap_notional=221100 vwap_quantity=3 vwap_floor_price=73700 latency_samples=3 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
```

### 결과 판단

성공입니다.

- benchmark 결과 한 줄에 실행 환경 metadata가 포함됨
- 기존 `STATS` payload와 deterministic scenario count는 유지됨
- throughput은 command 수와 elapsed time 기준 floor 정수로 출력됨
- output file append 옵션과 함께 쓰면 실행별 환경과 결과가 같은 줄에 함께 축적됨

### 다음 단계

다음 단계에서는 실제 PostgreSQL smoke 또는 결과 기록 포맷을 더 구조화합니다.

- `psql`/PostgreSQL 설치 환경에서 gateway/TCP end-to-end smoke 실행
- benchmark 결과를 TSV/CSV로 변환하는 별도 script 검토
- libpq 기반 connection adapter 검토

## 2026-06-10 26단계: psql 기반 기준정보 loader와 gateway 주입 옵션 추가

### 목표

PostgreSQL의 `mini_ats.instruments` row를 실행 시점에 읽어 `MatchingEngine` 기준정보로 주입할 수 있게 합니다.

이번 단계에서는 libpq 직접 연결 대신 `psql` 실행 파일을 사용하는 얇은 adapter를 둡니다. 테스트 환경에 PostgreSQL 서버가 없어도 parsing, command 생성, 오류 경계를 unit test로 검증할 수 있게 유지합니다.

### 입력

단일 instrument loader:

```bash
./build/mini_ats --load-instrument --instrument-id 1001
```

Gateway 기준정보 주입:

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway --load-reference-data --instrument-id 1001
```

### 변경 내용

- `PostgresInstrumentRepositoryConfig` 추가
- `PostgresInstrumentLoadResult`와 `PostgresInstrumentLoadError` 추가
- `instrument_reference_psql_query()` 추가
- `build_psql_instrument_command()` 추가
- `parse_psql_instrument_result()` 추가
- `load_instrument_reference_from_postgres()` 추가
- `format_instrument_reference()` 추가
- `--load-instrument --instrument-id <id>` CLI 추가
- `--gateway`와 `--tcp`에 `--load-reference-data --instrument-id <id>` 옵션 추가
- `--db-name`, `--db-user`, `--psql` 옵션 추가
- TCP gateway `--stats` 경로 추가
- seed 기준정보 `reference_version`을 replay/gateway 예제와 같은 `7`로 조정
- README, architecture, protocol, recovery-design 문서에 기준정보 loader 실행 경로 반영

### 실행 명령

```bash
cmake --build build
./build/mini_ats_unit_tests
./build/mini_ats --load-instrument --instrument-id 0
./build/mini_ats --load-instrument --instrument-id 1001 --psql definitely-missing-psql-binary
./build/mini_ats --gateway --load-reference-data --instrument-id 1001 --psql definitely-missing-psql-binary
./build/mini_ats --benchmark --iterations 1
```

### 출력

`./build/mini_ats_unit_tests`

```text
[==========] Running 92 tests from 14 test suites.
[  PASSED  ] 92 tests.
```

`./build/mini_ats --load-instrument --instrument-id 0`

```text
invalid instrument id: 0
```

`./build/mini_ats --load-instrument --instrument-id 1001 --psql definitely-missing-psql-binary`

```text
failed to load instrument: COMMAND_FAILED detail=psql command failed
```

`./build/mini_ats --gateway --load-reference-data --instrument-id 1001 --psql definitely-missing-psql-binary`

```text
failed to load reference data: COMMAND_FAILED detail=psql command failed
```

`./build/mini_ats --benchmark --iterations 1`

```text
BENCHMARK scenario=deterministic_gateway iterations=1 commands=3 elapsed_ns=<runtime> commands_per_second_floor=<throughput> compiler=gcc-13.3.0 cpp_standard=202002 build_mode=debug os=linux architecture=x86_64 hardware_threads=28 STATS commands_received=3 commands_accepted=2 commands_rejected=1 trades=1 traded_quantity=3 traded_notional=221100 vwap_notional=221100 vwap_quantity=3 vwap_floor_price=73700 latency_samples=3 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
```

### 결과 판단

성공입니다.

- psql 출력 한 줄을 `InstrumentReference`로 변환하는 경계를 추가함
- psql command 생성 시 DB 이름, 사용자, psql 경로를 옵션으로 지정할 수 있음
- stdin/TCP gateway가 기본 demo instrument 대신 DB에서 읽은 기준정보를 선택적으로 주입받을 수 있음
- 잘못된 instrument id와 psql 실행 실패가 명시적 오류로 반환됨
- 현재 로컬 환경에는 `psql`이 없어 실제 PostgreSQL smoke는 실행하지 못함
- 기존 benchmark와 전체 unit test는 계속 통과함

### 다음 단계

다음 단계에서는 실제 PostgreSQL 환경 또는 구조화된 benchmark 기록 경로를 보강합니다.

- `psql`/PostgreSQL 설치 환경에서 `--load-instrument` 성공 smoke 실행
- DB 기준정보를 주입한 stdin/TCP gateway end-to-end smoke 실행
- benchmark 결과를 TSV/CSV로 변환하는 별도 script 추가
- libpq 기반 connection adapter 검토

## 2026-06-10 27단계: Benchmark payload TSV/CSV 변환 스크립트 추가

### 목표

여러 번 축적한 `BENCHMARK ... STATS ...` 한 줄 payload를 spreadsheet나 비교 도구에서 읽기 쉬운 표 형태로 변환합니다.

이번 단계에서는 C++ benchmark runner의 출력 형식은 유지하고, 별도 Bash/AWK 스크립트를 추가합니다. 기본 출력은 TSV이며, 필요하면 CSV를 선택할 수 있게 합니다.

### 입력

Benchmark 결과 파일:

```bash
./build/mini_ats --benchmark --iterations 2 --output /tmp/mini_ats_benchmark_table.log
```

변환 명령:

```bash
./scripts/benchmark_to_table.sh /tmp/mini_ats_benchmark_table.log
./scripts/benchmark_to_table.sh --format csv /tmp/mini_ats_benchmark_table.log
./scripts/benchmark_to_table.sh --no-header /tmp/mini_ats_benchmark_table.log
```

### 변경 내용

- `scripts/benchmark_to_table.sh` 추가
- `--format tsv|csv` 옵션 추가
- `--no-header` 옵션 추가
- 파일 인자가 없으면 stdin을 읽도록 구성
- `BENCHMARK`/`STATS` marker를 제외하고 known `key=value` field를 stable column order로 출력
- README, architecture, benchmark 문서에 변환 스크립트 사용법 반영

### 실행 명령

```bash
chmod +x scripts/benchmark_to_table.sh
./build/mini_ats --benchmark --iterations 2 --output /tmp/mini_ats_benchmark_table.log
./scripts/benchmark_to_table.sh /tmp/mini_ats_benchmark_table.log
./scripts/benchmark_to_table.sh --format csv /tmp/mini_ats_benchmark_table.log
./scripts/benchmark_to_table.sh --no-header /tmp/mini_ats_benchmark_table.log
./scripts/benchmark_to_table.sh --format bad /tmp/mini_ats_benchmark_table.log
```

### 출력

`./scripts/benchmark_to_table.sh /tmp/mini_ats_benchmark_table.log`

```text
scenario	iterations	commands	elapsed_ns	commands_per_second_floor	compiler	cpp_standard	build_mode	os	architecture	hardware_threads	commands_received	commands_accepted	commands_rejected	trades	traded_quantity	traded_notional	vwap_notional	vwap_quantity	vwap_floor_price	latency_samples	latency_min_ns	latency_max_ns	latency_p50_ns	latency_p95_ns	latency_p99_ns
deterministic_gateway	2	6	<runtime>	<throughput>	gcc-13.3.0	202002	debug	linux	x86_64	28	6	4	2	2	6	442200	442200	6	73700	6	<runtime>	<runtime>	<runtime>	<runtime>	<runtime>
```

`./scripts/benchmark_to_table.sh --format csv /tmp/mini_ats_benchmark_table.log`

```text
scenario,iterations,commands,elapsed_ns,commands_per_second_floor,compiler,cpp_standard,build_mode,os,architecture,hardware_threads,commands_received,commands_accepted,commands_rejected,trades,traded_quantity,traded_notional,vwap_notional,vwap_quantity,vwap_floor_price,latency_samples,latency_min_ns,latency_max_ns,latency_p50_ns,latency_p95_ns,latency_p99_ns
deterministic_gateway,2,6,<runtime>,<throughput>,gcc-13.3.0,202002,debug,linux,x86_64,28,6,4,2,2,6,442200,442200,6,73700,6,<runtime>,<runtime>,<runtime>,<runtime>,<runtime>
```

`./scripts/benchmark_to_table.sh --format bad /tmp/mini_ats_benchmark_table.log`

```text
invalid format: bad
```

### 결과 판단

성공입니다.

- benchmark 결과 파일을 TSV와 CSV로 변환할 수 있음
- header 포함/제외를 선택할 수 있음
- stable column order를 유지하므로 여러 실행 결과를 append한 파일도 비교하기 쉬움
- 잘못된 format 옵션은 명시적 오류로 거부함

### 다음 단계

다음 단계에서는 실제 PostgreSQL smoke 또는 benchmark 결과 분석 경로를 더 보강합니다.

- `psql`/PostgreSQL 설치 환경에서 `--load-instrument` 성공 smoke 실행
- DB 기준정보를 주입한 stdin/TCP gateway end-to-end smoke 실행
- benchmark TSV/CSV를 기준으로 간단한 summary script 추가
- libpq 기반 connection adapter 검토

## 2026-06-11 28단계: Benchmark table summary 스크립트 추가

### 목표

여러 번 축적한 benchmark 결과를 TSV/CSV table로 변환한 뒤, 전체 실행 row를 한 줄 summary로 집계합니다.

이번 단계에서는 기존 `BENCHMARK ... STATS ...` payload 형식과 `benchmark_to_table.sh`는 유지하고, 변환된 table을 입력으로 받는 별도 summary 스크립트를 추가합니다.

### 입력

Benchmark 결과 파일:

```bash
./build/mini_ats --benchmark --iterations 2 --output /tmp/mini_ats_benchmark_summary.log
./build/mini_ats --benchmark --iterations 3 --output /tmp/mini_ats_benchmark_summary.log
```

TSV summary:

```bash
./scripts/benchmark_to_table.sh /tmp/mini_ats_benchmark_summary.log > /tmp/mini_ats_benchmark_summary.tsv
./scripts/summarize_benchmark_table.sh /tmp/mini_ats_benchmark_summary.tsv
```

CSV summary:

```bash
./scripts/benchmark_to_table.sh --format csv /tmp/mini_ats_benchmark_summary.log > /tmp/mini_ats_benchmark_summary.csv
./scripts/summarize_benchmark_table.sh --format csv /tmp/mini_ats_benchmark_summary.csv
```

### 변경 내용

- `scripts/summarize_benchmark_table.sh` 추가
- `--format tsv|csv` 옵션 추가
- `--no-header` 옵션 추가
- header가 있는 table은 column name 기준으로 읽고, `--no-header` 입력은 기존 stable column order 기준으로 읽도록 구성
- 여러 row의 iterations, commands, elapsed time, trade count, 거래량, 거래대금, latency sample 수를 합산
- throughput floor의 min/max/평균 floor와 row별 latency percentile 평균 floor 출력
- README, architecture, benchmark 문서에 summary 스크립트 사용법 반영

### 실행 명령

```bash
cmake --build build
./scripts/summarize_benchmark_table.sh --help
./build/mini_ats --benchmark --iterations 2 --output /tmp/mini_ats_benchmark_summary.log
./scripts/benchmark_to_table.sh /tmp/mini_ats_benchmark_summary.log > /tmp/mini_ats_benchmark_summary.tsv
./scripts/benchmark_to_table.sh --format csv /tmp/mini_ats_benchmark_summary.log > /tmp/mini_ats_benchmark_summary.csv
./scripts/benchmark_to_table.sh --no-header /tmp/mini_ats_benchmark_summary.log > /tmp/mini_ats_benchmark_summary_no_header.tsv
./scripts/summarize_benchmark_table.sh /tmp/mini_ats_benchmark_summary.tsv
./scripts/summarize_benchmark_table.sh --format csv /tmp/mini_ats_benchmark_summary.csv
./scripts/summarize_benchmark_table.sh --no-header /tmp/mini_ats_benchmark_summary_no_header.tsv
./scripts/summarize_benchmark_table.sh --format bad
./build/mini_ats --benchmark --iterations 3 --output /tmp/mini_ats_benchmark_summary.log
./scripts/benchmark_to_table.sh /tmp/mini_ats_benchmark_summary.log > /tmp/mini_ats_benchmark_summary.tsv
./scripts/summarize_benchmark_table.sh /tmp/mini_ats_benchmark_summary.tsv
./build/mini_ats_unit_tests
```

### 출력

`./scripts/summarize_benchmark_table.sh --help`

```text
Usage:
  ./scripts/summarize_benchmark_table.sh [--format tsv|csv] [--no-header] [file...]

Reads benchmark table rows from benchmark_to_table.sh and writes one aggregate summary.
```

단일 row TSV/CSV/header 없는 TSV summary는 모두 같은 형태로 출력되었습니다.

```text
SUMMARY rows=1 scenario=deterministic_gateway iterations_total=2 commands_total=6 elapsed_ns_total=1342812 commands_per_second_floor_min=4468 commands_per_second_floor_max=4468 commands_per_second_floor_avg_floor=4468 trades_total=2 traded_quantity_total=6 traded_notional_total=442200 vwap_floor_price=73700 latency_samples_total=6 latency_p50_ns_avg_floor=8277 latency_p95_ns_avg_floor=829622 latency_p99_ns_avg_floor=829622
```

잘못된 format 옵션:

```text
invalid format: bad
```

두 row 누적 summary:

```text
SUMMARY rows=2 scenario=deterministic_gateway iterations_total=5 commands_total=15 elapsed_ns_total=1809913 commands_per_second_floor_min=4468 commands_per_second_floor_max=19267 commands_per_second_floor_avg_floor=11867 trades_total=5 traded_quantity_total=15 traded_notional_total=1105500 vwap_floor_price=73700 latency_samples_total=15 latency_p50_ns_avg_floor=8265 latency_p95_ns_avg_floor=599386 latency_p99_ns_avg_floor=599386
```

`./build/mini_ats_unit_tests`

```text
[==========] Running 92 tests from 14 test suites.
[  PASSED  ] 92 tests.
```

### 결과 판단

성공입니다.

- 기존 benchmark payload를 변경하지 않고 별도 table summary 경로를 추가함
- TSV, CSV, header 없는 TSV 입력을 모두 처리함
- 여러 benchmark 실행 row의 총 command/trade/notional과 throughput min/max/평균을 한 줄로 확인할 수 있음
- latency percentile summary는 row별 percentile 값을 단순 평균한 값임을 문서에 명시함
- 기존 C++ unit test 92개는 계속 통과함

### 다음 단계

다음 단계에서는 실제 PostgreSQL smoke 또는 DB connection 경계를 확장합니다.

- `psql`/PostgreSQL 설치 환경에서 `--load-instrument` 성공 smoke 실행
- DB 기준정보를 주입한 stdin/TCP gateway end-to-end smoke 실행
- libpq 기반 connection adapter 검토

## 2026-06-11 29단계: 포트폴리오 완성도 정리

### 목표

기능 추가보다 포트폴리오 검토 경험을 정리합니다.

지원 직무와 프로젝트의 연결, 현재 구현 범위, 검증 방법, 의도적으로 남긴 한계를 README 첫 화면에서 바로 파악할 수 있게 하고, 검토자가 볼 코드와 테스트를 별도 문서로 안내합니다.

### 입력

정리 기준:

- 자동매매/예측/수익률 프로젝트가 아니라 매매체결 시스템 프로젝트임을 명확히 드러냄
- 넥스트레이드 매매체결 IT 시스템 직무와 연결되는 역량을 상단에 배치
- 빌드/테스트/간단 실행/benchmark 확인 경로를 짧게 유지
- 실제 PostgreSQL smoke와 libpq adapter처럼 아직 남은 범위는 숨기지 않고 명시

### 변경 내용

- README를 포트폴리오 검토 흐름 중심으로 재구성
- 직무 연결 표 추가
- 현재 구현 범위와 의도적으로 남긴 범위 분리
- quick verification, 실행 예시, PostgreSQL 기준정보, benchmark, 문서 링크 정리
- `docs/reviewer-guide.md` 추가
- `docs/matching-rules.md`의 오래된 1단계 표현 정리
- `docs/architecture.md`의 모듈 제목 정리
- `.gitignore` 추가/정리
- GitHub Actions CMake build/test workflow 추가

### 실행 명령

```bash
git diff --check
./scripts/run_tests.sh
```

### 출력

`git diff --check`

```text
<no output>
```

`./scripts/run_tests.sh`

```text
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
[100%] Built target mini_ats_unit_tests
100% tests passed, 0 tests failed out of 92

The following tests did not run:
	 33 - MarketDataPublisherTest.PublishesSingleDatagramOverLoopback (Skipped)
	 85 - TcpOrderServerTest.ProcessesLineDelimitedCommandsOverLoopback (Skipped)
```

### 결과 판단

성공입니다.

- README 첫 화면에서 프로젝트 목적, 직무 연결, 구현 범위, 한계가 분리되어 보임
- 검토자 가이드에서 빠른 검증 명령, 핵심 코드, 핵심 테스트를 바로 찾을 수 있음
- Docker 없이 Linux/CMake/GoogleTest 기준 CI workflow를 추가함
- 빌드와 CTest 검증이 통과함

### 다음 단계

다음 단계에서는 실제 실행 환경 검증을 보강합니다.

- `psql`/PostgreSQL 설치 환경에서 기준정보 load smoke
- DB 기준정보 주입 gateway smoke
- 필요 시 mock client/load generator를 작게 추가
