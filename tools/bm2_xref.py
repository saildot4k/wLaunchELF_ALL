#!/usr/bin/env python3
"""Focused static analysis helper for unpacked Crystal Chip BootManager ELFs.

The recovered BM2 ELF has one load segment and no section headers, which makes
normal ELF tools less useful. This script extracts the parts that matter for
DFFS investigation: embedded IRX inventory, ASCII strings, simple MIPS address
constant xrefs, and nearby JAL targets.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
from dataclasses import dataclass
from pathlib import Path


REG_NAMES = [
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
]


@dataclass
class LoadSegment:
    offset: int
    vaddr: int
    filesz: int
    memsz: int

    def contains_off(self, off: int) -> bool:
        return self.offset <= off < self.offset + self.filesz

    def contains_vaddr(self, vaddr: int) -> bool:
        return self.vaddr <= vaddr < self.vaddr + self.filesz

    def off_to_vaddr(self, off: int) -> int:
        return self.vaddr + (off - self.offset)

    def vaddr_to_off(self, vaddr: int) -> int:
        return self.offset + (vaddr - self.vaddr)


@dataclass
class StringRef:
    off: int
    vaddr: int
    text: str


def u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def cstr(data: bytes, off: int, limit: int = 96) -> str:
    end = data.find(b"\0", off, min(len(data), off + limit))
    if end < 0:
        end = min(len(data), off + limit)
    return data[off:end].decode("ascii", "replace")


def parse_load_segment(data: bytes) -> LoadSegment:
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError("expected little-endian ELF32")
    phoff = u32(data, 0x1C)
    phentsize = u16(data, 0x2A)
    phnum = u16(data, 0x2C)
    for i in range(phnum):
        off = phoff + i * phentsize
        p_type, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_flags, _p_align = struct.unpack_from(
            "<IIIIIIII", data, off
        )
        if p_type == 1:
            return LoadSegment(p_offset, p_vaddr, p_filesz, p_memsz)
    raise ValueError("no PT_LOAD segment")


def high_adjusted(vaddr: int) -> int:
    return ((vaddr + 0x8000) >> 16) & 0xFFFF


def sign16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def reg(value: int, shift: int) -> int:
    return (value >> shift) & 0x1F


def op(value: int) -> int:
    return (value >> 26) & 0x3F


def mnemonic(word: int, pc: int = 0) -> str:
    opc = op(word)
    rs = reg(word, 21)
    rt = reg(word, 16)
    rd = reg(word, 11)
    imm = word & 0xFFFF
    simm = sign16(imm)
    target = ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
    if word == 0:
        return "nop"
    if opc == 0:
        funct = word & 0x3F
        if funct == 0x08:
            return f"jr ${REG_NAMES[rs]}"
        if funct == 0x09:
            return f"jalr ${REG_NAMES[rd]}, ${REG_NAMES[rs]}"
        if funct == 0x21:
            return f"addu ${REG_NAMES[rd]}, ${REG_NAMES[rs]}, ${REG_NAMES[rt]}"
        if funct == 0x23:
            return f"subu ${REG_NAMES[rd]}, ${REG_NAMES[rs]}, ${REG_NAMES[rt]}"
        if funct == 0x24:
            return f"and ${REG_NAMES[rd]}, ${REG_NAMES[rs]}, ${REG_NAMES[rt]}"
        if funct == 0x25:
            return f"or ${REG_NAMES[rd]}, ${REG_NAMES[rs]}, ${REG_NAMES[rt]}"
        if funct == 0x2A:
            return f"slt ${REG_NAMES[rd]}, ${REG_NAMES[rs]}, ${REG_NAMES[rt]}"
        if funct == 0x2D:
            return f"daddu ${REG_NAMES[rd]}, ${REG_NAMES[rs]}, ${REG_NAMES[rt]}"
        if funct == 0x2F:
            return f"dsubu ${REG_NAMES[rd]}, ${REG_NAMES[rs]}, ${REG_NAMES[rt]}"
        if funct == 0x00:
            sa = (word >> 6) & 0x1F
            return f"sll ${REG_NAMES[rd]}, ${REG_NAMES[rt]}, {sa}"
        if funct == 0x02:
            sa = (word >> 6) & 0x1F
            return f"srl ${REG_NAMES[rd]}, ${REG_NAMES[rt]}, {sa}"
        return f"special_{funct:02x} 0x{word:08x}"
    if opc == 0x02:
        return f"j 0x{target:08x}"
    if opc == 0x03:
        return f"jal 0x{target:08x}"
    if opc == 0x04:
        return f"beq ${REG_NAMES[rs]}, ${REG_NAMES[rt]}, 0x{pc + 4 + simm * 4:08x}"
    if opc == 0x05:
        return f"bne ${REG_NAMES[rs]}, ${REG_NAMES[rt]}, 0x{pc + 4 + simm * 4:08x}"
    if opc == 0x06:
        return f"blez ${REG_NAMES[rs]}, 0x{pc + 4 + simm * 4:08x}"
    if opc == 0x07:
        return f"bgtz ${REG_NAMES[rs]}, 0x{pc + 4 + simm * 4:08x}"
    if opc == 0x14:
        return f"beql ${REG_NAMES[rs]}, ${REG_NAMES[rt]}, 0x{pc + 4 + simm * 4:08x}"
    if opc == 0x15:
        return f"bnel ${REG_NAMES[rs]}, ${REG_NAMES[rt]}, 0x{pc + 4 + simm * 4:08x}"
    if opc == 0x08:
        return f"addi ${REG_NAMES[rt]}, ${REG_NAMES[rs]}, {simm}"
    if opc == 0x09:
        return f"addiu ${REG_NAMES[rt]}, ${REG_NAMES[rs]}, {simm}"
    if opc == 0x0A:
        return f"slti ${REG_NAMES[rt]}, ${REG_NAMES[rs]}, {simm}"
    if opc == 0x0C:
        return f"andi ${REG_NAMES[rt]}, ${REG_NAMES[rs]}, 0x{imm:04x}"
    if opc == 0x0D:
        return f"ori ${REG_NAMES[rt]}, ${REG_NAMES[rs]}, 0x{imm:04x}"
    if opc == 0x0F:
        return f"lui ${REG_NAMES[rt]}, 0x{imm:04x}"
    if opc == 0x20:
        return f"lb ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x21:
        return f"lh ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x23:
        return f"lw ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x24:
        return f"lbu ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x25:
        return f"lhu ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x28:
        return f"sb ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x29:
        return f"sh ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x2B:
        return f"sw ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x37:
        return f"ld ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    if opc == 0x3F:
        return f"sd ${REG_NAMES[rt]}, {simm}(${REG_NAMES[rs]})"
    return f"op_{opc:02x} 0x{word:08x}"


def strings_for(data: bytes, seg: LoadSegment, pattern: re.Pattern[str]) -> list[StringRef]:
    out: list[StringRef] = []
    for match in re.finditer(rb"[\x20-\x7e]{4,}", data):
        text = match.group(0).decode("ascii", "replace")
        if pattern.search(text):
            off = match.start()
            vaddr = seg.off_to_vaddr(off) if seg.contains_off(off) else off
            out.append(StringRef(off, vaddr, text))
    return out


def find_xrefs(data: bytes, seg: LoadSegment, target: int, scan_start: int, scan_end: int) -> list[int]:
    hi = high_adjusted(target)
    lo = target & 0xFFFF
    hits: list[int] = []
    for off in range(scan_start, scan_end - 4, 4):
        word = u32(data, off)
        if op(word) != 0x0F or (word & 0xFFFF) != hi:
            continue
        rt = reg(word, 16)
        for look in range(off + 4, min(off + 44, scan_end - 4), 4):
            w2 = u32(data, look)
            opc = op(w2)
            rs2 = reg(w2, 21)
            rt2 = reg(w2, 16)
            imm2 = w2 & 0xFFFF
            if rs2 != rt:
                continue
            if opc == 0x09 and rt2 == rt and imm2 == lo:
                hits.append(seg.off_to_vaddr(off))
                break
            if opc == 0x0D and rt2 == rt and imm2 == lo:
                hits.append(seg.off_to_vaddr(off))
                break
    return hits


def nearby_calls(data: bytes, seg: LoadSegment, pc: int, before: int = 0x80, after: int = 0xC0) -> list[int]:
    out: list[int] = []
    start = max(seg.offset, seg.vaddr_to_off(pc) - before)
    end = min(seg.offset + seg.filesz, seg.vaddr_to_off(pc) + after)
    for off in range(start & ~3, end - 4, 4):
        word = u32(data, off)
        if op(word) == 0x03:
            cur = seg.off_to_vaddr(off)
            out.append(((cur + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2))
    return out


def disasm_window(data: bytes, seg: LoadSegment, pc: int, radius: int = 0x40) -> list[str]:
    start = max(seg.offset, seg.vaddr_to_off(pc) - radius)
    end = min(seg.offset + seg.filesz, seg.vaddr_to_off(pc) + radius)
    lines: list[str] = []
    for off in range(start & ~3, end - 4, 4):
        cur = seg.off_to_vaddr(off)
        lines.append(f"0x{cur:08x}: 0x{u32(data, off):08x}  {mnemonic(u32(data, off), cur)}")
    return lines


def embedded_elf_inventory(data: bytes) -> list[tuple[int, str, str, int]]:
    out: list[tuple[int, str, str, int]] = []
    pos = 0
    while True:
        off = data.find(b"\x7fELF", pos)
        if off < 0:
            break
        pos = off + 1
        if off + 0x34 > len(data):
            continue
        shoff = u32(data, off + 0x20)
        shentsize = u16(data, off + 0x2E)
        shnum = u16(data, off + 0x30)
        shstrndx = u16(data, off + 0x32)
        end = 0x34
        name = "(main)"
        if shoff and shentsize and shnum and shstrndx < shnum:
            end = max(end, shoff + shentsize * shnum)
            shstr_hdr_off = off + shoff + shstrndx * shentsize
            shstr = b""
            if shstr_hdr_off + 40 <= len(data):
                sh = struct.unpack_from("<IIIIIIIIII", data, shstr_hdr_off)
                shstr = data[off + sh[4]:off + sh[4] + sh[5]]
            for i in range(shnum):
                sh_hdr_off = off + shoff + i * shentsize
                if sh_hdr_off + 40 > len(data):
                    continue
                sh = struct.unpack_from("<IIIIIIIIII", data, sh_hdr_off)
                no = sh[0]
                ne = shstr.find(b"\0", no) if shstr else -1
                sname = shstr[no:ne].decode("ascii", "replace") if ne >= 0 else ""
                if sname == ".iopmod" and sh[4] + 90 <= len(data):
                    name = cstr(data, off + sh[4] + 26, 64)
        else:
            try:
                seg = parse_load_segment(data[off:])
                end = max(end, seg.offset + seg.filesz)
            except ValueError:
                pass
        blob = data[off:off + end]
        out.append((off, name, hashlib.sha256(blob).hexdigest(), len(blob)))
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("--pattern", default="dffs|DataFlashFS|ccdriver|ccrpc|moduleload|MODLOADED|LIBLOADED|MOUNT|LOADIMG|IOPIRX|IOPELF")
    parser.add_argument("--code-end", default="0x235b20", help="exclusive file offset for EE-side scan")
    parser.add_argument("--windows", action="store_true", help="print disassembly windows for each xref")
    parser.add_argument("--start", help="start virtual address for direct disassembly")
    parser.add_argument("--end", help="end virtual address for direct disassembly")
    args = parser.parse_args()

    data = args.elf.read_bytes()
    seg = parse_load_segment(data)
    code_end = int(args.code_end, 0)
    pattern = re.compile(args.pattern, re.IGNORECASE)

    print(f"ELF sha256={hashlib.sha256(data).hexdigest()} load_off=0x{seg.offset:x} vaddr=0x{seg.vaddr:x} filesz=0x{seg.filesz:x}")
    if args.start or args.end:
        if not args.start or not args.end:
            raise ValueError("--start and --end must be used together")
        start = int(args.start, 0)
        end = int(args.end, 0)
        for cur in range(start, end, 4):
            if not seg.contains_vaddr(cur):
                continue
            off = seg.vaddr_to_off(cur)
            print(f"0x{cur:08x}: 0x{u32(data, off):08x}  {mnemonic(u32(data, off), cur)}")
        return 0

    print("\nEmbedded ELF inventory:")
    for off, name, sha, size in embedded_elf_inventory(data):
        print(f"  off=0x{off:06x} vaddr=0x{seg.off_to_vaddr(off):08x} size=0x{size:x} sha256={sha} module={name}")

    print("\nString xrefs:")
    for s in strings_for(data, seg, pattern):
        if s.off >= code_end:
            continue
        xrefs = find_xrefs(data, seg, s.vaddr, seg.offset, code_end)
        if not xrefs:
            continue
        print(f"  str off=0x{s.off:06x} vaddr=0x{s.vaddr:08x} text={s.text!r}")
        for x in xrefs:
            calls = sorted(set(nearby_calls(data, seg, x)))
            calls_text = " ".join(f"0x{c:08x}" for c in calls)
            print(f"    xref=0x{x:08x} nearby_jal=[{calls_text}]")
            if args.windows:
                for line in disasm_window(data, seg, x):
                    print(f"      {line}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
