#!/usr/bin/env bash
# ==============================================================================
# CAN Logger & Decoder - Clean Test Artifacts Script
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

cd "${ROOT_DIR}"

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}   CAN LOGGER & DECODER - CLEANING TEST ARTIFACTS               ${NC}"
echo -e "${CYAN}================================================================${NC}"

# Remove build directory and compiled objects
if [[ -d "build" ]]; then
    rm -rf build
    echo "  [CLEAN] Removed build/ directory"
fi

# Remove stray object files and binaries
rm -f *.o src/*.o src/*/*.o tests/*.o tests/*/*.o
rm -f can_decoder can_decoder.exe test_suite test_suite.exe
rm -f *.gcda *.gcno *.gcov coverage.info
rm -f *.log can_log.csv can_log_*.csv

echo -e "${GREEN}Test artifacts and compiled binaries cleaned successfully.${NC}"
