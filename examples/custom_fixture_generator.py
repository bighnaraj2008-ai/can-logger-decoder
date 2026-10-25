#!/usr/bin/env python3
"""
Custom Test Fixture Generator for can-logger-decoder.

Generates binary wire frames (.bin) and hexadecimal frame sequences (.hex)
for CAN test fixtures and edge-case validation suites.
"""

import struct
import argparse
from typing import List, Tuple

CAN_EFF_FLAG = 0x80000000
CAN_RTR_FLAG = 0x40000000
CAN_ERR_FLAG = 0x20000000


def encode_wire_frame(can_id: int, dlc: int, data: bytes) -> bytes:
    """Encode a CAN frame into 5-byte header + payload wire format."""
    header = struct.pack(">IB", can_id, dlc)
    payload = data[:dlc].ljust(dlc, b"\x00")
    return header + payload


def generate_sample_binary_fixture(out_path: str):
    """Generate sample raw frames binary fixture."""
    frames = [
        (0x100, 8, bytes([0x10, 0x27, 0x72, 0x1F, 0x7F, 0x82, 0xAA, 0x55])),
        (0x101, 8, bytes([0x80, 0x5F, 0x01, 0x8D, 0x00, 0x00, 0x00, 0x00])),
        (0x200, 6, bytes([0x40, 0xC8, 0x00, 0x00, 0x00, 0x00])),
        (0x300, 4, bytes([0x01, 0x52, 0x02, 0x00])),
        (CAN_EFF_FLAG | 0x18DAF110, 8, bytes([0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04])),
        (CAN_EFF_FLAG | 0x1FFFFFFF, 0, b""),
        (CAN_RTR_FLAG | 0x100, 0, b""),
        (CAN_ERR_FLAG | 0x100, 0, b""),
    ]

    with open(out_path, "wb") as f:
        for can_id, dlc, data in frames:
            f.write(encode_wire_frame(can_id, dlc, data))
    print(f"Generated binary fixture: {out_path}")


def generate_sample_hex_fixture(out_path: str):
    """Generate sample hex fixture with anomaly descriptions."""
    lines = [
        "# Truncated header (3 bytes < 5 minimum)",
        "000000",
        "# Out of bounds DLC 9",
        "000000000900",
        "# Truncated payload",
        "000000000800",
        "# Standard zero-length frame",
        "0000000000",
    ]
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Generated hex fixture: {out_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CAN Test Fixture Generator")
    parser.add_argument("--bin", default="custom_frames.bin", help="Output path for binary fixture")
    parser.add_argument("--hex", default="custom_frames.hex", help="Output path for hex fixture")
    args = parser.parse_args()

    generate_sample_binary_fixture(args.bin)
    generate_sample_hex_fixture(args.hex)
