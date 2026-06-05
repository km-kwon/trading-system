# 복구 설계

복구 설계는 1단계에서 구현하지 않습니다. 이 문서는 이후 replay 기반 복구 방향을 정리하기 위한 초안입니다.

## 예정 방향

- 접수 완료된 주문 입력 스트림을 저장합니다.
- 저장된 입력을 deterministic replay하여 in-memory state를 재구성합니다.
- replay 결과로 생성된 체결 이벤트와 기존 기록을 비교합니다.
- PostgreSQL 기준정보와 volatile matching state를 분리합니다.

## 핵심 전제

복구가 가능하려면 matching core가 결정적이어야 합니다. 즉, 입력 순서와 기준정보가 같으면 order book 상태와 체결 결과도 같아야 합니다.

이를 위해 주문 sequence, 기준정보 version, 체결 이벤트 sequence를 명확히 기록하는 구조를 목표로 합니다.
