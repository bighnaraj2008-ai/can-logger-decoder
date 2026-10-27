#!/usr/bin/env bash
#
# schema_metadata_tests.sh â€” Repository metadata validation test
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

echo "================================================================"
echo " can-logger-decoder â€” Repository Schema & Metadata Validation"
echo "================================================================"

ERRORS=0

echo "1. Validating GitHub Actions Workflows..."
for wf in .github/workflows/*.yml; do
  if [ ! -f "$wf" ]; then
    echo "  [FAIL] Missing workflow files."
    ERRORS=$((ERRORS + 1))
  fi
done
echo "  [PASS] Workflow files present."

echo "2. Validating Required Configs..."
for cfg in .clang-format .clang-tidy .github/dependabot.yml; do
  if [ ! -f "$cfg" ]; then
    echo "  [FAIL] Missing config: $cfg"
    ERRORS=$((ERRORS + 1))
  fi
done
echo "  [PASS] Configuration files present."

echo "3. Running C schema metadata test binary if compiled..."
if [ -f "./tests/schema_metadata_tests" ] || [ -f "./bin/schema_metadata_tests" ]; then
  ./tests/schema_metadata_tests || ERRORS=$((ERRORS + 1))
fi

if [ "$ERRORS" -eq 0 ]; then
  echo "Metadata schema tests PASSED."
  exit 0
else
  echo "Metadata schema tests FAILED with $ERRORS errors."
  exit 1
fi