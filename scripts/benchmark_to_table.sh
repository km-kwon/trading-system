#!/usr/bin/env bash
set -euo pipefail

format="tsv"
header="yes"
inputs=()

usage() {
  cat <<'USAGE'
Usage:
  ./scripts/benchmark_to_table.sh [--format tsv|csv] [--no-header] [file...]

Reads BENCHMARK/STATS payload lines from files or stdin and writes a stable table.
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
  column_count = split("scenario iterations commands elapsed_ns commands_per_second_floor compiler cpp_standard build_mode os architecture hardware_threads commands_received commands_accepted commands_rejected trades traded_quantity traded_notional vwap_notional vwap_quantity vwap_floor_price latency_samples latency_min_ns latency_max_ns latency_p50_ns latency_p95_ns latency_p99_ns", columns, " ")
  if (header == "yes") {
    emit_row(columns)
  }
}

function output_value(value, escaped) {
  if (format == "csv") {
    escaped = value
    gsub(/"/, "\"\"", escaped)
    if (escaped ~ /[",\r\n]/) {
      return "\"" escaped "\""
    }
    return escaped
  }

  gsub(/\t/, " ", value)
  return value
}

function emit_row(values, column_index, line) {
  line = output_value(values[1])
  for (column_index = 2; column_index <= column_count; ++column_index) {
    line = line (format == "csv" ? "," : "\t") output_value(values[column_index])
  }
  print line
}

/^[[:space:]]*$/ { next }
/^[[:space:]]*#/ { next }

{
  delete values
  for (column_index = 1; column_index <= column_count; ++column_index) {
    values[column_index] = ""
  }

  for (field = 1; field <= NF; ++field) {
    if ($field == "BENCHMARK" || $field == "STATS") {
      continue
    }

    equals = index($field, "=")
    if (equals <= 1) {
      continue
    }

    key = substr($field, 1, equals - 1)
    value = substr($field, equals + 1)
    for (column_index = 1; column_index <= column_count; ++column_index) {
      if (columns[column_index] == key) {
        values[column_index] = value
        break
      }
    }
  }

  emit_row(values)
}
' "${inputs[@]}"
