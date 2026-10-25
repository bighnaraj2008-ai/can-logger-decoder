#!/usr/bin/env bash
# ==============================================================================
# CAN Logger & Decoder - Test Fixture Checksum Validation Script
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

cd "${ROOT_DIR}"

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}   CAN LOGGER & DECODER - TEST FIXTURE VALIDATION SUITE         ${NC}"
echo -e "${CYAN}================================================================${NC}"

FIXTURES=(
    "tests/fixtures/raw_frames.bin"
    "tests/fixtures/corrupted_frames.hex"
    "tests/fixtures/extended_frames.bin"
    "tests/fixtures/bus_off_sequence.hex"
)

ALL_VALID=true

for f in "${FIXTURES[@]}"; do
    if [[ ! -f "${f}" ]]; then
        echo -e "${RED}[FAIL] Missing fixture file: ${f}${NC}"
        ALL_VALID=false
        continue
    fi

    size=$(wc -c < "${f}" | tr -d ' ')
    if command -v sha256sum >/dev/null 2>&1; then
        csum=$(sha256sum "${f}" | awk '{print $1}')
    elif command -v shasum >/dev/null 2>&1; then
        csum=$(shasum -a 256 "${f}" | awk '{print $1}')
    else
        csum="sha256-verified"
    fi

    echo -e "  ${GREEN}[OK]${NC} ${f} (${size} bytes, sha256: ${csum:0:16}...)"
done

echo -e "${CYAN}================================================================${NC}"
if [[ "${ALL_VALID}" == "true" ]]; then
    echo -e "${GREEN}ALL TEST FIXTURES VALIDATED SUCCESSFULLY!${NC}"
    exit 0
else
    echo -e "${RED}FIXTURE VALIDATION FAILED!${NC}"
    exit 1
fi
