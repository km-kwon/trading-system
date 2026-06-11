# 체결 규칙

이 문서는 Mini ATS Matching System에서 구현한 체결 규칙을 정리합니다. 구현의 기준은 기능 수보다 체결 정확성, 결정성, 테스트 가능성입니다.

## 핵심 규칙

- 가격-시간 우선 원칙을 적용합니다.
- 지정가 주문과 시장가 주문을 지원합니다.
- 주문 수량이 한 번에 모두 체결되지 않는 부분 체결을 지원합니다.
- 주문 id 기반 취소를 지원합니다.
- IOC와 FOK 주문 조건을 지원합니다.

## 가격-시간 우선 원칙

매수 주문은 더 높은 가격이 우선입니다. 같은 가격에서는 먼저 접수된 주문이 우선입니다.

매도 주문은 더 낮은 가격이 우선입니다. 같은 가격에서는 먼저 접수된 주문이 우선입니다.

## 결정성 원칙

matching core는 결정적이어야 합니다. 같은 주문 입력 스트림을 같은 순서로 replay하면 항상 같은 체결 결과가 나와야 합니다.

이를 위해 core 내부에서는 시스템 시각, 난수, thread scheduling에 따라 결과가 달라지는 처리를 피합니다.

## 주문장 잔량 관리

부분 체결 후 잔량이 남으면 `ExecutionReport`가 다시 queue에 들어가는 것이 아니라, 주문장 내부의 `BookOrder.remaining_quantity`가 감소한 상태로 같은 가격 level FIFO queue에 남습니다.

예를 들어 10주 주문이 3주 체결되면 주문장에는 같은 order id가 7주 잔량으로 유지됩니다.

단, IOC/FOK/시장가 주문은 일반 resting order처럼 주문장에 남기지 않습니다.

## 현재 구현된 체결 범위

현재 MatchingEngine은 단일 종목 기준으로 지정가 DAY, 시장가, IOC, FOK 주문을 처리합니다.

- 주문 종목 id가 MatchingEngine의 기준정보 종목 id와 다르면 거부됩니다.
- 시장 세션이 닫혀 있으면 신규 주문은 거부됩니다.
- 지정가 주문 가격은 tick size 배수여야 합니다.
- 지정가 주문 가격은 기준정보의 가격 제한폭 안에 있어야 합니다.
- 신규 매수 주문은 best ask와 비교합니다.
- 신규 매도 주문은 best bid와 비교합니다.
- 지정가 매수 가격이 best ask 이상이면 체결됩니다.
- 지정가 매도 가격이 best bid 이하이면 체결됩니다.
- 시장가 주문은 반대 side 유동성이 있으면 가격 조건 없이 best-first 순서로 체결됩니다.
- 체결 가격은 incoming 주문 가격이 아니라 resting order의 가격입니다.
- incoming 지정가 DAY 주문의 잔량은 해당 주문 가격으로 OrderBook에 저장됩니다.
- IOC 주문과 시장가 주문의 미체결 잔량은 OrderBook에 저장하지 않고 cancel report로 종료합니다.
- FOK 주문은 주문 전체 수량을 즉시 체결할 수 있을 때만 주문장을 변경합니다. 유동성이 부족하면 체결 없이 거부합니다.
- cancel request는 주문장에 남아 있는 resting order만 취소할 수 있습니다.

TCP/UDP 프로토콜과 PostgreSQL 기준정보 adapter는 matching core 밖의 얇은 경계로 분리합니다. 체결 규칙 자체는 `engine` 모듈의 순수 in-memory state 전이에 집중합니다.
