# Mini ATS Matching System

Mini ATS Matching System은 거래소 내부의 매매 체결 시스템을 주제로 한 C++20 포트폴리오 프로젝트입니다.

이 프로젝트는 자동매매 봇, 주가 예측, 백테스팅 수익률 프로젝트가 아닙니다. 목표는 간소화된 ATS(Alternative Trading System)의 내부 동작을 구현하는 것입니다. 특히 주문 접수, in-memory order book, 가격-시간 우선 체결, 체결/호가 이벤트, 기준정보 관리, replay 가능한 결정적 동작을 중심으로 설계합니다.

## 프로젝트 목적

넥스트레이드 매매체결 IT 시스템 직무는 투자 전략보다 시스템 프로그래밍, 정확성, 결정성, 프로토콜 처리, 운영 관측 가능성에 더 가깝다고 판단했습니다. 이 프로젝트는 그 역량을 직접 보여주기 위해 다음 원칙을 따릅니다.

- 기능 개수보다 체결 정확성을 우선합니다.
- 같은 주문 입력을 replay하면 항상 같은 체결 결과가 나오도록 합니다.
- 가격과 수량에는 `double`을 사용하지 않고 정수 기반 타입을 사용합니다.
- matching core는 설명 가능한 단순한 C++ 코드로 작성합니다.
- Linux 환경에서 빌드/실행 가능한 CMake 프로젝트로 구성합니다.
- GoogleTest 기반 unit/integration test로 검증합니다.
- Docker가 아닌 WSL Ubuntu의 local PostgreSQL을 기준정보 저장소로 사용합니다.

## 현재 단계

현재는 프로젝트 골격, domain model, OrderBook, MatchingEngine, 기준정보 검증, PostgreSQL 기준정보 row adapter, replay input model, text command parser, replay log file I/O, gateway request/response model, response text formatter, accepted input recorder, TCP 주문 접수 skeleton, market data event model, UDP market data publisher skeleton, gateway와 publisher 연결 첫 버전, 운영 통계 model 첫 버전을 구현한 상태입니다.

- CMake 기반 C++20 프로젝트
- GoogleTest 세팅
- 기본 디렉터리 구조
- `Order`, `Trade`, `ExecutionReport`, `CancelRequest` domain model
- `InstrumentReference` 기반 tick size, 가격 제한폭, 시장 세션 model
- `BookOrder`, `OrderBook` 기반 in-memory 주문장
- 지정가 DAY, 시장가, IOC, FOK 주문 기준 가격-시간 우선 MatchingEngine
- MatchingEngine cancel request 처리
- MatchingEngine 기준정보 검증
- `reference_data` 모듈 기반 PostgreSQL instrument row mapping
- `replay` 모듈 기반 주문 입력/취소 입력 재생
- `protocol` 모듈 기반 text command parsing
- replay log stream/file reader/writer
- `gateway` 모듈 기반 주문 접수 request/response와 reject mapping
- gateway response canonical text formatter
- accepted gateway input replay log recorder
- line-delimited TCP order gateway skeleton
- `marketdata` 모듈 기반 trade/book update event model
- UDP market data publisher skeleton
- accepted gateway command 결과를 UDP market data publisher로 전달하는 연결 경계
- `stats` 모듈 기반 command count, 거래량, 거래대금, exact VWAP, latency percentile model
- PostgreSQL schema/seed/reset script
- 최소 application entry point
- domain model unit test
- order book unit test
- matching engine unit test
- 단계별 개발 기록

현재 MatchingEngine은 단일 종목 기준으로 주문 접수, 기준정보 검증, 즉시 체결, 잔량 resting, IOC 잔량 취소, FOK 사전 유동성 확인, resting order 취소를 지원합니다. PostgreSQL row를 `InstrumentReference`로 변환하는 경계, replay input model, text command parser, replay log file I/O, gateway request/response model, response text formatter, accepted input recorder, TCP order gateway skeleton, market data event model, UDP publisher skeleton, TCP gateway의 선택적 market data publish 경로, 운영 통계 model, stdin gateway stats 출력은 구현했으며, 실제 DB connection adapter와 TCP/benchmark 통계 노출은 이후 단계에서 확장할 예정입니다.

## 개발 환경

- WSL2 Ubuntu
- C++20
- CMake
- g++ 또는 clang++
- GoogleTest
- local PostgreSQL
- Docker 사용 안 함
- Windows 전용 Visual Studio `.sln` 프로젝트 사용 안 함

