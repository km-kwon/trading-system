# 프로토콜

프로토콜 상세는 matching core가 안정된 뒤 확정합니다. 현재 단계에서는 TCP 주문 접수와 UDP 이벤트 분배의 역할만 정의합니다.

## 예정 범위

- TCP 주문 접수 메시지
- UDP 체결 이벤트
- UDP 호가 이벤트
- replay 가능한 안정적인 message sequence number

## 기본 방향

주문 접수 경로는 TCP를 사용합니다. 주문 입력의 순서와 수신 결과를 명확히 관리하기 위해서입니다.

체결 이벤트와 호가 이벤트는 UDP 분배를 목표로 합니다. market data 성격의 이벤트를 core와 분리해 publish하는 구조를 연습하기 위해서입니다.

프로토콜 필드는 사람이 읽기 쉬운 text format으로 시작하고, core가 안정된 뒤 binary format을 검토합니다.
