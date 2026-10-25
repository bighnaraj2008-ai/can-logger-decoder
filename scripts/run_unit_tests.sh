#!/usr/bin/env bash
# ==============================================================================
# CAN Logger & Decoder - Automated Test Runner
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}   CAN LOGGER & DECODER - AUTOMATED UNIT TEST SUITE RUNNER       ${NC}"
echo -e "${CYAN}================================================================${NC}"

cd "${ROOT_DIR}"

TESTS=(
    "test_parser"
    "test_state_machine"
    "test_vcan"
    "test_filters"
    "test_metrics"
    "test_router"
    "test_edge_cases"
    "test_fixtures"
    "test_decoder"
)

TOTAL_PASS=0
TOTAL_FAIL=0

for test_name in "${TESTS[@]}"; do
    bin_path="build/${test_name}"
    if [[ -f "${bin_path}.exe" ]]; then
        bin_path="${bin_path}.exe"
    fi

    if [[ -x "${bin_path}" || -f "${bin_path}" ]]; then
        echo -e "\n--> Running ${test_name}..."
        if "${bin_path}"; then
            echo -e "${GREEN}[PASS] ${test_name}${NC}"
            TOTAL_PASS=$((TOTAL_PASS + 1))
        else
            echo -e "${RED}[FAIL] ${test_name}${NC}"
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
        fi
    else
        echo -e "\n--> Binary for ${test_name} not yet built (skipping)."
    fi
done

echo -e "\n${CYAN}================================================================${NC}"
echo -e "Test Summary: ${GREEN}${TOTAL_PASS} Passed${NC}, ${RED}${TOTAL_FAIL} Failed${NC}"
echo -e "${CYAN}================================================================${NC}"

if [[ ${TOTAL_FAIL} -ne 0 ]]; then
    exit 1
fi
exit 0
