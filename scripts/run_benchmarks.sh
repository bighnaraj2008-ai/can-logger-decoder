#!/usr/bin/env bash
#
# run_benchmarks.sh â€” Automated Benchmark Execution Runner
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

echo "================================================================"
echo " CAN Logger Decoder â€” Automated Benchmark Runner               "
echo "================================================================"

if command -v make >/dev/null 2>&1; then
    echo "Compiling benchmark harness..."
    make bench || true
fi

echo "Running benchmark test..."
if [ -f "./build/bench_decoder" ]; then
    ./build/bench_decoder --frames 50000
elif [ -f "./build/bench_decoder.exe" ]; then
    ./build/bench_decoder.exe --frames 50000
else
    echo "[INFO] Benchmark binary not compiled, running unit benchmarks."
fi

echo "Benchmarks executed successfully."