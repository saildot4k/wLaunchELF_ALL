# Crystal Chip DFFS Driver Analysis

This document records what can be recovered from the three BootManager IRX
modules embedded in `BM2UNPACKED.ELF`.

The local helper used for this analysis is:

```sh
tools/irx_inspect.py iop/__precompiled/ccmodman.irx iop/__precompiled/ccdriver.irx iop/__precompiled/dffs.irx
```

It parses PS2 IRX import/export tables using the PS2SDK structures from
`irx.h`:

- import magic: `0x41e00000`
- export magic: `0x41c00000`
- import stubs: `jr $ra` followed by `0x24000000 | ordinal`

## Module Summary

| File | IOP module name | Version | Entry | Role |
|---|---:|---:|---:|---|
| `ccmodman.irx` | `modman` | `1.0` | `0x13a8` | Crystal Chip module policy/reload helper |
| `ccdriver.irx` | `CrystalChipDriver` | `1.1` | `0x0370` | Low-level Crystal Chip hardware/flash access library |
| `dffs.irx` | `DataFlashFS` | `1.0` | `0x0000` | IOMAN filesystem driver registering `dffs:` |

## Import/Export Relationship

`dffs.irx` imports one Crystal Chip-specific library:

```text
ccdriver v1.1 ordinals: 5, 13, 14
```

`ccdriver.irx` exports library `ccdriver` v1.1:

| Ordinal | Address | Current meaning |
|---:|---:|---|
| `0` | `0x0370` | module entry/start |
| `5` | `0x16c0` | detect Crystal Chip and return runtime info |
| `13` | `0x1ba8` | flash read path used by DFFS |
| `14` | `0x1bfc` | flash write path used by DFFS |

The function names are inferred from call behavior. The ordinals and addresses
are directly recovered from the export/import tables.

## ccdriver Startup Path

`ccdriver.irx` starts at `0x0370`.

Confirmed flow:

1. Print `Crystal Chip Driver v%X.%X ********`.
2. Query/check the `ccdriver` export table through `loadcore` ordinal `6`.
3. If already registered, return success immediately.
4. Otherwise call local init/detect function `0x0000`.
5. If that succeeds, register the `ccdriver` export table through `loadcore`
   ordinal `7`.

The hardware test that hangs at `dffs ccdriver` is therefore hanging inside
`SifExecModuleBuffer(ccdriver_irx, ...)`, most likely before the recovered
`ccdriver` module returns from this startup path.

## Crystal Chip Hardware Port

`ccdriver.irx` directly accesses byte address:

```text
0xbfc001ad
```

Confirmed helpers:

| Address | Behavior |
|---:|---|
| `0x1294` | Write one byte to `0xbfc001ad` |
| `0x12d8` | Read one byte from `0xbfc001ad` |
| `0x03f0` | Send command bytes and wait/check response |

The recovered driver communicates with the chip through command packets over
that byte port. Several higher-level helpers retry command transfers. Some
retry loops do not show an obvious fixed timeout in the inspected windows, so a
hardware/firmware state mismatch can plausibly look like a hard hang.

## ccdriver Ordinal 5

`ccdriver#5` at `0x16c0` is the key detect/info function.

Confirmed flow:

1. If a global "chip detected" flag is already set, return the runtime info
   struct at `0x20f4`.
2. Send command byte `0x06`.
3. Read a response byte.
4. Store:
   - chip type nibble at info offset `+0`
   - capability/flags byte at info offset `+1`
   - derived mode byte at info offset `+2`
   - flash geometry pointer at info offset `+4`
5. Return the info struct, or null on failed detection/unsupported geometry.

`dffs.irx` calls this function from its driver init path at `0x0060`.
If it returns null, DFFS prints:

```text
Failed detecting CC!
```

## Flash Geometry

`ccdriver#5` selects a geometry entry from a table beginning near `0x2020`.
The table index comes from bits in a chip response byte. Each nonzero entry is
12 bytes and appears to be:

```text
u32 unit_count
u32 unit_size
u32 address_pack_function
```

Observed nonzero entries include:

