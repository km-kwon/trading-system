# OrderBook 상태 읽기

이 문서는 `OrderBook`의 내부 상태를 텍스트 출력으로 이해하기 위한 설명입니다.

## 핵심 구조

`OrderBook`은 종목 하나의 주문장을 표현합니다.

```text
OrderBook
├── asks: 매도 주문 가격 level
└── bids: 매수 주문 가격 level
```

각 가격 level 안에는 FIFO queue가 있습니다.

```text
가격 level
└── BookOrder -> BookOrder -> BookOrder
```

`BookOrder`는 원 주문인 `Order`와 현재 잔량인 `remaining_quantity`를 함께 들고 있습니다.

## ExecutionReport와의 차이

부분 체결 후 잔량이 남으면 `ExecutionReport`가 다시 queue에 들어가지 않습니다.

주문장에 남는 것은 다음 형태의 `BookOrder`입니다.

```text
BookOrder{
  order_id=1,
  remaining_quantity=7
}
```

`ExecutionReport`는 주문 처리 결과를 외부에 알려주는 통지이고, `BookOrder`는 주문장 내부 상태입니다.

## 출력 예시

현재 데모 실행:

```bash
./build/mini_ats
```

출력:

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

## 읽는 방법

`Trades`는 MatchingEngine이 생성한 체결 결과입니다.

```text
trade#1 resting#10 incoming#20 price=73700 qty=3
```

뜻:

- 체결 id는 `1`
- 이미 주문장에 있던 주문은 `#10`
- 새로 들어와 체결을 일으킨 주문은 `#20`
- 체결 가격은 resting order 가격인 `73700`
- 체결 수량은 `3`

`ASK best-first`는 매도 주문입니다. 가장 먼저 체결될 수 있는 낮은 매도 가격부터 보여줍니다. 현재 예시에서는 매도 주문 `#10`, `#11`이 모두 체결되어 매도 주문장이 비어 있습니다.

`BID best-first`는 매수 주문입니다. 가장 먼저 체결될 수 있는 높은 매수 가격부터 보여줍니다.

```text
73800 | total=3 | #20(rem=3,seq=3)
```

뜻:

- 가격 `73800`
- 이 가격의 총 잔량 `3`
- 주문 id `20`
- 주문 id 20은 원래 10주 매수 주문이었음
- 매도 주문 `#10`과 3주, 매도 주문 `#11`과 4주가 체결되어 총 7주 체결됨
- 따라서 `10 - 7 = 3`주가 남아 `rem=3`으로 주문장에 유지됨

`->`는 같은 가격 level 안의 FIFO 순서를 의미합니다.

## 현재 상태 요약

위 출력의 주문장 상태는 다음과 같습니다.

```text
매도
(비어 있음)

매수
73800: 주문 20, 잔량 3
```

현재 best ask는 없고, best bid는 `73800`입니다.

이 상태에서 새로운 매도 주문이 `73800` 이하로 들어오면 best bid와 crossing되어 체결될 수 있습니다.
