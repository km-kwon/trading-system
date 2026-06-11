#!/usr/bin/env bash
set -euo pipefail

format="tsv"
header="yes"
inputs=()

usage() {
  cat <<'USAGE'
Usage:
  ./scripts/summarize_benchmark_table.sh [--format tsv|csv] [--no-header] [file...]

Reads benchmark table rows from benchmark_to_table.sh and writes one aggregate summary.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --format)
      if [[ $# -lt 2 ]]; then
        usage >&2
        exit 1
      fi
      format="$2"
      shift 2
      ;;
    --no-header)
      header="no"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      while [[ $# -gt 0 ]]; do
        inputs+=("$1")
        shift
      done
      ;;
    -*)
      usage >&2
      exit 1
      ;;
    *)
      inputs+=("$1")
      shift
      ;;
  esac
done

if [[ "${format}" != "tsv" && "${format}" != "csv" ]]; then
  echo "invalid format: ${format}" >&2
  exit 1
fi

if [[ ${#inputs[@]} -eq 0 ]]; then
  inputs=("-")
fi

awk -v format="${format}" -v header="${header}" '
BEGIN {
  FS = format == "csv" ? "," : "\t"
  split("scenario iterations commands elapsed_ns commands_per_second_floor compiler cpp_standard build_mode os architecture hardware_threads commands_received commands_accepted commands_rejected trades traded_quantity traded_notional vwap_notional vwap_quantity vwap_floor_price latency_samples latency_min_ns latency_max_ns latency_p50_ns latency_p95_ns latency_p99_ns", stable_columns, " ")
  split("scenario iterations commands elapsed_ns commands_per_second_floor trades traded_quantity traded_notional latency_samples latency_p50_ns latency_p95_ns latency_p99_ns", required_columns, " ")

  if (header == "no") {
    load_stable_columns()
  }
}

function clean(value) {
  gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
  gsub(/\r$/, "", value)
  if (format == "csv" && value ~ /^".*"$/) {
    value = substr(value, 2, length(value) - 2)
    gsub(/""/, "\"", value)
  }
  return value
}

function load_stable_columns(idx) {
  delete column_index_by_name
  for (idx = 1; idx in stable_columns; ++idx) {
    column_index_by_name[stable_columns[idx]] = idx
  }
  header_loaded = 1
  validate_required_columns()
}

function load_header(idx, name) {
  delete column_index_by_name
  for (idx = 1; idx <= NF; ++idx) {
    name = clean($idx)
    column_index_by_name[name] = idx
  }
  header_loaded = 1
  validate_required_columns()
}

function validate_required_columns(idx, name) {
  for (idx = 1; idx in required_columns; ++idx) {
    name = required_columns[idx]
    if (!(name in column_index_by_name)) {
      print "missing column: " name > "/dev/stderr"
      exit 1
    }
  }
}

function text_column(name) {
  return clean($(column_index_by_name[name]))
}

function numeric_column(name) {
  return text_column(name) + 0
}

/^[[:space:]]*$/ { next }
/^[[:space:]]*#/ { next }

{
  if (!header_loaded && header == "yes") {
    load_header()
    next
  }

  ++rows

  scenario = text_column("scenario")
  if (rows == 1) {
    first_scenario = scenario
  } else if (scenario != first_scenario) {
    mixed_scenarios = 1
  }

  iterations_total += numeric_column("iterations")
  commands_total += numeric_column("commands")
  elapsed_ns_total += numeric_column("elapsed_ns")
  trades_total += numeric_column("trades")
  traded_quantity_total += numeric_column("traded_quantity")
  traded_notional_total += numeric_column("traded_notional")
  latency_samples_total += numeric_column("latency_samples")

  throughput = numeric_column("commands_per_second_floor")
  throughput_sum += throughput
  if (rows == 1 || throughput < throughput_min) {
    throughput_min = throughput
  }
  if (rows == 1 || throughput > throughput_max) {
    throughput_max = throughput
  }

  latency_p50_sum += numeric_column("latency_p50_ns")
  latency_p95_sum += numeric_column("latency_p95_ns")
  latency_p99_sum += numeric_column("latency_p99_ns")
}

END {
  if (rows == 0) {
    print "no benchmark rows" > "/dev/stderr"
    exit 1
  }

  scenario_summary = mixed_scenarios ? "mixed" : first_scenario
  vwap_floor_price = traded_quantity_total > 0 ? int(traded_notional_total / traded_quantity_total) : 0

  printf "SUMMARY rows=%d", rows
  printf " scenario=%s", scenario_summary
  printf " iterations_total=%.0f", iterations_total
  printf " commands_total=%.0f", commands_total
  printf " elapsed_ns_total=%.0f", elapsed_ns_total
  printf " commands_per_second_floor_min=%.0f", throughput_min
  printf " commands_per_second_floor_max=%.0f", throughput_max
  printf " commands_per_second_floor_avg_floor=%.0f", int(throughput_sum / rows)
  printf " trades_total=%.0f", trades_total
  printf " traded_quantity_total=%.0f", traded_quantity_total
  printf " traded_notional_total=%.0f", traded_notional_total
  printf " vwap_floor_price=%.0f", vwap_floor_price
  printf " latency_samples_total=%.0f", latency_samples_total
  printf " latency_p50_ns_avg_floor=%.0f", int(latency_p50_sum / rows)
  printf " latency_p95_ns_avg_floor=%.0f", int(latency_p95_sum / rows)
  printf " latency_p99_ns_avg_floor=%.0f\n", int(latency_p99_sum / rows)
}
' "${inputs[@]}"
