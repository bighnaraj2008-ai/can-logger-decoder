#!/usr/bin/env bash
#
# export_logs.sh â€” CAN Log Export Utility
#
# Exports decoded CAN logs (CSV) into structured CSV or JSON archives for
# offline analysis. Supports filtering by time range, CAN ID, and message
# name, with optional tarball packaging and SHA-256 checksum generation.
#
# Usage:
#   ./scripts/export_logs.sh <input.csv> [OPTIONS]
#
# Options:
#   --format csv|json     Output format (default: csv)
#   --output PATH         Output file/directory path (default: ./exported_logs)
#   --filter-id HEX       Include only frames matching this CAN ID (hex, e.g. 0x123)
#   --filter-name NAME    Include only frames matching this message name
#   --start-time ISO      Include frames at or after this ISO-8601 timestamp
#   --end-time ISO        Include frames at or before this ISO-8601 timestamp
#   --archive             Package output into a .tar.gz archive
#   --checksum            Generate SHA-256 checksum file alongside output
#   --dry-run             Preview actions without writing files
#   --help                Show this help message
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

INPUT_FILE=""
OUTPUT_FORMAT="csv"
OUTPUT_PATH="./exported_logs"
FILTER_ID=""
FILTER_NAME=""
START_TIME=""
END_TIME=""
CREATE_ARCHIVE=false
CREATE_CHECKSUM=false
DRY_RUN=false

print_usage() {
    cat <<EOF
Usage: $(basename "$0") <input.csv> [OPTIONS]

Export decoded CAN logs into structured CSV or JSON archives.

Options:
  --format csv|json     Output format (default: csv)
  --output PATH         Output file/directory path (default: ./exported_logs)
  --filter-id HEX       Include only frames matching this CAN ID (hex, e.g. 0x123)
  --filter-name NAME    Include only frames matching this message name
  --start-time ISO      Include frames at or after this ISO-8601 timestamp
  --end-time ISO        Include frames at or before this ISO-8601 timestamp
  --archive             Package output into a .tar.gz archive
  --checksum            Generate SHA-256 checksum file alongside output
  --dry-run             Preview actions without writing files
  --help                Show this help message
EOF
}

# ----------------------------------------------------------------------
# Parse arguments
# ----------------------------------------------------------------------
if [[ $# -lt 1 ]]; then
    print_usage
    exit 1
fi

INPUT_FILE="$1"
shift

while [[ $# -gt 0 ]]; do
    case "$1" in
        --format)
            OUTPUT_FORMAT="$2"; shift 2 ;;
        --output)
            OUTPUT_PATH="$2"; shift 2 ;;
        --filter-id)
            FILTER_ID="$2"; shift 2 ;;
        --filter-name)
            FILTER_NAME="$2"; shift 2 ;;
        --start-time)
            START_TIME="$2"; shift 2 ;;
        --end-time)
            END_TIME="$2"; shift 2 ;;
        --archive)
            CREATE_ARCHIVE=true; shift ;;
        --checksum)
            CREATE_CHECKSUM=true; shift ;;
        --dry-run)
            DRY_RUN=true; shift ;;
        --help|-h)
            print_usage; exit 0 ;;
        *)
            echo -e "${RED}Error: unknown option '$1'${NC}" >&2
            print_usage >&2
            exit 1 ;;
    esac
done

# Validate input
if [[ ! -f "${INPUT_FILE}" ]]; then
    echo -e "${RED}Error: input file not found: ${INPUT_FILE}${NC}" >&2
    exit 1
fi

if [[ "${OUTPUT_FORMAT}" != "csv" && "${OUTPUT_FORMAT}" != "json" ]]; then
    echo -e "${RED}Error: invalid format '${OUTPUT_FORMAT}' (use csv or json)${NC}" >&2
    exit 1
fi

# ----------------------------------------------------------------------
# Dry-run preview
# ----------------------------------------------------------------------
if [[ "${DRY_RUN}" == true ]]; then
    echo -e "${CYAN}================================================================${NC}"
    echo -e "${CYAN}  EXPORT LOGS â€” DRY RUN (no files will be written)              ${NC}"
    echo -e "${CYAN}================================================================${NC}"
    echo "  Input file      : ${INPUT_FILE}"
    echo "  Output format   : ${OUTPUT_FORMAT}"
    echo "  Output path     : ${OUTPUT_PATH}"
    echo "  Filter ID       : ${FILTER_ID:-none}"
    echo "  Filter name     : ${FILTER_NAME:-none}"
    echo "  Start time      : ${START_TIME:-none}"
    echo "  End time        : ${END_TIME:-none}"
    echo "  Archive         : ${CREATE_ARCHIVE}"
    echo "  Checksum        : ${CREATE_CHECKSUM}"
    echo -e "${CYAN}================================================================${NC}"
    exit 0
