# Crystal Chip Firmware DFFS Loading

This note records the firmware-side loading path from
`saildot4k/Crystal-Chip-R34-v6` branch `v6`, checked against the recovered
BootManager IRXs in this tree.

## Official CC2 DFFS Boot Path

For `BOOT_DEV == 3`, `SRC/OSDPAY.S` does not pass DFFS IRX modules to
`moduleload`. It does this instead:

- writes `TMP/IOPLOAD.BIN` to Crystal Chip SRAM with `CC_SRAM_Write`
- enables `BIOS_IOPBOOT_EN | BIOS_READKEY_EN | BIOS_MM16_EN | BIOS_MCEXEC_EN`
- calls `LoadExecPS2("moduleload", 1, ["dffs:/BM/BM2.ELF"])`

`SRC/IOPLOAD.S` is a patch for `rom0:IOPBOOT`. It copies `SRC/IOPPAY.S`
payload code from the Crystal Chip boot ROM window to IOP RAM and jumps to it.

`SRC/IOPPAY.S` hooks execution of LOADCORE at `0xBFC4A44C`, reads the firmware
FWFS index from DataFlash, loads packed firmware file `IOPMAN.IRX`, and inserts
that module as the third LOADCORE module after `SYSMEM` and `LOADCORE`.

Practical consequence: official `dffs:/BM/BM2.ELF` boot does not start the
DFFS stack by loading `ccmodman.irx`, `ccdriver.irx`, and `dffs.irx` from the
EE after normal IOP startup. It injects a special IOP module before normal
LOADCORE module initialization has finished.

## IOPMAN.N2E

`SRC/IOPMAN.N2E` can be unpacked with:

```sh
tools/n2e_unpack.py /tmp/Crystal-Chip-R34-v6/SRC/IOPMAN.N2E /tmp/iopman.irx
```

Observed hashes from the `v6` firmware tree:

| File | SHA-256 | Size |
|---|---|---:|
| `SRC/IOPMAN.N2E` | `a66f3e9ce7d1e8ffc6e6787f44e5ea93b170dc02d5404454f8ac38f7bf50699d` | 17095 |
| unpacked `iopman.irx` | `33e104e952adbf4ba43ce3e2bdde5a8ef9b45fcff3ecdaf45669375cf5fbb0d3` | 41869 |

The unpacked module has IOP module name `iopman`, version `1.0`, entry
`0x0630`. It embeds short `ccdriver` and `dffs` ELF images:

| Embedded IRX | Offset in unpacked `iopman.irx` | SHA-256 | Size |
|---|---:|---|---:|
| `ccdriver` | `0x0ac0` | `45def2973dbaa060fcb9423464d4bfbd5d5f3e583fd74eabd39a4a48a8075eda` | 9056 |
| `dffs` | `0x3a40` | `29b0179ef589cc707cb7c0640060ab9ec2696c3e249111cc99f4ef9087d3940c` | 21036 |

It does not contain `ccmodman.irx` as a byte-for-byte embedded module.

## BM2 EE Loader

`BM2UNPACKED.ELF` has one load segment at `0x00100000`, entry `0x00100008`,
and no section headers. `tools/bm2_xref.py` extracts useful static-analysis
facts without needing Ghidra.

The embedded BootManager module-loading function at `0x001005b0` checks whether
each module is already loaded, then calls a buffer-module loader at `0x001239d8`
with `(module_buffer, size_word_at_buffer_minus_0x10, 0, 0, &ret)`.

Ghidra 12.1.2 plus `ghidra-emotionengine-reloaded` imports this ELF as
`r5900:LE:32:default`. The checked-in helper
`tools/ghidra_scripts/BM2DumpFunctions.py` can be run through PyGhidra to dump
focused decompiler output for the addresses below.

Ghidra confirms the loader shape:

- `0x001005b0` is a fixed embedded-module dispatcher.
- `0x00109b00` returns whether a named IOP module is present.
- `0x00109a00` walks the IOP module list at `0xbc000800`, comparing names with
  `strcmp` while masking/restoring interrupts.
- `0x001239d8` allocates IOP heap memory, writes back the EE cache, DMAs the
  embedded IRX image to that IOP heap pointer, calls the module-buffer RPC, and
  frees the IOP heap buffer.
- `0x001247b8` binds the old loadfile/module-loader RPC server
  `0x80000006`, command `6`, and sends a 0x200-byte request whose first word is
  the IOP pointer to the already-DMAed module image. Optional arguments are
  copied into the request at offset `0x104`.
- `0x00123ab8` and `0x00123b28` bind RPC server `0x80000003` and use commands
  `1` and `2` for IOP heap allocation/free.

