#!/usr/bin/env bash
#
# clean_cache.sh â€” Workflow cache and build object cleaning utility
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Cleaning build artifacts, caches, and temporary files..."

rm -rf "${REPO_ROOT}/build"
rm -rf "${REPO_ROOT}/bin"
rm -rf "${REPO_ROOT}/release_*"
rm -f "${REPO_ROOT}"/*.o "${REPO_ROOT}/src"/*.o "${REPO_ROOT}/src"/*/*.o
rm -f "${REPO_ROOT}"/*.log "${REPO_ROOT}"/*.csv
rm -f "${REPO_ROOT}/can_decoder" "${REPO_ROOT}/can_decoder.exe"
rm -f "${REPO_ROOT}/test_suite" "${REPO_ROOT}/test_suite.exe"

echo "Cache cleanup completed successfully."