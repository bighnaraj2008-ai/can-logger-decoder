#!/usr/bin/env bash
#
# generate_telemetry_report.sh â€” Automated Telemetry Summary Report Generator
#
set -euo pipefail

INPUT_LOG="${1:-can_log.csv}"
OUTPUT_REPORT="${2:-telemetry_summary_report.md}"

echo "Generating telemetry report from ${INPUT_LOG} -> ${OUTPUT_REPORT}..."

if [ ! -f "${INPUT_LOG}" ]; then
    echo "[WARN] Log file ${INPUT_LOG} not found, generating template report."
    cat <<EOF > "${OUTPUT_REPORT}"
# CAN Bus Telemetry Summary Report
- Status: No active recording found.
- Generated: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
EOF
    exit 0
fi

TOTAL_LINES=$(wc -l < "${INPUT_LOG}")
cat <<EOF > "${OUTPUT_REPORT}"
# CAN Bus Telemetry Summary Report
- Source Log: ${INPUT_LOG}
- Total Records: ${TOTAL_LINES}
- Generated: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
EOF

echo "Report generated successfully."