#!/usr/bin/env bash
#
# format_code.sh â€” Automated source formatting fixer for can-logger-decoder (Day 4)
#
# Formats all C source and header files under src/, include/, and tests/
# in-place according to .clang-format.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "================================================================"
echo " can-logger-decoder â€” In-Place Source Formatter (clang-format)"
echo "================================================================"
echo "Repo root: ${REPO_ROOT}"
echo ""

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is not installed."
  echo "Install it (e.g. sudo apt-get install clang-format) and re-run."
  exit 1
fi

echo "Formatting source and header files in-place..."
find "${REPO_ROOT}/src" "${REPO_ROOT}/include" "${REPO_ROOT}/tests" \
  -type f \( -name "*.c" -o -name "*.h" \) \
  -exec clang-format -i {} +

echo "Formatting completed successfully."
exit 0