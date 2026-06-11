#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build
cmake --build build

if [[ $# -eq 0 ]]; then
  ./build/mini_ats --benchmark --iterations 1000
else
  ./build/mini_ats --benchmark "$@"
fi
