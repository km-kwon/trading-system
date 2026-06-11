# 벤치마크

벤치마크는 core matching path와 운영 통계 model을 기준으로 추가합니다. 현재는 측정 항목과 기록 원칙을 정의했고, `stats` 모듈에서 거래량/거래대금/VWAP/latency percentile을 집계하는 순수 model, deterministic gateway benchmark runner, 결과 파일 append 옵션, 실행 환경 metadata 출력, TSV/CSV 변환 스크립트, table summary 스크립트를 구현했습니다.

## 예정 측정 항목

- 초당 처리 주문 수
- 체결 건수
- 거래량
- 거래대금
- VWAP
- p50 latency
- p95 latency
- p99 latency

## 현재 통계 model

`OperationalStatistics`는 다음 값을 snapshot으로 제공합니다.

- command received/accepted/rejected count
- trade count
- 거래량
- 거래대금
- exact VWAP (`notional / quantity`)
- latency min/max/p50/p95/p99

Latency percentile은 현재 sample vector를 정렬한 뒤 nearest-rank 방식으로 계산합니다. 이 방식은 작은 테스트 입력에서도 결과가 결정적이고, 이후 benchmark runner에서 같은 규칙을 그대로 사용할 수 있습니다.

`format_operational_statistics()`는 snapshot을 다음 형태의 한 줄 payload로 변환합니다.

```text
STATS commands_received=5 commands_accepted=3 commands_rejected=2 trades=2 traded_quantity=5 traded_notional=510 vwap_notional=510 vwap_quantity=5 vwap_floor_price=102 latency_samples=5 latency_min_ns=10000 latency_max_ns=50000 latency_p50_ns=30000 latency_p95_ns=50000 latency_p99_ns=50000
```

현재 stdin gateway runner와 TCP gateway는 `--stats` 옵션으로 command 처리 latency와 trade 집계를 기록합니다. deterministic benchmark runner는 같은 formatter를 사용해 고정 시나리오 측정 결과를 기록합니다.

## 현재 benchmark runner

```bash
./build/mini_ats --benchmark --iterations 1000
```

결과를 파일에 축적하려면 `--output <path>`를 지정합니다. 이 옵션은 stdout 출력은 유지하면서 같은 payload를 파일 끝에 한 줄로 append합니다.

```bash
./build/mini_ats --benchmark --iterations 1000 --output benchmark-results.log
```

각 iteration은 다음 command 3개로 구성됩니다.

- accepted resting sell
- accepted crossing buy
- rejected off-tick buy

따라서 iteration 1회마다 command 3개, accepted 2개, rejected 1개, trade 1개, 거래량 3, 거래대금 221100이 생성됩니다.

출력 예시:

```text
BENCHMARK scenario=deterministic_gateway iterations=2 commands=6 elapsed_ns=<runtime> commands_per_second_floor=<throughput> compiler=<compiler> cpp_standard=<standard> build_mode=<debug-or-release> os=<os> architecture=<arch> hardware_threads=<threads> STATS commands_received=6 commands_accepted=4 commands_rejected=2 trades=2 traded_quantity=6 traded_notional=442200 vwap_notional=442200 vwap_quantity=6 vwap_floor_price=73700 latency_samples=6 latency_min_ns=<runtime> latency_max_ns=<runtime> latency_p50_ns=<runtime> latency_p95_ns=<runtime> latency_p99_ns=<runtime>
```

축적한 payload를 표 형태로 바꾸려면 `scripts/benchmark_to_table.sh`를 사용합니다. 기본 출력은 TSV이고, `--format csv`를 지정하면 CSV로 출력합니다.

```bash
./scripts/benchmark_to_table.sh benchmark-results.log
./scripts/benchmark_to_table.sh --format csv benchmark-results.log
```

스크립트는 파일 인자가 없으면 stdin을 읽습니다.

여러 benchmark row의 aggregate summary가 필요하면 변환된 table을 `scripts/summarize_benchmark_table.sh`에 넘깁니다.

```bash
./scripts/benchmark_to_table.sh benchmark-results.log > benchmark-results.tsv
./scripts/summarize_benchmark_table.sh benchmark-results.tsv
```

CSV table도 읽을 수 있습니다.

```bash
./scripts/benchmark_to_table.sh --format csv benchmark-results.log > benchmark-results.csv
./scripts/summarize_benchmark_table.sh --format csv benchmark-results.csv
```

summary 출력 예시:

```text
SUMMARY rows=2 scenario=deterministic_gateway iterations_total=5 commands_total=15 elapsed_ns_total=<runtime> commands_per_second_floor_min=<throughput> commands_per_second_floor_max=<throughput> commands_per_second_floor_avg_floor=<throughput> trades_total=5 traded_quantity_total=15 traded_notional_total=1105500 vwap_floor_price=73700 latency_samples_total=15 latency_p50_ns_avg_floor=<runtime> latency_p95_ns_avg_floor=<runtime> latency_p99_ns_avg_floor=<runtime>
```

`latency_p50_ns_avg_floor`, `latency_p95_ns_avg_floor`, `latency_p99_ns_avg_floor`는 각 benchmark row가 이미 계산한 percentile 값의 단순 평균입니다. 원본 latency sample을 다시 합쳐 percentile을 재계산하는 값은 아닙니다.

## 기록 원칙

벤치마크 결과는 재현 가능해야 합니다. 결과 문서에는 다음 정보를 함께 남깁니다.

- 컴파일러 종류와 버전
- 빌드 타입
- CPU와 메모리 환경
- 주문 입력 시나리오
- 총 주문 수
- 체결 발생 비율
- 측정 시간과 반복 횟수

초기 목표는 과한 최적화가 아니라 정확한 기준선 확보입니다. lock-free 구조나 custom memory pool은 core correctness가 검증된 뒤에만 검토합니다.