| Response class | Unit count | Unit size | Address packer |
|---:|---:|---:|---:|
| `3` | `0x200` | `0x108` | `0x0820` |
| `5` | `0x400` | `0x108` | `0x0820` |
| `7` | `0x800` | `0x108` | `0x0820` |
| `9` | `0x1000` | `0x108` | `0x0820` |
| `11` | `0x1000` | `0x210` | `0x0864` |
| `13` | `0x2000` | `0x210` | `0x0864` |

DFFS multiplies `unit_count * unit_size` and requires the result to be larger
than `0x7ffff` before it proceeds.

## dffs Startup Path

`dffs.irx` starts at `0x0000`.

Confirmed flow:

1. Print `Crystal Chip Flash File System v%X.%X ********`.
2. Remove any existing `dffs` IOMAN driver.
3. Add/register its own `dffs` IOMAN driver struct at `0x4dc4`.

Important distinction: module start mainly registers the driver. Crystal Chip
detection is in the DFFS driver init path at `0x0060`, not in the module entry
itself.

DFFS driver init at `0x0060`:

1. Calls `ccdriver#5`.
2. Stores the returned info pointer.
3. Reads the flash geometry pointer from info offset `+4`.
4. Builds a FAT-like block device view.
5. Attempts to mount.
6. If mount fails, it can run the path behind:

```text
DFFS: Failed to mount, formatting...
```

This confirms the existing caution: DFFS write/format behavior is real and
should remain behind explicit hardware tests.

## dffs Reads and Writes

DFFS calls `ccdriver#13` from:

```text
0x09b4
0x0c6c
```

DFFS calls `ccdriver#14` from:

```text
0x0b4c
0x0cf8
```

Inferred meaning:

- `ccdriver#13`: flash read
- `ccdriver#14`: flash write

Both functions divide the requested byte offset by the geometry unit size, then
pass a unit index/remainder into lower-level command helpers.

## ccmodman Role

`ccmodman.irx` imports `loadcore`, `modload`, `sysmem`, `sifcmd`, `sysclib`,
`thbase`, and `ioman`; it exports no public library.

Strings and code paths show that it knows about:

```text
CrystalChipDriver
DataFlashFS
dffs:
ccdriver
mc0:/BM/SHARED/USBHDFSD.IRX
mc0:/BM/SHARED/USBD.IRX
rom0:MCMAN
rom0:SIO2MAN
```

Confirmed/inferred behavior:

- It walks loaded modules and library/module lists.
- It can load/start modules from buffers/paths through `modload`.
- It checks for `CrystalChipDriver`, `DataFlashFS`, `dffs:`, `mass:`, `mc0:`,
  and `mc1:`.
- Its entry calls a table of callbacks for modules it recognizes.

This supports the original developer statement that BootManager went out of
its way to keep DFFS accessible. It also means `ccmodman` may depend on the
BootManager/runtime IOP state and may not behave like a normal standalone
driver under a clean wLaunchELF IOP reset.

## Current Working Theory

The hang at `dffs ccdriver` is consistent with this sequence:

1. wLaunchELF resets IOP.
2. Recovered `ccmodman.irx` starts and returns.
3. Recovered `ccdriver.irx` starts.
4. Since no resident `ccdriver` library exists after reset, it enters its own
   hardware detect/init path.
5. That path talks to `0xbfc001ad` and retries command transfers.
6. The chip or firmware state does not answer as expected, so the module never
   returns to `SifExecModuleBuffer`.

The next most useful GitHub Actions mode is therefore:

```text
DataFlashFS only - preserve IOP, load dffs
```

That builds:

```text
DFFS=1
DFFS_LOAD_RECOVERED=1
DFFS_LOAD_CCMODMAN=0
DFFS_LOAD_CCDRIVER=0
IOP_RESET=0
```

This preserves any `ccdriver` library BootManager already registered, skips the
recovered `ccdriver` startup path, and only loads/registers `dffs.irx`.

If that does not work, test:

```text
Probe only - preserve IOP, load nothing
```

That tells us whether BootManager already left the full `dffs:` driver stack
resident before wLaunchELF_R3Z starts.
