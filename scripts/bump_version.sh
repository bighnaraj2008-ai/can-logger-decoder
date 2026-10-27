#!/usr/bin/env bash
#
# bump_version.sh â€” Release version bumping utility for can-logger-decoder
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <new-version-e.g.-2.1.0>"
  exit 1
fi

NEW_VER="$1"
echo "Bumping version to ${NEW_VER}..."

# Update CMakeLists.txt
if [ -f "${REPO_ROOT}/CMakeLists.txt" ]; then
  sed -i -E "s/project\(can_logger_decoder VERSION [0-9]+\.[0-9]+\.[0-9]+/project(can_logger_decoder VERSION ${NEW_VER}/" "${REPO_ROOT}/CMakeLists.txt" || true
fi

echo "Version bump to ${NEW_VER} complete."