# 아키텍처

Mini ATS Matching System은 결정적인 matching core를 중심으로 구성합니다. 같은 순서의 주문 입력이 들어오면 항상 같은 체결 결과가 나와야 하며, 이 특성을 기준으로 모듈 경계를 나눕니다.

## 예정 모듈

- `domain`: 정수 기반 가격/수량 타입, 주문, 체결, 이벤트 모델
- `engine`: in-memory order book과 가격-시간 우선 matching engine
- `gateway`: TCP 기반 주문 접수 서버
- `marketdata`: UDP 기반 체결/호가 이벤트 분배기
- `stats`: 거래량, 거래대금, VWAP, latency 통계

## 1단계 범위

현재 단계에서는 빌드 가능한 프로젝트 골격과 최소 domain stub만 제공합니다. 실제 주문장, 체결 알고리즘, 네트워크 프로토콜, PostgreSQL 연동은 이후 단계에서 순서대로 추가합니다.

## 설계 방향

- matching core는 외부 I/O와 분리합니다.
- 주문 입력 순서는 명시적인 sequence로 관리합니다.
- 가격과 수량은 `double`이 아닌 정수 타입으로 표현합니다.
- PostgreSQL 기준정보와 in-memory matching state를 분리합니다.
- TCP/UDP gateway는 core가 안정된 뒤 얇은 adapter로 붙입니다.
