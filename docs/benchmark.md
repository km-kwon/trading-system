# 벤치마크

벤치마크는 core matching path와 운영 통계 model을 기준으로 추가합니다. 현재는 측정 항목과 기록 원칙을 정의했고, `stats` 모듈에서 거래량/거래대금/VWAP/latency percentile을 집계하는 순수 model을 구현했습니다.

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

현재 stdin gateway runner는 `--stats` 옵션으로 command 처리 latency와 trade 집계를 기록한 뒤 종료 시 snapshot을 stderr에 출력합니다. 이후 benchmark runner는 같은 formatter를 사용해 시나리오별 측정 결과를 기록합니다.

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
