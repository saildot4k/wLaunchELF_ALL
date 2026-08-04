## ###
# Dumps relocated words/strings from the current Ghidra program.
# @category: CrystalChip.Python
# @runtime PyGhidra
##

import typing

if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *


def read_c_string(address, limit=128):
    data = []
    for offset in range(limit):
        value = getByte(address.add(offset)) & 0xff
        if value == 0:
            break
        if value < 0x20 or value >= 0x7f:
            data.append("\\x%02x" % value)
        else:
            data.append(chr(value))
    return "".join(data)


args = getScriptArgs()
if len(args) < 3:
    print("Usage: DumpMemoryWords.py <output.txt> <addr> <count> [strings]")
else:
    out_path = args[0]
    base = toAddr(args[1])
    count = int(args[2], 0)
    dump_strings = len(args) > 3 and args[3] == "strings"

    with open(out_path, "w", encoding="utf-8") as out:
        out.write(f"Program: {currentProgram.name}\n")
        out.write(f"Base: {base}\n")
        out.write(f"Count: {count}\n\n")
        for index in range(count):
            addr = base.add(index * 4)
            value = getInt(addr) & 0xffffffff
            line = f"{addr}: 0x{value:08x}"
            if dump_strings and value != 0:
                try:
                    line += f"  -> {read_c_string(toAddr(value))}"
                except Exception:
                    pass
            out.write(line + "\n")
    print("Wrote " + out_path)