fi

# ----------------------------------------------------------------------
# Export logic
# ----------------------------------------------------------------------
echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}  CAN LOG EXPORT UTILITY                                         ${NC}"
echo -e "${CYAN}================================================================${NC}"
echo "  Input file      : ${INPUT_FILE}"
echo "  Output format   : ${OUTPUT_FORMAT}"
echo "  Output path     : ${OUTPUT_PATH}"
echo ""

# Determine output filename base
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BASENAME="can_export_${TIMESTAMP}"

if [[ "${OUTPUT_FORMAT}" == "csv" ]]; then
    OUTFILE="${OUTPUT_PATH}.csv"
else
    OUTFILE="${OUTPUT_PATH}.json"
fi

# Create output directory if writing to a directory path
OUT_DIR="$(dirname "${OUTFILE}")"
mkdir -p "${OUT_DIR}"

# Write CSV header if format is csv
FRAME_COUNT=0

if [[ "${OUTPUT_FORMAT}" == "csv" ]]; then
    {
        echo "Timestamp_ISO,Timestamp_Sec,CAN_ID_Hex,CAN_ID_Dec,Message_Name,DLC,Payload_Hex,Signal_Name,Raw_Value,Physical_Value,Unit"
    } > "${OUTFILE}"
fi

# ----------------------------------------------------------------------
# Process input CSV line by line
# ----------------------------------------------------------------------
# Expected CSV header:
# Timestamp_ISO,Timestamp_Sec,CAN_ID_Hex,CAN_ID_Dec,Message_Name,DLC,Payload_Hex,Signal_Name,Raw_Value,Physical_Value,Unit
#
# We skip the header line and apply filters on each data row.

HEADER_SEEN=false

while IFS= read -r line || [[ -n "${line}" ]]; do
    # Skip empty lines
    [[ -z "${line}" ]] && continue

    # Detect and skip header
    if [[ "${HEADER_SEEN}" == false ]]; then
        if [[ "${line}" == Timestamp_ISO* ]]; then
            HEADER_SEEN=true
            continue
        fi
    fi

    # Parse CSV fields (simple comma split â€” no quoted fields expected)
    IFS=',' read -r ts_iso ts_sec can_id_hex can_id_dec msg_name dlc payload_hex sig_name raw_val phys_val unit <<< "${line}"

    # Apply CAN ID filter
    if [[ -n "${FILTER_ID}" ]]; then
        # Normalize filter to uppercase hex without 0x prefix for comparison
        FILTER_NORM=$(echo "${FILTER_ID}" | sed 's/^0x//' | tr 'a-f' 'A-F')
        CAN_NORM=$(echo "${can_id_hex}" | sed 's/^0x//' | tr 'a-f' 'A-F')
        if [[ "${CAN_NORM}" != "${FILTER_NORM}" ]]; then
            continue
        fi
    fi

    # Apply message name filter
    if [[ -n "${FILTER_NAME}" && "${msg_name}" != "${FILTER_NAME}" ]]; then
        continue
    fi

    # Apply time range filters (string comparison works for ISO-8601)
    if [[ -n "${START_TIME}" && "${ts_iso}" < "${START_TIME}" ]]; then
        continue
    fi
    if [[ -n "${END_TIME}" && "${ts_iso}" > "${END_TIME}" ]]; then
        continue
    fi

    FRAME_COUNT=$((FRAME_COUNT + 1))

    if [[ "${OUTPUT_FORMAT}" == "csv" ]]; then
        echo "${ts_iso},${ts_sec},${can_id_hex},${can_id_dec},${msg_name},${dlc},${payload_hex},${sig_name},${raw_val},${phys_val},${unit}" >> "${OUTFILE}"
    else
        # JSON output â€” accumulate as JSON array
        # Escape any quotes in string fields for JSON safety
        json_msg_name=$(echo "${msg_name}" | sed 's/"/\\"/g')
        json_unit=$(echo "${unit}" | sed 's/"/\\"/g')
        json_payload=$(echo "${payload_hex}" | sed 's/"/\\"/g')
        json_sig=$(echo "${sig_name}" | sed 's/"/\\"/g')
        json_ts=$(echo "${ts_iso}" | sed 's/"/\\"/g')

        if [[ "${FRAME_COUNT}" -eq 1 ]]; then
            echo "[" > "${OUTFILE}"
            echo "  {" >> "${OUTFILE}"
        else
            echo "  }," >> "${OUTFILE}"
            echo "  {" >> "${OUTFILE}"
        fi

        printf '    "timestamp_iso": "%s",\n' "${json_ts}" >> "${OUTFILE}"
        printf '    "timestamp_sec": %s,\n' "${ts_sec}" >> "${OUTFILE}"
        printf '    "can_id_hex": "%s",\n' "${can_id_hex}" >> "${OUTFILE}"
        printf '    "can_id_dec": %s,\n' "${can_id_dec}" >> "${OUTFILE}"
        printf '    "message_name": "%s",\n' "${json_msg_name}" >> "${OUTFILE}"
        printf '    "dlc": %s,\n' "${dlc}" >> "${OUTFILE}"
        printf '    "payload_hex": "%s",\n' "${json_payload}" >> "${OUTFILE}"
        printf '    "signal_name": "%s",\n' "${json_sig}" >> "${OUTFILE}"
        printf '    "raw_value": %s,\n' "${raw_val}" >> "${OUTFILE}"
        printf '    "physical_value": %s,\n' "${phys_val}" >> "${OUTFILE}"
        printf '    "unit": "%s"\n' "${json_unit}" >> "${OUTFILE}"
    fi
