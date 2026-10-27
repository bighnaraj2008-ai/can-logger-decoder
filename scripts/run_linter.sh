#!/usr/bin/env bash
#
# run_linter.sh â€” Local static analysis helper for can-logger-decoder (Day 4)
#
# Runs cppcheck and clang-tidy against src/, include/, and tests/ so
# developers can verify code before committing. Gracefully skips tools
# that are not installed locally (e.g. clang-tidy on Windows without LLVM).
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIRS=(src include tests)

STATUS=0

echo "================================================================"
echo " can-logger-decoder â€” Local Linter (cppcheck + clang-tidy)"
echo "================================================================"
echo "Repo root: ${REPO_ROOT}"
echo ""

# ---------------------------------------------------------------------------
# cppcheck â€” catches memory leaks, uninitialized vars, null derefs, etc.
# ---------------------------------------------------------------------------
if command -v cppcheck >/dev/null 2>&1; then
  echo "[1/2] Running cppcheck (static analysis)..."
  cppcheck \
    --enable=all \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    --suppress=missingInclude \
    --suppress=constVariablePointer \
    --suppress=constVariable \
    --suppress=constParameterPointer \
    --suppress=staticFunction \
    --suppress=unusedStructMember \
    --suppress=redundantAssignment \
    --suppress=unreadVariable \
    --suppress=badBitmaskCheck \
    --suppress=variableScope \
    --suppress=clarifyCalculation \
    --suppress=invalidPrintfArgType_uint \
    --suppress=normalCheckLevelMaxBranches \
    --std=c11 \
    --language=c \
    --quiet \
    --error-exitcode=1 \
    "${SRC_DIRS[@]}" || STATUS=$?
  if [ "${STATUS}" -ne 0 ]; then
    echo "  -> cppcheck FAILED (exit code ${STATUS})"
  else
    echo "  -> cppcheck passed"
  fi
  echo ""
else
  echo "[1/2] cppcheck not installed â€” skipping (install: sudo apt-get install cppcheck)"
  echo ""
fi

# ---------------------------------------------------------------------------
# clang-tidy â€” needs compile_commands.json; skips gracefully if missing.
# ---------------------------------------------------------------------------
if command -v clang-tidy >/dev/null 2>&1; then
  echo "[2/2] Running clang-tidy (static analysis)..."
  if [ ! -f "${REPO_ROOT}/build/compile_commands.json" ] && [ ! -f "${REPO_ROOT}/compile_commands.json" ]; then
    echo "  compile_commands.json not found â€” generating via CMake..."
    cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/build" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_C_COMPILER=clang \
      >/dev/null 2>&1 || true
  fi

  if [ -f "${REPO_ROOT}/build/compile_commands.json" ]; then
    COMPILE_DB="-p ${REPO_ROOT}/build"
  elif [ -f "${REPO_ROOT}/compile_commands.json" ]; then
    COMPILE_DB="-p ${REPO_ROOT}"
  else
    COMPILE_DB=""
    echo "  -> clang-tidy skipped (no compile_commands.json and CMake configure failed)"
  fi

  if [ -n "${COMPILE_DB}" ]; then
    # shellcheck disable=SC2086
    clang-tidy ${COMPILE_DB} --warnings-as-errors=* \
      src/core/*.c src/hardware/*.c src/network/*.c src/state/*.c src/utils/*.c \
      src/dbc_decoder.c src/main.c include/*.h tests/*.c tests/**/*.c || STATUS=$?
    if [ "${STATUS}" -ne 0 ]; then
      echo "  -> clang-tidy FAILED (exit code ${STATUS})"
    else
      echo "  -> clang-tidy passed"
    fi
  fi
  echo ""
else
  echo "[2/2] clang-tidy not installed â€” skipping (install: sudo apt-get install clang-tidy clang-16)"
  echo ""
fi

# ---------------------------------------------------------------------------
# Result
# ---------------------------------------------------------------------------
if [ "${STATUS}" -eq 0 ]; then
  echo "================================================================"
  echo "  Linter checks PASSED"
  echo "================================================================"
  exit 0
else
  echo "================================================================"
  echo "  Linter checks FAILED (exit code ${STATUS})"
  echo "================================================================"
  exit "${STATUS}"
fi
