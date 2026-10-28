#!/usr/bin/env bash
#
# prune_buffer.sh â€” CAN Log Rotation & Buffer Cleanup Utility
#
# Manages log rotation and buffer cleanup on resource-constrained embedded
# nodes. Supports size-based rotation, age-based pruning, disk-space
# threshold checks, and a dry-run mode to preview actions.
#
# Usage:
#   ./scripts/prune_buffer.sh [OPTIONS]
#
# Options:
#   --dir PATH            Log directory to manage (default: ./logs)
#   --max-size SIZE       Max size per log file before rotation (e.g. 10M, 1G)
#   --max-count N         Maximum number of rotated log files to retain
#   --max-age-days N      Delete logs older than N days
#   --min-free-space PCT  Minimum free disk percentage required (abort if below)
#   --dry-run             Preview actions without executing
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

LOG_DIR="./logs"
MAX_SIZE=""
MAX_COUNT=""
MAX_AGE_DAYS=""
MIN_FREE_SPACE=""
DRY_RUN=false

print_usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Manage log rotation and buffer cleanup on resource-constrained embedded nodes.

Options:
  --dir PATH            Log directory to manage (default: ./logs)
  --max-size SIZE       Max size per log file before rotation (e.g. 10M, 1G)
  --max-count N         Maximum number of rotated log files to retain
  --max-age-days N      Delete logs older than N days
  --min-free-space PCT  Minimum free disk percentage required (abort if below)
  --dry-run             Preview actions without executing
  --help                Show this help message
EOF
}

# ----------------------------------------------------------------------
# Parse arguments
# ----------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir)
            LOG_DIR="$2"; shift 2 ;;
        --max-size)
            MAX_SIZE="$2"; shift 2 ;;
        --max-count)
            MAX_COUNT="$2"; shift 2 ;;
        --max-age-days)
            MAX_AGE_DAYS="$2"; shift 2 ;;
        --min-free-space)
            MIN_FREE_SPACE="$2"; shift 2 ;;
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

# ----------------------------------------------------------------------
# Dry-run preview
# ----------------------------------------------------------------------
if [[ "${DRY_RUN}" == true ]]; then
    echo -e "${CYAN}================================================================${NC}"
    echo -e "${CYAN}  PRUNE BUFFER â€” DRY RUN (no actions will be executed)         ${NC}"
    echo -e "${CYAN}================================================================${NC}"
    echo "  Log directory     : ${LOG_DIR}"
    echo "  Max file size     : ${MAX_SIZE:-none}"
    echo "  Max file count    : ${MAX_COUNT:-none}"
    echo "  Max age (days)    : ${MAX_AGE_DAYS:-none}"
    echo "  Min free space    : ${MIN_FREE_SPACE:-none}%"
    echo -e "${CYAN}================================================================${NC}"
    exit 0
fi

# ----------------------------------------------------------------------
# Validate log directory
# ----------------------------------------------------------------------
if [[ ! -d "${LOG_DIR}" ]]; then
    echo -e "${YELLOW}[INFO]${NC} Log directory '${LOG_DIR}' does not exist â€” nothing to prune."
    exit 0
fi

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}  CAN LOG PRUNE / BUFFER CLEANUP                                 ${NC}"
echo -e "${CYAN}================================================================${NC}"
echo "  Log directory     : ${LOG_DIR}"
echo ""

# ----------------------------------------------------------------------
# Disk space check
# ----------------------------------------------------------------------
if [[ -n "${MIN_FREE_SPACE}" ]]; then
    # Get free space percentage for the filesystem containing LOG_DIR
    FREE_PCT=$(df -P "${LOG_DIR}" | awk 'NR==2 {gsub("%","",$5); print 100 - $5}')
    if [[ "${FREE_PCT}" -lt "${MIN_FREE_SPACE}" ]]; then
        echo -e "${RED}[ABORT]${NC} Free disk space (${FREE_PCT}%) below threshold (${MIN_FREE_SPACE}%)."
        exit 1
    fi
    echo -e "${GREEN}[OK]${NC} Free disk space: ${FREE_PCT}% (threshold: ${MIN_FREE_SPACE}%)"
