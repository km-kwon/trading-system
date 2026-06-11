# 검토자 가이드

이 문서는 Mini ATS Matching System을 짧은 시간 안에 검토할 때 볼 지점을 정리합니다.

## 5분 검증 경로

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

간단한 체결 흐름:

```bash
printf '%s\n' \
  'SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY price=73700 quantity=3' \
  'SUBMIT seq=2 ref=7 order_id=11 instrument_id=1001 side=BUY type=LIMIT tif=DAY price=73700 quantity=3' \
  | ./build/mini_ats --gateway --stats
```

Benchmark:

```bash
./build/mini_ats --benchmark --iterations 1000
```

## 먼저 보면 좋은 코드

- `include/engine/order_book.hpp`, `src/engine/order_book.cpp`: 가격-시간 우선 주문장
- `include/engine/matching_engine.hpp`, `src/engine/matching_engine.cpp`: 신규 주문, 취소, IOC/FOK, 기준정보 검증
- `include/replay/replay_log.hpp`, `src/replay/replay_log.cpp`: replay validation과 결정성 경계
- `include/gateway/order_gateway.hpp`, `src/gateway/order_gateway.cpp`: parser/replay/engine 결과를 gateway response로 변환
- `include/marketdata/market_data.hpp`, `src/marketdata/market_data.cpp`: 체결/호가 이벤트 adapter
- `include/stats/operational_stats.hpp`, `src/stats/operational_stats.cpp`: VWAP과 latency percentile 집계

## 테스트에서 확인할 점

- `tests/unit/order_book_test.cpp`: 가격 level 정렬, FIFO, 부분 체결 후 잔량, 취소
- `tests/unit/matching_engine_test.cpp`: 지정가/시장가/IOC/FOK, 기준정보 reject, duplicate order id
- `tests/unit/replay_test.cpp`: sequence/reference version mismatch가 state를 변경하지 않는지
- `tests/unit/gateway_test.cpp`: parse/replay/engine reject가 gateway response로 분류되는지
- `tests/unit/market_data_test.cpp`: accepted matching result만 market data event로 변환되는지
- `tests/unit/operational_stats_test.cpp`: 정수 기반 notional/VWAP과 percentile 계산
- `tests/unit/deterministic_benchmark_test.cpp`: benchmark scenario count와 stats payload

## 설계 판단

- 가격과 수량은 `double`을 쓰지 않고 정수 기반 domain type으로 표현합니다.
- matching core는 socket, DB, file I/O를 직접 알지 않습니다.
- text protocol은 사람이 검증하기 쉬운 중간 단계입니다. binary protocol은 core correctness 이후 확장 대상으로 둡니다.
- accepted input만 replay log에 기록합니다. parser/replay/engine reject는 복구 입력 스트림에 남기지 않습니다.
- latency는 운영 시점 지표이므로 replay로 복구되는 matching state와 분리합니다.

## 현재 한계

- 실제 PostgreSQL smoke는 `psql`이 설치된 local WSL 환경에서 별도로 확인해야 합니다.
- CI는 PostgreSQL 서버를 띄우지 않고 C++ build/test만 검증합니다.
- TCP server는 skeleton 수준의 순차 client 처리입니다.
- market data payload는 text format입니다.
- 고성능 최적화보다 correctness와 설명 가능성을 우선했습니다.