## 빌드

```bash
cmake -S . -B build
cmake --build build
```

## 테스트

```bash
ctest --test-dir build
```

또는 다음 스크립트를 실행할 수 있습니다.

```bash
./scripts/run_tests.sh
```

## 기준정보 DB

`MINI_ATS_DB_NAME`과 `MINI_ATS_DB_USER`를 지정하지 않으면 각각 `mini_ats`, 현재 Linux 사용자를 사용합니다.

```bash
./scripts/setup_postgres.sh
./scripts/reset_db.sh
```

현재 C++ 테스트는 PostgreSQL 서버 없이 실행됩니다. `reference_data` unit test는 DB row 모양의 값을 `InstrumentReference`로 변환하는 순수 adapter 경계를 검증합니다.

## 현재 실행

```bash
./build/mini_ats
```

text command gateway runner는 stdin에서 `SUBMIT`/`CANCEL` 한 줄 명령을 읽고 canonical gateway response를 stdout으로 출력합니다.

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway
```

accepted input만 replay log로 남기려면 `--record-log`를 함께 사용합니다.

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway --record-log accepted-input.log
```

처리 종료 시 운영 통계 snapshot을 함께 보려면 `--stats`를 추가합니다. Gateway response는 stdout, stats snapshot은 stderr로 출력합니다.

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3' \
  'SUBMIT seq=2 ref=7 order_id=11 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=3' \
  | ./build/mini_ats --gateway --stats
```

TCP skeleton은 newline으로 끝나는 text command를 받아 response text 한 줄을 돌려줍니다.

```bash
./build/mini_ats --tcp --port 9001 --record-log accepted-input.log
```

TCP gateway에서 accepted matching 결과를 UDP market data payload로 함께 publish하려면 `--market-data <addr> <port>`를 추가합니다.

```bash
./build/mini_ats --tcp --port 9001 --record-log accepted-input.log --market-data 127.0.0.1 9100
```

또는 빌드 후 실행까지 한 번에 처리하려면 다음 스크립트를 사용할 수 있습니다.

```bash
./run_cpp.sh
```

## 개발 기록

단계별 구현 후에는 입력, 실행 명령, 출력, 결과 판단을 [개발 기록](docs/development-log.md)에 남깁니다.

OrderBook 상태 출력이 헷갈릴 때는 [OrderBook 상태 읽기](docs/orderbook-state.md)를 참고합니다.

## 디렉터리 구조

```text
.
├── CMakeLists.txt
├── README.md
├── docs/
├── db/
├── scripts/
├── include/
│   ├── domain/
│   ├── engine/
│   ├── gateway/
│   ├── protocol/
│   ├── reference_data/
│   ├── replay/
│   ├── marketdata/
│   └── stats/
├── src/
│   ├── domain/
│   ├── engine/
│   ├── gateway/
│   ├── protocol/
│   ├── reference_data/
│   ├── replay/
│   ├── marketdata/
│   ├── stats/
│   └── main.cpp
├── tests/
│   ├── unit/
│   └── integration/
└── tools/
```

## 구현 로드맵

1. Domain model
2. In-memory order book
3. 가격-시간 우선 MatchingEngine
4. 지정가 주문
5. 시장가 주문
6. 주문 취소
7. 부분 체결
8. IOC/FOK 주문
9. PostgreSQL 기반 종목/시장세션/호가단위/가격제한폭 관리
10. TCP 기반 주문 접수 서버
11. UDP 기반 체결/호가 이벤트 분배
12. 거래량, 거래대금, VWAP, p50/p95/p99 latency 통계
13. GoogleTest 기반 unit/integration test 및 benchmark 문서화

## 직무 연결 포인트

이 프로젝트는 매매체결 IT 시스템에서 중요한 다음 요소를 작은 범위로 재현하는 것을 목표로 합니다.

- 주문 입력 순서에 따른 결정적 처리
- 가격-시간 우선순위 기반 체결 규칙
- 정수 기반 가격/수량 표현
- replay 가능한 테스트 구조
- TCP/UDP 프로토콜 기반 gateway와 market data 분리
- PostgreSQL 기준정보와 in-memory matching state의 역할 분리

최종적으로는 “수익률을 내는 프로그램”이 아니라 “거래소 내부에서 주문이 어떻게 관리되고 체결되는지 설명 가능한 시스템”을 만드는 것이 목표입니다.
