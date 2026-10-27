# Release Process & Tagging Runbook â€” can-logger-decoder (Day 4)

This runbook outlines the checklist and procedure for tagging and publishing
new production releases of `can-logger-decoder`.

---

## 1. Pre-Release Verification Checklist

1. **Verify Clean Git Status**:
   ```bash
   git status
   git diff
   ```
2. **Run Linter & Formatting Checks**:
   ```bash
   ./scripts/check_formatting.sh
   ./scripts/run_linter.sh
   ```
3. **Execute All Test Suites**:
   ```bash
   make clean && make test
   ./scripts/run_unit_tests.sh
   ```
4. **Smoke Test Integration**:
   ```bash
   ./scripts/smoke_test.sh
   ```

---

## 2. Version Bumping

1. Run version bump script:
   ```bash
   ./scripts/bump_version.sh 2.1.0
   ```
2. Update `CHANGELOG.md` with release notes and date.

---

## 3. Git Tagging & Release Trigger

1. Create annotated Git tag:
   ```bash
   git tag -a v2.1.0 -m "Release v2.1.0: Day 4 CI/CD and automation"
   ```
2. Push commit and tag to GitHub:
   ```bash
   git push origin main
   git push origin v2.1.0
   ```

---

## 4. Automated Workflow Verification

Pushing a `v*` tag triggers `.github/workflows/release_artifacts.yml`:
- Cross-compiles for Linux x86_64, Linux ARMv7, and Windows x86_64.
- Generates SHA-256 checksums.
- Attaches release archives to GitHub Releases page.