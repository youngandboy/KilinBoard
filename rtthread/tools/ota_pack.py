#!/usr/bin/env python3
"""Package rtthread.bin into the OTA container expected by the bootloader."""

import argparse
import struct
import zlib
from pathlib import Path

MAGIC = b"KYOTA1\0\0"
HEADER_SIZE = 32


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("bin", type=Path, help="input rtthread.bin")
    parser.add_argument("-o", "--out", type=Path, help="output .ota path")
    parser.add_argument("--version", type=lambda x: int(x, 0), default=1)
    args = parser.parse_args()

    payload = args.bin.read_bytes()
    out = args.out or args.bin.with_suffix(".ota")
    image_crc = zlib.crc32(payload) & 0xFFFFFFFF

    header = struct.pack(
        "<8sIIII8s",
        MAGIC,
        args.version & 0xFFFFFFFF,
        len(payload),
        image_crc,
        0,
        b"\0" * 8,
    )
    header_crc = zlib.crc32(header[:20]) & 0xFFFFFFFF
    header = header[:20] + struct.pack("<I", header_crc) + header[24:]

    out.write_bytes(header + payload)

    print(f"OTA image: {out}")
    print(f"  version  : 0x{args.version & 0xFFFFFFFF:08x}")
    print(f"  payload  : {len(payload)} bytes")
    print(f"  image CRC: 0x{image_crc:08x}")
    print(f"  header CRC: 0x{header_crc:08x}")
    print(f"  file size: {len(header) + len(payload)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
