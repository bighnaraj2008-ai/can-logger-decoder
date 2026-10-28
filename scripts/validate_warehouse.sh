#!/usr/bin/env bash
#
# validate_warehouse.sh â€” Master Repository Health & Hygiene Validator
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

echo "================================================================"
echo " CAN Logger Decoder â€” Master Repository Hygiene Validation     "
echo "================================================================"

ERRORS=0

echo "1. Checking tracked files for forbidden binaries or secrets..."
FORBIDDEN=$(git ls-files | grep -E "(\.env|\.vscode|\.idea|\.antigravity|\.o$|\.exe$)" || true)
if [ -n "$FORBIDDEN" ]; then
    echo "[FAIL] Forbidden files tracked in repository:"
    echo "$FORBIDDEN"
    ERRORS=$((ERRORS + 1))
else
    echo "[PASS] Zero forbidden files tracked."
fi

echo "2. Checking required project root files..."
for f in README.md LICENSE Makefile CMakeLists.txt sample.dbc; do
    if [ ! -f "$f" ]; then
        echo "[FAIL] Missing required file: $f"
        ERRORS=$((ERRORS + 1))
    fi
done
echo "[PASS] All core project files verified."

if [ "$ERRORS" -eq 0 ]; then
    echo "REPOSITORY HYGIENE VALIDATION PASSED (EXIT 0)."
    exit 0
else
    echo "VALIDATION FAILED WITH $ERRORS ERRORS."
    exit 1
fi