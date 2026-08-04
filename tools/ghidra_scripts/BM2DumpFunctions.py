## ###
# Dumps focused Ghidra decompiler output for Crystal Chip BootManager analysis.
# @category: CrystalChip.Python
# @runtime PyGhidra
##

import typing

if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *

from ghidra.app.decompiler import DecompInterface, DecompileOptions


def function_for(address):
    function = getFunctionContaining(address)
    if function is None:
        function = getFunctionAt(address)
    if function is None:
        disassemble(address)
        function = createFunction(address, "FUN_" + str(address))
    return function


def dump_listing(out, function):
    listing = currentProgram.listing
    instruction = listing.getInstructionAt(function.entryPoint)
    count = 0
    while instruction is not None and function.body.contains(instruction.address) and count < 512:
        out.write(f"  {instruction.address}: {instruction}\n")
        instruction = instruction.next
        count += 1
    if count >= 512:
        out.write("  <listing truncated at 512 instructions>\n")


def function_instructions(function, limit=4096):
    listing = currentProgram.listing
    instruction = listing.getInstructionAt(function.entryPoint)
    count = 0
    while instruction is not None and function.body.contains(instruction.address) and count < limit:
        yield instruction
        instruction = instruction.next
        count += 1


def dump_function(out, decompiler, address_text):
    address = toAddr(address_text)
    function = function_for(address)

    out.write("================================================================\n")
    out.write(f"Requested: {address_text}\n")
    if function is None:
        out.write(f"No function recovered at {address}\n\n")
        return

    out.write(f"Function: {function.name}\n")
    out.write(f"Entry: {function.entryPoint}\n")
    out.write(f"Body: {function.body}\n\n")

    out.write("Call references from body:\n")
    any_refs = False
    for instruction in function_instructions(function):
        for ref in currentProgram.referenceManager.getReferencesFrom(instruction.address):
            if ref.referenceType.isCall():
                any_refs = True
                out.write(f"  {ref.fromAddress} -> {ref.toAddress} {ref.referenceType}\n")
    if not any_refs:
        out.write("  <none>\n")
    out.write("\n")

    results = decompiler.decompileFunction(function, 120, monitor)
    out.write("Decompile status:\n")
    out.write(
        f"  completed={results.decompileCompleted()} "
        f"failed={results.failedToStart()} "
        f"message={results.errorMessage}\n\n"
    )
    out.write("Decompiled C:\n")
    decompiled = results.decompiledFunction
    if decompiled is None:
        out.write("<no decompiled function>\n")
    else:
        out.write(decompiled.getC())
        out.write("\n")

    out.write("\nListing:\n")
    dump_listing(out, function)
    out.write("\n")


args = getScriptArgs()
if len(args) < 2:
    print("Usage: BM2DumpFunctions.py <output.txt> <addr> [addr ...]")
else:
    decompiler = DecompInterface()
    options = DecompileOptions()
    decompiler.setOptions(options)
    if not decompiler.openProgram(currentProgram):
        raise RuntimeError("Unable to initialize decompiler: " + decompiler.lastMessage)
    try:
        with open(args[0], "w", encoding="utf-8") as out:
            out.write(f"Program: {currentProgram.name}\n")
            out.write(f"Language: {currentProgram.languageID}\n")
            out.write(f"Compiler: {currentProgram.compilerSpec.compilerSpecID}\n\n")
            for address_text in args[1:]:
                dump_function(out, decompiler, address_text)
        print("Wrote " + args[0])
    finally:
        decompiler.closeProgram()
        decompiler.dispose()