done < "${INPUT_FILE}"

# Close JSON array if needed
if [[ "${OUTPUT_FORMAT}" == "json" && "${FRAME_COUNT}" -gt 0 ]]; then
    echo "  }" >> "${OUTFILE}"
    echo "]" >> "${OUTFILE}"
elif [[ "${OUTPUT_FORMAT}" == "json" && "${FRAME_COUNT}" -eq 0 ]]; then
    echo "[]" > "${OUTFILE}"
fi

# ----------------------------------------------------------------------
# Checksum generation
# ----------------------------------------------------------------------
if [[ "${CREATE_CHECKSUM}" == true ]]; then
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "${OUTFILE}" > "${OUTFILE}.sha256"
        echo -e "${GREEN}[OK]${NC} Checksum written: ${OUTFILE}.sha256"
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "${OUTFILE}" > "${OUTFILE}.sha256"
        echo -e "${GREEN}[OK]${NC} Checksum written: ${OUTFILE}.sha256"
    else
        echo -e "${YELLOW}[WARN]${NC} No sha256sum/shasum available â€” skipping checksum"
    fi
fi

# ----------------------------------------------------------------------
# Archive packaging
# ----------------------------------------------------------------------
ARCHIVE_FILE=""
if [[ "${CREATE_ARCHIVE}" == true ]]; then
    ARCHIVE_DIR="$(dirname "${OUTFILE}")"
    ARCHIVE_FILE="${ARCHIVE_DIR}/${BASENAME}.tar.gz"
    tar -czf "${ARCHIVE_FILE}" -C "${ARCHIVE_DIR}" "$(basename "${OUTFILE}")" \
        $([[ -f "${OUTFILE}.sha256" ]] && echo "$(basename "${OUTFILE}.sha256")")
    echo -e "${GREEN}[OK]${NC} Archive created: ${ARCHIVE_FILE}"
fi

# ----------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------
echo -e "${CYAN}================================================================${NC}"
echo -e "${GREEN}  EXPORT COMPLETE${NC}"
echo -e "${CYAN}================================================================${NC}"
echo "  Frames exported : ${FRAME_COUNT}"
echo "  Output file     : ${OUTFILE}"
if [[ "${CREATE_CHECKSUM}" == true ]]; then
    echo "  Checksum        : ${OUTFILE}.sha256"
fi
if [[ "${CREATE_ARCHIVE}" == true ]]; then
    echo "  Archive         : ${ARCHIVE_FILE}"
fi
echo -e "${CYAN}================================================================${NC}"

exit 0
