#!/usr/bin/env bash
# ==============================================================================
# CAN Logger & Decoder - Automated Valgrind Memory Leak Checker
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
YELLOW='\033[0;33m'
NC='\033[0m'

cd "${ROOT_DIR}"

if ! command -v valgrind >/dev/null 2>&1; then
    echo -e "${YELLOW}[SKIP] Valgrind is not installed or not in PATH.${NC}"
    exit 0
fi

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}   CAN LOGGER & DECODER - VALGRIND MEMORY SAFETY SUITE          ${NC}"
echo -e "${CYAN}================================================================${NC}"

VALGRIND_FLAGS=(
    "--leak-check=full"
    "--show-leak-kinds=all"
    "--track-origins=yes"
    "--error-exitcode=1"
    "--quiet"
)

TESTS=(
    "build/test_parser"
    "build/test_state_machine"
    "build/test_edge_cases"
    "build/test_fixtures"
)

FAILED_COUNT=0

for test_bin in "${TESTS[@]}"; do
    if [[ -x "${test_bin}" || -f "${test_bin}.exe" ]]; then
        bin_to_run="${test_bin}"
        [[ -f "${test_bin}.exe" ]] && bin_to_run="${test_bin}.exe"

        echo -e "\n--> Running Valgrind on ${bin_to_run}..."
        if valgrind "${VALGRIND_FLAGS[@]}" "${bin_to_run}" >/dev/null; then
            echo -e "${GREEN}[PASS] No memory leaks detected in ${bin_to_run}${NC}"
        else
            echo -e "${RED}[FAIL] Memory errors detected in ${bin_to_run}${NC}"
            FAILED_COUNT=$((FAILED_COUNT + 1))
        fi
    fi
done

echo -e "\n${CYAN}================================================================${NC}"
if [[ ${FAILED_COUNT} -eq 0 ]]; then
    echo -e "${GREEN}ALL VALGRIND MEMORY CHECKS PASSED CLEANLY (0 LEAKS)${NC}"
    exit 0
else
    echo -e "${RED}VALGRIND DETECTED LEAKS IN ${FAILED_COUNT} SUITES${NC}"
    exit 1
fi
