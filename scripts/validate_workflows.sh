#!/usr/bin/env bash
#
# validate_workflows.sh â€” Actionlint & syntax verification for GitHub workflows
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKFLOW_DIR="${REPO_ROOT}/.github/workflows"

echo "================================================================"
echo " can-logger-decoder â€” Workflow Syntax & Lint Verification"
echo "================================================================"
echo "Workflow directory: ${WORKFLOW_DIR}"
echo ""

STATUS=0

for wf in "${WORKFLOW_DIR}"/*.yml; do
  [ -f "$wf" ] || continue
  echo "Checking $(basename "$wf")..."
  if command -v actionlint >/dev/null 2>&1; then
    actionlint "$wf" || STATUS=1
  else
    if command -v python3 >/dev/null 2>&1; then
      python3 -c "import yaml; yaml.safe_load(open('$wf'))" || {
        echo "YAML parse failure in $wf"
        STATUS=1
      }
    else
      echo "  [INFO] actionlint/python3 not installed, syntax check skipped."
    fi
  fi
done

if [ "$STATUS" -eq 0 ]; then
  echo "Workflow validation completed successfully."
  exit 0
else
  echo "Workflow validation encountered errors."
  exit "$STATUS"
fi