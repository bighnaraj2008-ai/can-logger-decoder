# Telemetry Data Archiving & Retention Policy â€” can-logger-decoder (Day 5)

This document establishes the storage tiering, compression, and retention rules
for automotive CAN bus logs captured by `can-logger-decoder`.

---

## 1. Storage Tiers

| Tier | Format | Storage Location | Max Age | Retention Action |
|:-----|:-------|:-----------------|:--------|:-----------------|
| Hot | Uncompressed `.csv` | Local NVMe / eMMC | 24 Hours | Rotated by `prune_buffer.sh` |
| Warm | Compressed `.tar.gz` | Edge Storage / SD | 7 Days | Synced to Fleet Cloud |
| Cold | Structured JSON / Parquet | Cloud Data Lake | 3 Years | Long-term Fleet Analytics |

---

## 2. Retention Enforcement

- **Disk Threshold**: Storage nodes abort local recording if available disk drops below 5%.
- **Integrity**: Every compressed log archive must generate a SHA-256 `.sha256` checksum file.
- **Pruning**: Automated daily execution via `scripts/prune_buffer.sh --max-age-days 7`.