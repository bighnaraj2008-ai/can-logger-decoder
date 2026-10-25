#!/usr/bin/env bash
# ==============================================================================
# CAN Logger & Decoder - Automated Test Runner (Day 2 Enhanced)
# ==============================================================================
# Ingests test fixtures, builds every test binary, runs each suite, parses the
# [PASS]/[FAIL] assertion lines emitted by each binary, and emits a structured
# diagnostic summary with per-suite assertion pass/fail rates.
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

# --------------------------------------------------------------------------
# Test fixtures ingested by the harness (relative to repo root).
# --------------------------------------------------------------------------
FIXTURE_DIR="tests/fixtures"
RAW_FRAMES="${FIXTURE_DIR}/raw_frames.bin"
CORRUPTED_HEX="${FIXTURE_DIR}/corrupted_frames.hex"
EXTENDED_FRAMES="${FIXTURE_DIR}/extended_frames.bin"
BUS_OFF_HEX="${FIXTURE_DIR}/bus_off_sequence.hex"

# Every standalone test binary
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

# Aggregate counters
TOTAL_SUITES=0
SUITES_PASSED=0
SUITES_FAILED=0
TOTAL_ASSERTIONS=0
TOTAL_ASSERT_PASS=0
TOTAL_ASSERT_FAIL=0

declare -A SUITE_ASSERT_PASS 2>/dev/null || true
declare -A SUITE_ASSERT_FAIL 2>/dev/null || true

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}   CAN LOGGER & DECODER - AUTOMATED UNIT TEST SUITE RUNNER       ${NC}"
echo -e "${CYAN}================================================================${NC}"

# --------------------------------------------------------------------------
# 1. Build the library and every test binary.
# --------------------------------------------------------------------------
echo -e "\n--> Building test binaries..."
if command -v make >/dev/null 2>&1; then
    make clean >/dev/null 2>&1 || true
    if ! make all >/dev/null 2>&1; then
        echo -e "${RED}[FAIL] Build failed (make all).${NC}"
        exit 1
    fi
    for t in "${TESTS[@]}"; do
        if ! make "build/${t}" >/dev/null 2>&1; then
            echo -e "${RED}[FAIL] Failed to build test binary: ${t}${NC}"
            exit 1
        fi
    done
    echo -e "${GREEN}[OK] Build complete.${NC}"
else
    echo -e "${YELLOW}[INFO] make not found in PATH; assuming test binaries are pre-built.${NC}"
fi

# --------------------------------------------------------------------------
# 2. Fixture pre-flight: the fixture-dependent suites require these files.
# --------------------------------------------------------------------------
echo -e "\n--> Checking test fixtures..."
for f in "${RAW_FRAMES}" "${CORRUPTED_HEX}" "${EXTENDED_FRAMES}" "${BUS_OFF_HEX}"; do
    if [[ ! -f "${f}" ]]; then
        echo -e "${RED}[FAIL] Missing test fixture: ${f}${NC}"
        exit 1
    fi
done
echo -e "${GREEN}[OK] Fixtures present and verified.${NC}"

# --------------------------------------------------------------------------
# 3. Run each suite, capturing assertion-level pass/fail rates.
# --------------------------------------------------------------------------
for test_name in "${TESTS[@]}"; do
    bin_path="build/${test_name}"
    if [[ -f "${bin_path}.exe" ]]; then
        bin_path="${bin_path}.exe"
    fi

    TOTAL_SUITES=$((TOTAL_SUITES + 1))

    if [[ ! -x "${bin_path}" && ! -f "${bin_path}" ]]; then
        echo -e "\n--> ${YELLOW}[SKIP] ${test_name}: binary not built${NC}"
        SUITES_FAILED=$((SUITES_FAILED + 1))
        continue
    fi

    echo -e "\n--> Running ${test_name}..."
    out_file="$(mktemp 2>/dev/null || echo "test_${test_name}.tmp")"
    suite_rc=0
    if "${bin_path}" >"${out_file}" 2>&1; then
        suite_rc=0
    else
        suite_rc=$?
    fi

    apass=$(grep -c '\[PASS\]' "${out_file}" 2>/dev/null || echo "0")
    afail=$(grep -c '\[FAIL\]' "${out_file}" 2>/dev/null || echo "0")
    rm -f "${out_file}"

    TOTAL_ASSERT_PASS=$((TOTAL_ASSERT_PASS + apass))
    TOTAL_ASSERT_FAIL=$((TOTAL_ASSERT_FAIL + afail))
    TOTAL_ASSERTIONS=$((TOTAL_ASSERTIONS + apass + afail))

    if [[ ${suite_rc} -eq 0 && ${afail} -eq 0 ]]; then
        echo -e "${GREEN}[PASS] ${test_name}${NC}"
        SUITES_PASSED=$((SUITES_PASSED + 1))
    else
        echo -e "${RED}[FAIL] ${test_name}${NC} (exit=${suite_rc}, assertion_failures=${afail})"
        SUITES_FAILED=$((SUITES_FAILED + 1))
    fi
done

# --------------------------------------------------------------------------
# 4. Structured diagnostic summary.
# --------------------------------------------------------------------------
assert_rate=0
if [[ ${TOTAL_ASSERTIONS} -gt 0 ]]; then
    assert_rate=$((TOTAL_ASSERT_PASS * 100 / TOTAL_ASSERTIONS))
fi

echo -e "\n${CYAN}================================================================${NC}"
echo -e "                    STRUCTURED DIAGNOSTIC SUMMARY"
echo -e "${CYAN}================================================================${NC}"
echo -e "root_dir             : ${ROOT_DIR}"
echo -e "fixture_dir          : ${FIXTURE_DIR}"
echo -e "total_suites         : ${TOTAL_SUITES}"
echo -e "suites_passed        : ${SUITES_PASSED}"
echo -e "suites_failed        : ${SUITES_FAILED}"
echo -e "total_assertions     : ${TOTAL_ASSERTIONS}"
echo -e "assertions_passed    : ${TOTAL_ASSERT_PASS}"
echo -e "assertions_failed    : ${TOTAL_ASSERT_FAIL}"
echo -e "assert_pass_rate_pct : ${assert_rate}"
echo -e "${CYAN}================================================================${NC}"

if [[ ${SUITES_FAILED} -ne 0 || ${TOTAL_ASSERT_FAIL} -ne 0 ]]; then
    echo -e "${RED}OVERALL: FAILED (non-zero exit)${NC}"
    exit 1
fi
echo -e "${GREEN}OVERALL: PASSED (exit 0)${NC}"
exit 0
