# Mini ATS Matching System

Mini ATS Matching System은 거래소 내부의 매매 체결 시스템을 작은 범위로 구현한 C++20 포트폴리오 프로젝트입니다.

이 프로젝트는 자동매매 봇, 주가 예측, 백테스팅 수익률 프로젝트가 아닙니다. 목표는 주문 접수, in-memory order book, 가격-시간 우선 체결, 기준정보 검증, replay 가능한 결정성, TCP/UDP adapter, 운영 통계까지 이어지는 매매체결 시스템의 핵심 경계를 설명 가능한 코드로 구현하는 것입니다.

## 직무 연결

넥스트레이드 매매체결 IT 시스템 직무는 투자 전략보다 시스템 프로그래밍, 정확성, 결정성, 프로토콜 처리, 운영 관측 가능성에 더 가깝다고 판단했습니다. 이 프로젝트는 그 역량을 다음 방식으로 보여줍니다.

| 직무 역량 | 프로젝트에서 보여주는 구현 |
| --- | --- |
| 체결 정확성 | 가격-시간 우선 `OrderBook`과 `MatchingEngine` |
| 결정성 | 명시적 input sequence와 replay validation |
| 금융 도메인 모델링 | 정수 기반 가격/수량, 체결, execution report, 기준정보 |
| 기준정보 관리 | PostgreSQL row adapter, tick size, 가격 제한폭, 시장 세션 검증 |
| 프로토콜 처리 | text command parser, canonical gateway response, TCP order server |
| 시장 데이터 경계 | 체결/호가 event model과 UDP publisher |
| 운영 관측성 | 거래량, 거래대금, exact VWAP, p50/p95/p99 latency 통계 |
| 검증 가능성 | CMake, GoogleTest, deterministic benchmark, 단계별 개발 기록 |

## 현재 구현 범위

현재 구현은 단일 종목 기준의 mini matching system입니다.

- CMake 기반 C++20 프로젝트 구조
- `Order`, `Trade`, `ExecutionReport`, `CancelRequest`, `InstrumentReference` domain model
- in-memory `OrderBook`
- 지정가 DAY, 시장가, IOC, FOK 주문 처리
- 부분 체결과 잔량 resting
- resting order 취소
- tick size, 가격 제한폭, 시장 세션, reference version 검증
- text command parser와 replay input model
- replay log stream/file reader/writer
- gateway request/response와 canonical response formatter
- accepted command replay recorder
- line-delimited TCP order server
- trade/book update market data event model
- UDP market data publisher
- command/trade/VWAP/latency statistics
- deterministic gateway benchmark runner
- benchmark payload file append, TSV/CSV 변환, summary 집계 script
- PostgreSQL schema/seed/reset script
- psql 기반 PostgreSQL instrument loader adapter
- GoogleTest 기반 unit test

아직 의도적으로 남겨둔 범위도 있습니다.

- 실제 local PostgreSQL smoke test는 `psql`이 설치된 WSL 환경에서 수행해야 합니다.
- libpq 직접 연동 adapter는 아직 구현하지 않았습니다.
- binary protocol, lock-free queue, custom memory pool 같은 고급 최적화는 core correctness 검증 이후의 확장 대상으로 남겼습니다.

## 개발 환경

- WSL2 Ubuntu
- C++20
- CMake
- g++ 또는 clang++
- GoogleTest
- local PostgreSQL
- Docker 사용 안 함
- Windows 전용 Visual Studio `.sln` 프로젝트 사용 안 함

## 빠른 검증

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

또는:

```bash
./scripts/run_tests.sh
```

현재 C++ 테스트는 PostgreSQL 서버 없이 실행됩니다. 기준정보 DB adapter는 DB row 모양의 값과 `psql` 출력 parsing 경계를 unit test로 검증합니다.

## 실행 예시

기본 실행:

```bash
./build/mini_ats
```

stdin gateway:

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3' \
  'SUBMIT seq=2 ref=7 order_id=11 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=3' \
  | ./build/mini_ats --gateway --stats
```

accepted input replay log 기록:

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway --record-log accepted-input.log
```

TCP order gateway:

```bash
./build/mini_ats --tcp --port 9001 --record-log accepted-input.log --stats
```

TCP gateway에서 UDP market data publish를 함께 켜기:

```bash
./build/mini_ats --tcp --port 9001 --record-log accepted-input.log --market-data 127.0.0.1 9100
```

## PostgreSQL 기준정보

`MINI_ATS_DB_NAME`과 `MINI_ATS_DB_USER`를 지정하지 않으면 각각 `mini_ats`, 현재 Linux 사용자를 사용합니다.

```bash
./scripts/setup_postgres.sh
./scripts/reset_db.sh
```

단일 instrument 기준정보를 DB에서 읽기:

```bash
./build/mini_ats --load-instrument --instrument-id 1001
```

stdin/TCP gateway 실행 경로에 DB 기준정보 주입:

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73500 quantity=10' \
  | ./build/mini_ats --gateway --load-reference-data --instrument-id 1001
```

```bash
./build/mini_ats --tcp --port 9001 --load-reference-data --instrument-id 1001 \
  --db-name mini_ats --db-user "$USER" --psql psql
```

이 옵션을 쓰면 text command의 `ref` 값은 DB에서 읽은 `reference_version`과 같아야 합니다.

## Benchmark

고정 gateway 시나리오 benchmark:

```bash
./build/mini_ats --benchmark --iterations 1000
```

결과 파일 append:

```bash
./build/mini_ats --benchmark --iterations 1000 --output benchmark-results.log
```

TSV/CSV 변환:

```bash
./scripts/benchmark_to_table.sh benchmark-results.log
./scripts/benchmark_to_table.sh --format csv benchmark-results.log
```

여러 benchmark row summary:

```bash
./scripts/benchmark_to_table.sh benchmark-results.log > benchmark-results.tsv
./scripts/summarize_benchmark_table.sh benchmark-results.tsv
```

빌드 후 benchmark 실행:

```bash
./scripts/run_benchmark.sh --iterations 1000
```

## 문서

- [아키텍처](docs/architecture.md)
- [체결 규칙](docs/matching-rules.md)
- [프로토콜](docs/protocol.md)
- [벤치마크](docs/benchmark.md)
- [복구/replay 설계](docs/recovery-design.md)
- [OrderBook 상태 읽기](docs/orderbook-state.md)
- [검토자 가이드](docs/reviewer-guide.md)
- [개발 기록](docs/development-log.md)

## 디렉터리 구조

```text
.
├── CMakeLists.txt
├── README.md
├── db/
├── docs/
├── include/
│   ├── benchmark/
│   ├── domain/
│   ├── engine/
│   ├── gateway/
│   ├── marketdata/
│   ├── protocol/
│   ├── reference_data/
│   ├── replay/
│   └── stats/
├── scripts/
├── src/
│   ├── benchmark/
│   ├── domain/
│   ├── engine/
│   ├── gateway/
│   ├── marketdata/
│   ├── protocol/
│   ├── reference_data/
│   ├── replay/
│   ├── stats/
│   └── main.cpp
└── tests/
    └── unit/
```

## 다음 확장 후보

1. `psql`/PostgreSQL 설치 환경에서 `--load-instrument` 성공 smoke test
2. DB 기준정보를 주입한 stdin/TCP gateway end-to-end smoke test
3. libpq 기반 connection adapter
4. integration test 디렉터리와 fixture 정리
5. 간단한 mock client와 load generator
