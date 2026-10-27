#!/usr/bin/env bash
#
# test_style_compliance.sh â€” Automated style compliance test runner
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

echo "================================================================"
echo " can-logger-decoder â€” Style & Whitespace Compliance Test"
echo "================================================================"

FAILURES=0

echo "1. Checking for hard tabs in source and header files..."
TAB_FILES=$(find "${REPO_ROOT}/src" "${REPO_ROOT}/include" "${REPO_ROOT}/tests" \
  -type f \( -name "*.c" -o -name "*.h" \) \
  -exec grep -l $'\t' {} + || true)

if [ -n "$TAB_FILES" ]; then
  echo "FAIL: Hard tabs found in files:"
  echo "$TAB_FILES"
  FAILURES=$((FAILURES + 1))
else
  echo "PASS: Zero hard tabs found in codebase."
fi

echo "2. Checking for trailing whitespace..."
WS_FILES=$(find "${REPO_ROOT}/src" "${REPO_ROOT}/include" "${REPO_ROOT}/tests" \
  -type f \( -name "*.c" -o -name "*.h" \) \
  -exec grep -l '[[:space:]]$' {} + || true)

if [ -n "$WS_FILES" ]; then
  echo "FAIL: Trailing whitespace detected:"
  echo "$WS_FILES"
  FAILURES=$((FAILURES + 1))
else
  echo "PASS: No trailing whitespace detected."
fi

if [ "$FAILURES" -eq 0 ]; then
  echo "ALL STYLE COMPLIANCE CHECKS PASSED."
  exit 0
else
  echo "STYLE COMPLIANCE CHECKS FAILED: $FAILURES issue(s)."
  exit 1
fi