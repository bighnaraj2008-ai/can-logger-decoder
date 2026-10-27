#!/usr/bin/env bash
#
# check_formatting.sh â€” Local clang-format verification helper for can-logger-decoder (Day 4)
#
# Runs clang-format in dry-run mode over src/, include/, and tests/.
# Exits non-zero if any file does not conform to .clang-format.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_GLOBS=(
  "src/**/*.c"
  "src/*.c"
  "include/*.h"
  "tests/**/*.c"
  "tests/*.c"
)

STATUS=0

echo "================================================================"
echo " can-logger-decoder â€” Format Check (clang-format --dry-run)"
echo "================================================================"
echo "Repo root: ${REPO_ROOT}"
echo ""

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is not installed."
  echo "Install it (e.g. sudo apt-get install clang-format) and re-run."
  exit 1
fi

echo "Checking formatting against .clang-format..."
echo ""

for pattern in "${SRC_GLOBS[@]}"; do
  # shellcheck disable=SC2086
  files=$(find "${REPO_ROOT}" -path "${REPO_ROOT}/.git" -prune -o -name "${pattern##*/}" -print 2>/dev/null || true)
  # Use clang-format directly on matched files under src/, include/, tests/
  while IFS= read -r f; do
    [ -z "${f}" ] && continue
    case "${f}" in
      "${REPO_SRC}"/src/*|"${REPO_ROOT}"/include/*|"${REPO_ROOT}"/tests/*)
        ;;
      *)
        continue
        ;;
    esac
    if ! clang-format --dry-run --Werror "${f}" >/dev/null 2>&1; then
      echo "  NEEDS FORMATTING: ${f}"
      STATUS=1
    fi
  done <<< "${files}"
done

# Fallback: direct directory-based check if find returned nothing
if [ "${STATUS}" -eq 0 ]; then
  if ! clang-format --dry-run --Werror \
      src/core/*.c src/hardware/*.c src/network/*.c src/state/*.c src/utils/*.c \
      src/dbc_decoder.c src/main.c \
      include/*.h \
      tests/*.c tests/edge_cases/*.c tests/fixtures/*.c tests/harness/*.c tests/integration/*.c tests/memory/*.c tests/performance/*.c tests/stress/*.c \
      >/dev/null 2>&1; then
    echo "  One or more files need formatting (see clang-format output above)."
    STATUS=1
  fi
fi

echo ""
if [ "${STATUS}" -eq 0 ]; then
  echo "================================================================"
  echo "  Formatting check PASSED"
  echo "================================================================"
  exit 0
else
  echo "================================================================"
  echo "  Formatting check FAILED â€” run: clang-format -i <files>"
  echo "================================================================"
  exit 1
fi
