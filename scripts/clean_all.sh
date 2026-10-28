#!/usr/bin/env bash
#
# clean_all.sh â€” Master Workspace Cleaning and Reset Utility
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Performing full workspace deep cleaning..."
rm -rf "${REPO_ROOT}/build"
rm -rf "${REPO_ROOT}/bin"
rm -rf "${REPO_ROOT}/release_*"
rm -rf "${REPO_ROOT}/exported_logs"
rm -f "${REPO_ROOT}"/*.tar.gz
rm -f "${REPO_ROOT}"/*.sha256
rm -f "${REPO_ROOT}"/*.log
rm -f "${REPO_ROOT}"/*.csv
rm -f "${REPO_ROOT}/can_decoder" "${REPO_ROOT}/can_decoder.exe"
rm -f "${REPO_ROOT}/test_suite" "${REPO_ROOT}/test_suite.exe"

echo "Deep clean complete."