fi

ACTIONS_TAKEN=0

# ----------------------------------------------------------------------
# Size-based rotation: truncate oversized log files
# ----------------------------------------------------------------------
if [[ -n "${MAX_SIZE}" ]]; then
    # Convert human-readable size to bytes
    case "${MAX_SIZE}" in
        *K|*k) SIZE_BYTES=$(( ${MAX_SIZE%[Kk]} * 1024 )) ;;
        *M|*m) SIZE_BYTES=$(( ${MAX_SIZE%[Mm]} * 1024 * 1024 )) ;;
        *G|*g) SIZE_BYTES=$(( ${MAX_SIZE%[Gg]} * 1024 * 1024 * 1024 )) ;;
        *)     SIZE_BYTES="${MAX_SIZE}" ;;
    esac

    while IFS= read -r -d '' file; do
        fsize=$(stat -c%s "${file}" 2>/dev/null || stat -f%z "${file}" 2>/dev/null || echo 0)
        if [[ "${fsize}" -gt "${SIZE_BYTES}" ]]; then
            rotated="${file}.rotated.$(date +%Y%m%d_%H%M%S)"
            echo -e "${YELLOW}[ROTATE]${NC} ${file} (${fsize} bytes > ${SIZE_BYTES} bytes) -> ${rotated}"
            mv "${file}" "${rotated}"
            touch "${file}"
            ACTIONS_TAKEN=$((ACTIONS_TAKEN + 1))
        fi
    done < <(find "${LOG_DIR}" -type f \( -name "*.log" -o -name "*.csv" \) -print0 2>/dev/null || true)
fi

# ----------------------------------------------------------------------
# Max count: retain only the N most recent rotated files
# ----------------------------------------------------------------------
if [[ -n "${MAX_COUNT}" ]]; then
    # Collect rotated files sorted by modification time (newest first)
    mapfile -t ROTATED_FILES < <(find "${LOG_DIR}" -type f -name "*.rotated.*" -printf '%T@ %p\n' 2>/dev/null \
        | sort -rn | awk '{print $2}')

    if [[ ${#ROTATED_FILES[@]} -gt "${MAX_COUNT}" ]]; then
        for ((i = MAX_COUNT; i < ${#ROTATED_FILES[@]}; i++)); do
            echo -e "${YELLOW}[PRUNE]${NC} Exceeds max count (${MAX_COUNT}): ${ROTATED_FILES[$i]}"
            rm -f "${ROTATED_FILES[$i]}"
            ACTIONS_TAKEN=$((ACTIONS_TAKEN + 1))
        done
    fi
fi

# ----------------------------------------------------------------------
# Max age: delete files older than N days
# ----------------------------------------------------------------------
if [[ -n "${MAX_AGE_DAYS}" ]]; then
    while IFS= read -r -d '' file; do
        echo -e "${YELLOW}[PRUNE]${NC} Older than ${MAX_AGE_DAYS} days: ${file}"
        rm -f "${file}"
        ACTIONS_TAKEN=$((ACTIONS_TAKEN + 1))
    done < <(find "${LOG_DIR}" -type f \( -name "*.log" -o -name "*.csv" -o -name "*.rotated.*" \) -mtime "+${MAX_AGE_DAYS}" -print0 2>/dev/null || true)
fi

# ----------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------
echo -e "${CYAN}================================================================${NC}"
if [[ "${ACTIONS_TAKEN}" -eq 0 ]]; then
    echo -e "${GREEN}  NO ACTIONS REQUIRED â€” BUFFER IS CLEAN${NC}"
else
    echo -e "${GREEN}  ACTIONS TAKEN: ${ACTIONS_TAKEN}${NC}"
fi
echo -e "${CYAN}================================================================${NC}"

exit 0
