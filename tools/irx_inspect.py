#!/usr/bin/env python3
"""Small PS2 IOP IRX inspection helper.

This intentionally avoids external Python packages so it can run in the
minimal CI/dev environments this repo tends to use.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from dataclasses import dataclass
from pathlib import Path


IMPORT_MAGIC = 0x41E00000
EXPORT_MAGIC = 0x41C00000
JR_RA = 0x03E00008
IMPORT_STUB_PREFIX = 0x24000000
SHT_NOBITS = 8

REGS = [
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
]


@dataclass
class Section:
    name: str
    stype: int
    flags: int
    addr: int
    offset: int
    size: int
    entsize: int

    @property
    def end(self) -> int:
        return self.addr + self.size


@dataclass
class ImportEntry:
    ordinal: int
    stub_vaddr: int
    stub_offset: int


@dataclass
class ImportTable:
    name: str
    version: int
    mode: int
    vaddr: int
    offset: int
    entries: list[ImportEntry]


@dataclass
class ExportTable:
    name: str
    version: int
    mode: int
    vaddr: int
    offset: int
    entries: list[int]


class Elf:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        self.sections = self._read_sections()

    def u16(self, off: int) -> int:
        return struct.unpack_from("<H", self.data, off)[0]

    def u32(self, off: int) -> int:
        return struct.unpack_from("<I", self.data, off)[0]

    def bytes_at_vaddr(self, vaddr: int, size: int) -> bytes:
        off = self.vaddr_to_offset(vaddr)
        return self.data[off:off + size]

    def u32_at_vaddr(self, vaddr: int) -> int:
        return self.u32(self.vaddr_to_offset(vaddr))

    def vaddr_to_offset(self, vaddr: int) -> int:
        matches = [
            sec for sec in self.sections
            if sec.size and sec.stype != SHT_NOBITS and sec.addr <= vaddr < sec.end
        ]
        for sec in matches:
            if sec.name != ".iopmod":
                return sec.offset + (vaddr - sec.addr)
        for sec in matches:
            return sec.offset + (vaddr - sec.addr)
        raise ValueError(f"vaddr 0x{vaddr:x} is not in any file-backed section")

    def offset_to_vaddr(self, off: int) -> int | None:
        for sec in self.sections:
            if sec.size and sec.stype != SHT_NOBITS and sec.offset <= off < sec.offset + sec.size:
                return sec.addr + (off - sec.offset)
        return None

    def section_for_vaddr(self, vaddr: int) -> Section | None:
        matches = [sec for sec in self.sections if sec.size and sec.addr <= vaddr < sec.end]
        for sec in matches:
            if sec.name != ".iopmod":
                return sec
        for sec in matches:
            return sec
        return None

    def _read_sections(self) -> list[Section]:
        if self.data[:4] != b"\x7fELF" or self.data[4] != 1 or self.data[5] != 1:
            raise ValueError(f"{self.path}: expected little-endian ELF32")

        e_shoff = self.u32(0x20)
        e_shentsize = self.u16(0x2E)
        e_shnum = self.u16(0x30)
        e_shstrndx = self.u16(0x32)

        raw = []
        for i in range(e_shnum):
            off = e_shoff + i * e_shentsize
            raw.append(struct.unpack_from("<IIIIIIIIII", self.data, off))

        str_hdr = raw[e_shstrndx]
        strtab = self.data[str_hdr[4]:str_hdr[4] + str_hdr[5]]

        def read_name(name_off: int) -> str:
            end = strtab.find(b"\0", name_off)
            if end < 0:
                end = len(strtab)
            return strtab[name_off:end].decode("ascii", "replace")

        return [
            Section(
                name=read_name(sh_name),
                stype=sh_type,
                flags=sh_flags,
                addr=sh_addr,
                offset=sh_offset,
                size=sh_size,
                entsize=sh_entsize,
            )
            for sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
            _link, _info, _align, sh_entsize in raw
        ]

    def text_words(self) -> list[tuple[int, int]]:
        text = next((s for s in self.sections if s.name == ".text"), None)
        if not text:
            return []
        out = []
        for rel in range(0, text.size, 4):
            out.append((text.addr + rel, self.u32(text.offset + rel)))
        return out


def cstr(data: bytes, off: int, max_len: int | None = None) -> str:
    end_limit = len(data) if max_len is None else min(len(data), off + max_len)
    end = data.find(b"\0", off, end_limit)
    if end < 0:
        end = end_limit
    return data[off:end].decode("ascii", "replace")


def irx_version(version: int) -> str:
    return f"{(version >> 8) & 0xff}.{version & 0xff}"


def parse_iopmod(elf: Elf) -> dict[str, int | str] | None:
    sec = next((s for s in elf.sections if s.name == ".iopmod"), None)
    if not sec or sec.size < 0x1A:
        return None
    off = sec.offset
    return {
        "moduleinfo": elf.u32(off),
        "entry": elf.u32(off + 4),
        "gp": elf.u32(off + 8),
        "text_size": elf.u32(off + 12),
        "data_size": elf.u32(off + 16),
        "bss_size": elf.u32(off + 20),
        "version": elf.u16(off + 24),
        "name": cstr(elf.data, off + 26, sec.size - 26),
    }


def parse_imports(elf: Elf) -> list[ImportTable]:
    out: list[ImportTable] = []
    data = elf.data
    pos = 0
    while True:
        idx = data.find(struct.pack("<I", IMPORT_MAGIC), pos)
        if idx < 0:
            break
        vaddr = elf.offset_to_vaddr(idx)
        pos = idx + 4
        if vaddr is None or idx + 20 > len(data):
            continue

        version = elf.u16(idx + 8)
        mode = elf.u16(idx + 10)
        name = cstr(data, idx + 12, 8)
        entries: list[ImportEntry] = []
        cur = idx + 20
        while cur + 8 <= len(data):
            jump = elf.u32(cur)
            fno_word = elf.u32(cur + 4)
            if jump == 0 and fno_word == 0:
                cur += 8
                break
            if jump != JR_RA or (fno_word & 0xFFFF0000) != IMPORT_STUB_PREFIX:
                break
            stub_vaddr = elf.offset_to_vaddr(cur)
            if stub_vaddr is None:
                break
            entries.append(ImportEntry(fno_word & 0xFFFF, stub_vaddr, cur))
            cur += 8

        out.append(ImportTable(name, version, mode, vaddr, idx, entries))
    return out


def parse_exports(elf: Elf) -> list[ExportTable]:
    out: list[ExportTable] = []
    data = elf.data
    pos = 0
    while True:
        idx = data.find(struct.pack("<I", EXPORT_MAGIC), pos)
        if idx < 0:
            break
        vaddr = elf.offset_to_vaddr(idx)
        pos = idx + 4
        if vaddr is None or idx + 20 > len(data):
            continue

        version = elf.u16(idx + 8)
        mode = elf.u16(idx + 10)
        name = cstr(data, idx + 12, 8)
        entries: list[int] = []
        cur = idx + 20
        while cur + 4 <= len(data):
            target = elf.u32(cur)
            cur += 4
            if target == 0:
                break
            entries.append(target)

        out.append(ExportTable(name, version, mode, vaddr, idx, entries))
    return out


def imm16(word: int) -> int:
    val = word & 0xFFFF
    return val - 0x10000 if val & 0x8000 else val


def reg(idx: int) -> str:
    return REGS[idx]


def decode(word: int, pc: int) -> str:
    op = (word >> 26) & 0x3F
    rs = (word >> 21) & 0x1F
    rt = (word >> 16) & 0x1F
    rd = (word >> 11) & 0x1F
    sh = (word >> 6) & 0x1F
    fn = word & 0x3F
    target = ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
    off = pc + 4 + (imm16(word) << 2)

    if word == 0:
        return "nop"
    if op == 0:
        special = {
            0x08: f"jr ${reg(rs)}",
            0x09: f"jalr ${reg(rd)}, ${reg(rs)}",
            0x0C: "syscall",
            0x0D: "break",
            0x10: f"mfhi ${reg(rd)}",
            0x12: f"mflo ${reg(rd)}",
            0x18: f"mult ${reg(rs)}, ${reg(rt)}",
            0x19: f"multu ${reg(rs)}, ${reg(rt)}",
            0x1A: f"div ${reg(rs)}, ${reg(rt)}",
            0x1B: f"divu ${reg(rs)}, ${reg(rt)}",
            0x20: f"add ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x21: f"addu ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x22: f"sub ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x23: f"subu ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x24: f"and ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x25: f"or ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x26: f"xor ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x27: f"nor ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x2A: f"slt ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
            0x2B: f"sltu ${reg(rd)}, ${reg(rs)}, ${reg(rt)}",
        }
        if fn in (0x00, 0x02, 0x03):
            m = {0x00: "sll", 0x02: "srl", 0x03: "sra"}[fn]
            return f"{m} ${reg(rd)}, ${reg(rt)}, {sh}"
        return special.get(fn, f"special_0x{fn:02x} 0x{word:08x}")
    if op in (0x02, 0x03):
        return f"{'jal' if op == 0x03 else 'j'} 0x{target:08x}"
    if op in (0x04, 0x05):
        return f"{'beq' if op == 0x04 else 'bne'} ${reg(rs)}, ${reg(rt)}, 0x{off:08x}"
    if op in (0x06, 0x07):
        return f"{'blez' if op == 0x06 else 'bgtz'} ${reg(rs)}, 0x{off:08x}"
    if op == 0x01:
        names = {0x00: "bltz", 0x01: "bgez", 0x10: "bltzal", 0x11: "bgezal"}
        return f"{names.get(rt, 'regimm')} ${reg(rs)}, 0x{off:08x}"
    if op in (0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E):
        names = {
            0x08: "addi", 0x09: "addiu", 0x0A: "slti", 0x0B: "sltiu",
            0x0C: "andi", 0x0D: "ori", 0x0E: "xori",
        }
        imm = word & 0xFFFF if op in (0x0C, 0x0D, 0x0E) else imm16(word)
        return f"{names[op]} ${reg(rt)}, ${reg(rs)}, {imm}"
    if op == 0x0F:
        return f"lui ${reg(rt)}, 0x{word & 0xFFFF:04x}"
    if op in (0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B, 0x2E):
        names = {
            0x20: "lb", 0x21: "lh", 0x23: "lw", 0x24: "lbu", 0x25: "lhu",
            0x28: "sb", 0x29: "sh", 0x2B: "sw", 0x2E: "swr",
        }
        return f"{names[op]} ${reg(rt)}, {imm16(word)}(${reg(rs)})"
    if op in (0x10, 0x12):
        return f"cop{op - 0x10} 0x{word & 0x03ffffff:07x}"
    return f"op_0x{op:02x} 0x{word:08x}"


def jal_target(word: int, pc: int) -> int | None:
    if ((word >> 26) & 0x3F) == 0x03:
        return ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
    return None


def find_call_sites(elf: Elf, imports: list[ImportTable]) -> dict[int, list[int]]:
    stubs = {entry.stub_vaddr for table in imports for entry in table.entries}
    out = {stub: [] for stub in stubs}
    for pc, word in elf.text_words():
        target = jal_target(word, pc)
        if target in out:
            out[target].append(pc)
    return out


def infer_function_starts(elf: Elf, exports: list[ExportTable], iopmod: dict[str, int | str] | None) -> list[int]:
    starts = set()
    if iopmod:
        starts.add(int(iopmod["entry"]))
    for exp in exports:
        for target in exp.entries:
            if elf.section_for_vaddr(target):
                starts.add(target)
    for pc, word in elf.text_words():
        target = jal_target(word, pc)
        if target is not None and elf.section_for_vaddr(target):
            starts.add(target)
    return sorted(starts)


def nearest_start(starts: list[int], pc: int) -> int | None:
    best = None
    for start in starts:
        if start <= pc:
            best = start
        else:
            break
    return best


def printable_string_at(elf: Elf, vaddr: int) -> str | None:
    sec = elf.section_for_vaddr(vaddr)
    if not sec or sec.name not in (".rodata", ".data", ".text"):
        return None
    off = elf.vaddr_to_offset(vaddr)
    raw = cstr(elf.data, off, 96)
    if len(raw) < 3:
        return None
    if any((ord(ch) < 0x20 and ch not in "\r\n\t") or ord(ch) > 0x7E for ch in raw):
        return None
    return raw


def find_la_strings(elf: Elf) -> list[tuple[int, str, int, str]]:
    words = elf.text_words()
    out: list[tuple[int, str, int, str]] = []
    last_lui: dict[int, tuple[int, int]] = {}
    for pc, word in words:
        op = (word >> 26) & 0x3F
        rs = (word >> 21) & 0x1F
        rt = (word >> 16) & 0x1F
        if op == 0x0F:
            last_lui[rt] = (pc, (word & 0xFFFF) << 16)
            continue
        if op in (0x09, 0x0D) and rs == rt and rt in last_lui:
            base_pc, hi = last_lui[rt]
            lo = (word & 0xFFFF) if op == 0x0D else imm16(word)
            addr = (hi + lo) & 0xFFFFFFFF
            s = printable_string_at(elf, addr)
            if s:
                out.append((base_pc, reg(rt), addr, s))
        if rt in last_lui and op != 0x0F:
            # Keep it simple: only track the immediately-built address pattern.
            last_lui.pop(rt, None)
    return out


def summarize(path: Path, disasm_windows: list[str]) -> None:
    elf = Elf(path)
    imports = parse_imports(elf)
    exports = parse_exports(elf)
    iopmod = parse_iopmod(elf)
    starts = infer_function_starts(elf, exports, iopmod)
    calls = find_call_sites(elf, imports)
    stub_names = {
        entry.stub_vaddr: f"{table.name}#{entry.ordinal}"
        for table in imports
        for entry in table.entries
    }

    print(f"# {path}")
    print(f"sha256: {hashlib.sha256(elf.data).hexdigest()}")
    print(f"size: {len(elf.data)} bytes")
    if iopmod:
        print(
            "iopmod: "
            f"name={iopmod['name']} version={irx_version(int(iopmod['version']))} "
            f"entry=0x{int(iopmod['entry']):08x} gp=0x{int(iopmod['gp']):08x} "
            f"text=0x{int(iopmod['text_size']):x} data=0x{int(iopmod['data_size']):x} "
            f"bss=0x{int(iopmod['bss_size']):x}"
        )

    print("sections:")
    for sec in elf.sections:
        if sec.name:
            print(f"  {sec.name:10s} addr=0x{sec.addr:08x} off=0x{sec.offset:06x} size=0x{sec.size:x}")

    print("imports:")
    for table in imports:
        ords = ", ".join(str(e.ordinal) for e in table.entries)
        print(
            f"  {table.name:8s} v{irx_version(table.version)} "
            f"table=0x{table.vaddr:08x} ordinals=[{ords}]"
        )

    print("exports:")
    for table in exports:
        print(f"  {table.name:8s} v{irx_version(table.version)} table=0x{table.vaddr:08x}")
        for i, target in enumerate(table.entries):
            tags = []
            if iopmod and target == int(iopmod["entry"]):
                tags.append("module_entry")
            try:
                if elf.u32_at_vaddr(target) == JR_RA:
                    tags.append("jr_ra_stub")
            except Exception:
                pass
            tag = f" ({', '.join(tags)})" if tags else ""
            print(f"    #{i:02d} -> 0x{target:08x}{tag}")

    print("import call sites:")
    for table in imports:
        for entry in table.entries:
            sites = calls.get(entry.stub_vaddr, [])
            if not sites:
                continue
            grouped = []
            for site in sites:
                start = nearest_start(starts, site)
                where = f"func 0x{start:08x}" if start is not None else "func ?"
                grouped.append(f"0x{site:08x} ({where})")
            print(f"  {table.name}#{entry.ordinal} stub=0x{entry.stub_vaddr:08x}: {', '.join(grouped)}")

    strings = find_la_strings(elf)
    if strings:
        print("string address builds:")
        for pc, r, addr, text in strings[:80]:
            print(f"  0x{pc:08x}: ${r}=0x{addr:08x} {text!r}")

    for spec in disasm_windows:
        start_s, _, count_s = spec.partition(":")
        start = int(start_s, 0)
        count = int(count_s or "64", 0)
        print(f"disasm 0x{start:08x}:")
        for i in range(count):
            pc = start + i * 4
            try:
                word = elf.u32_at_vaddr(pc)
            except Exception:
                break
            text = decode(word, pc)
            target = jal_target(word, pc)
            anno = ""
            if target in stub_names:
                anno = f" ; {stub_names[target]}"
            elif target is not None and elf.section_for_vaddr(target):
                anno = f" ; local 0x{target:08x}"
            print(f"  0x{pc:08x}: 0x{word:08x}  {text}{anno}")
    print()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("irx", nargs="+", type=Path)
    parser.add_argument("--disasm", action="append", default=[], help="vaddr[:instruction-count]")
    args = parser.parse_args()

    for path in args.irx:
        summarize(path, args.disasm)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
