# Authoring Custom CAN Test Fixtures

This guide describes how to author, encode, and validate custom test fixtures for the `can-logger-decoder` test harness.

---

## 1. Wire Framing Protocol

All binary test fixtures follow the canonical `can-logger-decoder` wire format:

```text
[id_b3][id_b2][id_b1][id_b0][dlc][data_byte_0]...[data_byte_N-1]
Total size = 5 + dlc bytes
```

### Identifier Encodings
- **11-bit Standard ID**: Upper 21 bits are zero. (Example: `0x00000100`)
- **29-bit Extended ID**: Bit 31 is set (`CAN_EFF_FLAG = 0x80000000`). (Example: `0x98DAF110`)
- **RTR Flag**: Bit 30 set (`CAN_RTR_FLAG = 0x40000000`).
- **Error Flag**: Bit 29 set (`CAN_ERR_FLAG = 0x20000000`).

---

## 2. Text Fixtures (.hex)

Hexadecimal text fixtures allow human-readable documentation of malformed and corrupted frames:

- One frame per line as contiguous uppercase or lowercase hex pairs.
- Lines starting with `#` are treated as comments and ignored.
- Empty lines are skipped.

### Example Hex Entry
```text
# DLC 8 frame with truncated 2-byte payload (wire_len 7 < 13)
00000000080000
```

---

## 3. Using the Generator Script

A helper script is provided in `examples/custom_fixture_generator.py`:

```bash
python3 examples/custom_fixture_generator.py --bin custom.bin --hex custom.hex
```

---

## 4. Registering Fixtures in the Test Harness

1. Place generated fixture files in `tests/fixtures/`.
2. Add expected outcomes to the test tables in `tests/test_fixtures.c`.
3. Run `./scripts/run_unit_tests.sh` to verify.
