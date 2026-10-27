#!/usr/bin/env bash
#
# test_build_matrix.sh â€” Local multi-compiler build matrix verification
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

echo "================================================================"
echo " can-logger-decoder â€” Local Build Matrix Verification"
echo "================================================================"

COMPILERS=("gcc" "clang")
TEST_STATUS=0

for cc in "${COMPILERS[@]}"; do
  if command -v "$cc" >/dev/null 2>&1; then
    echo "Testing compilation with $cc..."
    make clean >/dev/null 2>&1 || true
    if make CC="$cc" all test >/dev/null 2>&1; then
      echo "  [PASS] $cc compilation & test suite passed."
    else
      echo "  [FAIL] $cc compilation or test suite failed."
      TEST_STATUS=1
    fi
  else
    echo "  [SKIP] $cc compiler not found in local environment."
  fi
done

make clean >/dev/null 2>&1 || true

if [ "$TEST_STATUS" -eq 0 ]; then
  echo "Local matrix testing verified successfully."
  exit 0
else
  echo "Local matrix testing encountered failures."
  exit 1
fi