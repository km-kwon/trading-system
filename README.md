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

현재는 1단계로, 빌드 가능한 프로젝트 골격만 구성했습니다.

- CMake 기반 C++20 프로젝트
- GoogleTest 세팅
- 기본 디렉터리 구조
- 최소 domain stub
- 최소 application entry point
- 샘플 unit test 1개
- 초기 문서 placeholder

아직 matching logic은 의도적으로 구현하지 않았습니다. 다음 단계에서 domain model, OrderBook, MatchingEngine 순서로 확장할 예정입니다.

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

## 현재 실행

```bash
./build/mini_ats
```

또는 빌드 후 실행까지 한 번에 처리하려면 다음 스크립트를 사용할 수 있습니다.

```bash
./run_cpp.sh
```

## 개발 기록

단계별 구현 후에는 입력, 실행 명령, 출력, 결과 판단을 [개발 기록](docs/development-log.md)에 남깁니다.

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
│   ├── marketdata/
│   └── stats/
├── src/
│   ├── domain/
│   ├── engine/
│   ├── gateway/
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
