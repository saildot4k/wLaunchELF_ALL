#!/usr/bin/env python3
"""Unpack Crystal Chip/UCL N2E streams used by the firmware sources."""

from __future__ import annotations

import argparse
from pathlib import Path


def unpack_n2e(data: bytes) -> bytes:
    src = 0
    out = bytearray()
    bitbuf = 0
    bitmask = 0x00FF0000
    last_offset = 1

    def getbit() -> int:
        nonlocal src, bitbuf

        if (bitbuf & bitmask) == 0:
            if src >= len(data):
                raise EOFError("input exhausted while reading bitstream")
            bitbuf = data[src] | bitmask
            src += 1

        bit = (bitbuf >> 7) & 1
        bitbuf = (bitbuf << 1) & 0xFFFFFFFF
        return bit

    while True:
        if getbit():
            if src >= len(data):
                raise EOFError("input exhausted while reading literal")
            out.append(data[src])
            src += 1
            continue

        match_offset = 1
        while True:
            match_offset <<= 1
            match_offset += getbit()
            bit = getbit()
            match_offset -= 1
            if bit:
                break
            match_offset <<= 1
            match_offset += getbit()

        match_offset -= 1
        if match_offset == 0:
            match_offset = last_offset
            match_length = getbit()
        else:
            match_offset -= 1
            if src >= len(data):
                raise EOFError("input exhausted while reading match offset")
            match_offset = ((match_offset << 8) + data[src]) & 0xFFFFFFFF
            src += 1
            marker = (match_offset + 1) & 0xFFFFFFFF
            if marker == 0:
                return bytes(out)
            match_length = marker & 1
            match_offset = (match_offset >> 1) + 1
            last_offset = match_offset

        bit = getbit()
        if match_length:
            match_length = bit + 3
        elif bit:
            match_length = getbit() + 5
        else:
            match_length += 1
            while True:
                match_length <<= 1
                match_length += getbit()
                if getbit():
                    break
            match_length += 5

        if match_offset < 0x501:
            match_length -= 1

        ref = len(out) - match_offset
        if ref < 0:
            raise ValueError(f"invalid back-reference offset {match_offset}")
        for _ in range(match_length):
            out.append(out[ref])
            ref += 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    args.output.write_bytes(unpack_n2e(args.input.read_bytes()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
