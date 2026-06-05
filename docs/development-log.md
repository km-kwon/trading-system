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
