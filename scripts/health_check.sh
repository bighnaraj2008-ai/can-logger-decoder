#!/usr/bin/env bash
#
# health_check.sh Ã¢â‚¬â€ CI environment diagnostic health check script
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "================================================================"
echo " can-logger-decoder Ã¢â‚¬â€ Environment Diagnostic Health Check"
echo "================================================================"
echo "Host OS: $(uname -s 2>/dev/null || echo "Windows/Other")"
echo "Date: $(date 2>/dev/null || echo "N/A")"
echo ""

echo "1. Checking Core Compilers..."
for tool in gcc clang make cmake git; do
  if command -v "$tool" >/dev/null 2>&1; then
    echo "  [FOUND] $tool -> $(command -v "$tool")"
  else
    echo "  [MISSING] $tool"
  fi
done

echo ""
echo "2. Checking Linter & Analysis Tools..."
for tool in clang-format clang-tidy cppcheck actionlint valgrind; do
  if command -v "$tool" >/dev/null 2>&1; then
    echo "  [FOUND] $tool -> $(command -v "$tool")"
  else
    echo "  [OPTIONAL MISSING] $tool"
  fi
done

echo ""
echo "3. Repository Health Check:"
test -f "${REPO_ROOT}/sample.dbc" && echo "  [PASS] sample.dbc present"
test -f "${REPO_ROOT}/.clang-format" && echo "  [PASS] .clang-format present"
test -f "${REPO_ROOT}/.clang-tidy" && echo "  [PASS] .clang-tidy present"

echo ""

echo "4. Checking Storage & File Permissions..."
if [ -w "." ]; then
  echo "  [PASS] Current working directory is writable"
else
  echo "  [FAIL] Directory write permission denied"
fi

echo "Environment diagnostic check completed."