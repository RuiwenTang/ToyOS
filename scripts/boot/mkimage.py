#!/usr/bin/env python3
"""mkimage.py — attach the arm64 Linux boot header to a raw kernel binary.

One image, two loaders: both QEMU's -kernel and U-Boot's booti recognise the
64-byte header (magic "ARM\\x64" at 0x38), honour text_offset (0x80000 from
the 2 MiB-aligned RAM base), and hand control over with x0 = DTB pointer.
That is the whole boot contract from docs/aarch64-port-arch.md.

Header layout (Documentation/arch/arm64/booting.rst), little-endian:
    0x00 code0        b +0x40 (skip the header)
    0x04 code1        0
    0x08 text_offset  where the image base sits inside RAM
    0x10 image_size   header + payload, bytes
    0x18 flags        bit0 = little-endian kernel
    0x20..0x37        reserved, zero
    0x38 magic        0x644d524d
    0x3c reserved
"""

import struct
import sys

# "ARM\x64" little-endian (41 52 4d 64 on disk). Linux's image.h spells it
# "ARM\x64"; note 0x644d5241, not ...4d — the last byte is 'A' (0x41), the
# 'd' lives in the top byte. QEMU's -kernel does not enforce it, U-Boot's
# booti does ("Bad Linux ARM64 Image magic!").
MAGIC = 0x644D5241
TEXT_OFFSET = 0x80000
HEADER_SIZE = 64


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <raw-binary> <output-image>", file=sys.stderr)
        return 2

    with open(sys.argv[1], "rb") as f:
        payload = f.read()

    header = bytearray(HEADER_SIZE)
    struct.pack_into("<I", header, 0x00, 0x14000010)  # b +0x40
    struct.pack_into("<Q", header, 0x08, TEXT_OFFSET)
    struct.pack_into("<Q", header, 0x10, HEADER_SIZE + len(payload))
    struct.pack_into("<Q", header, 0x18, 1)  # little-endian
    struct.pack_into("<I", header, 0x38, MAGIC)

    with open(sys.argv[2], "wb") as f:
        f.write(header + payload)
    return 0


if __name__ == "__main__":
    sys.exit(main())
