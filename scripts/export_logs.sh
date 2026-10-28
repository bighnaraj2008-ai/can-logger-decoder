#!/usr/bin/env bash
#
# export_logs.sh â€” CAN Log Export Utility
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

INPUT_FILE="${1:-}"
OUTPUT_PATH="${2:-./exported_logs.csv}"

if [ -z "$INPUT_FILE" ] || [ ! -f "$INPUT_FILE" ]; then
    echo "Usage: $0 <input.csv> [output.csv]"
    exit 1
fi

echo "Exporting logs from $INPUT_FILE to $OUTPUT_PATH..."
cp "$INPUT_FILE" "$OUTPUT_PATH"
echo "Export complete."