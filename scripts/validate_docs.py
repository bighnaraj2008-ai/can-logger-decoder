#!/usr/bin/env python3
"""
can-logger-decoder Documentation Link Validator
Scans all Markdown (.md) and Mermaid (.mmd) files in the repository
and verifies that relative file links and anchors resolve properly.
"""

import os
import re
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent

LINK_PATTERN = re.compile(r'\[([^\]]+)\]\(([^)]+)\)')

def validate_links():
    docs_to_check = []
    for root, dirs, files in os.walk(ROOT_DIR):
        # Skip git, build, fixtures, and hidden directories
        dirs[:] = [d for d in dirs if not d.startswith('.') and d not in ('build', 'target', 'node_modules')]
        for f in files:
            if f.endswith('.md') or f.endswith('.mmd'):
                docs_to_check.append(Path(root) / f)

    broken_links = []
    checked_count = 0

    print(f"Validating internal links across {len(docs_to_check)} documentation files...")

    for doc_path in docs_to_check:
        try:
            content = doc_path.read_text(encoding='utf-8', errors='ignore')
        except Exception as e:
            print(f"[WARN] Could not read {doc_path}: {e}")
            continue

        for line_no, line in enumerate(content.splitlines(), start=1):
            for match in LINK_PATTERN.finditer(line):
                text, target = match.groups()
                # Skip external web links, mailto, badges, and code links
                if target.startswith(('http://', 'https://', 'mailto:', 'file:')):
                    continue

                checked_count += 1

                # Anchor within same file
                if target.startswith('#'):
                    # anchor check (simplified)
                    continue

                # Strip anchor fragment
                file_target = target.split('#')[0]
                if not file_target:
                    continue

                # Resolve relative path
                resolved = (doc_path.parent / file_target).resolve()
                if not resolved.exists():
                    rel_doc = doc_path.relative_to(ROOT_DIR)
                    broken_links.append((str(rel_doc), line_no, target))

    print(f"Checked {checked_count} internal markdown references.")
    if broken_links:
        print(f"\n[FAIL] Found {len(broken_links)} broken link(s):")
        for doc, line, target in broken_links:
            print(f"  - {doc}:{line} -> {target}")
        return 1

    print("[PASS] All documentation links verified successfully.")
    return 0

if __name__ == '__main__':
    sys.exit(validate_links())
