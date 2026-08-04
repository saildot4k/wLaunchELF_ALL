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
`0x0630`. It embeds the same recovered modules already present in this tree:

| Embedded IRX | Offset in unpacked `iopman.irx` | Match |
|---|---:|---|
| `ccdriver.irx` | `0x0ac0` | byte-for-byte match |
| `dffs.irx` | `0x3a40` | byte-for-byte match |

It does not contain `ccmodman.irx` as a byte-for-byte embedded module.

## Loading Implication

The current full recovered stack hangs at `dffs ccdriver`, meaning the EE-side
`SifExecModuleBuffer(ccdriver_irx, ...)` call never returns on the test
hardware. The firmware analysis gives two better hypotheses to test:

1. The chip expects its firmware IOPBOOT/LOADCORE path to provide `ccdriver`
   before `dffs.irx` is used.
2. If wLaunchELF does not use the firmware IOPBOOT path, loading only
   `dffs.irx` after a normal IOP reset is still safer than directly starting
   the recovered standalone `ccdriver.irx`; it should fail or succeed without
   hanging at the known `ccdriver` stage.

The new GitHub Actions mode:

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

If that reaches the main page but DFFS still fails, the next experimental path
is to embed unpacked `iopman.irx` and test a post-boot `iopman` load followed
by a harmless module-load trigger. That is closer to the firmware design than
the standalone `ccdriver` path, but it is not identical to the official
IOPBOOT-time injection and should be treated as experimental.