Practical consequence: BM2 is not using a magical EE-side module loader for
these direct IRXs. It uses the normal old SIF RPC path after copying the module
image to IOP memory itself. The unusual Crystal Chip behavior is still the
firmware/IOPBOOT/LOADCORE injection path described above, or behavior inside
the recovered IOP modules.

Observed direct-load order:

1. `CrystalChipDriver` from `0x0034cf30`
2. `ccrpc` from `0x0034feb0`
3. `IOX/File_Manager` from `0x003419f0`
4. `IOX/File_Manager_Rpc` from `0x00343c20`
5. `CDVDDRVR` from `0x00345bd0`
6. `ISOFS` from `0x0034a530`
7. `DataFlashFS` from `0x003507f0`

The direct `ccdriver` and `dffs` buffers used by BM2 are larger than the short
copies embedded in `IOPMAN.N2E`; they include the relocation records referenced
by `.rel.text` and `.rel.data`.

| Module | BootManager offset | BootManager size word | SHA-256 of full buffer |
|---|---:|---:|---|
| `ccdriver.irx` | `0x24df30` | `0x2f69` | `fce71823295a13f737856b056d2a638e3df1044d22e16fb87a4e9bba21518028` |
| `ccrpc.irx` | `0x250eb0` | `0x929` | `97f6f40f9f5409daa7e53ccbbb89cb8d470018783a05b47ef9e0a245e335d040` |
| `dffs.irx` | `0x2517f0` | `0x648d` | `5bed54983a9a09a1bdf121ed4957e3eafd1b82198b6bb11e3f6a0eacd6329380` |

The repo now uses those full BootManager-sized `ccdriver.irx` and `dffs.irx`
buffers for direct EE-side load tests, and also includes `ccrpc.irx`.

## Loading Implication

The full recovered stack hung at `dffs ccdriver`, meaning the EE-side
`SifExecModuleBuffer(ccdriver_irx, ...)` call never returned on the test
hardware. Ghidra analysis of `ccdriver.irx` shows why this point is dangerous:
the module entry registers the `ccdriver` export table, then performs a
read/write/verify/restore self-test against flash offset `0` before it returns.

The firmware/BM2 analysis gives two better hypotheses to test:

1. The chip expects its firmware IOPBOOT/LOADCORE path to provide `ccdriver`
   before `dffs.irx` is used.
2. The recovered standalone modules are not safe to start directly from EE code
   after normal IOP startup.

Hardware testing supports the second point. `DataFlashFS only - reset IOP, load
dffs` hangs at `dffs fs`, while the preserve-IOP modes do not reach the main
page. That makes a simple "keep whatever BootManager already loaded" approach
unlikely to work.

The new GitHub Actions mode:

```text
BootManager order - ccdriver, ccrpc, dffs
```

builds:

```text
DFFS=1 DFFS_LOAD_RECOVERED=1 DFFS_LOAD_IOPMAN=0 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=1 DFFS_LOAD_CCRPC=1 IOP_RESET=1
```

This is the closest direct EE-side test to the BM2 decompilation without also
replacing wLaunchELF's current `iomanx`, `filexio`, CDVD, and ISOFS stack with
the old BootManager copies.

The firmware-wrapper mode:

```text
Firmware iopman - iopman, dffs
```

builds:

```text
DFFS=1 DFFS_LOAD_RECOVERED=1 DFFS_LOAD_IOPMAN=1 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=0 IOP_RESET=1
```

This fetches `SRC/IOPMAN.N2E` from the Crystal Chip firmware repository during
GitHub Actions, unpacks it with `tools/n2e_unpack.py`, embeds the resulting
`iopman.irx`, loads it as `dffs iopman`, then probes `dffs:` before attempting
to start `dffs.irx`.

The fallback comparison mode:

```text
DataFlashFS only - reset IOP, load dffs
```

builds:

```text
DFFS=1 DFFS_LOAD_RECOVERED=1 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=0 IOP_RESET=1
```

This keeps the normal wLaunchELF IOP reset/core startup, skips both recovered
`ccmodman` and recovered standalone `ccdriver`, then loads only `dffs.irx` when
the user selects `dffs:`.

The direct `dffs.irx`-only path is now mostly a control test. It is expected to
fail unless a compatible `ccdriver` export table is already resident.

If `Firmware iopman - iopman, dffs` hangs at `dffs iopman`, then loading
firmware `iopman.irx` after normal IOP startup is not viable. If the BM2 direct
order also hangs, the next path should be a bounded custom `ccdriver`
replacement for DFFS ordinals `5`, `13`, and `14`, or the heavier true
firmware-style IOPBOOT/LOADCORE injection/custom IOP reboot image.
