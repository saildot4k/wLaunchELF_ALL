# Crystal Chip DFFS Driver Analysis

This document records what can be recovered from the Crystal Chip BootManager
IRX modules embedded in `BM2UNPACKED.ELF`.

The local helper used for this analysis is:

```sh
tools/irx_inspect.py iop/__precompiled/ccmodman.irx iop/__precompiled/ccdriver.irx iop/__precompiled/ccrpc.irx iop/__precompiled/dffs.irx
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
| `ccrpc.irx` | `ccrpc` | `1.0` | `0x0000` | EE RPC wrapper around `ccdriver`; used by BootManager |
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

`ccrpc.irx` imports `ccdriver` ordinals `5` through `15`, plus normal `sifcmd`
and thread libraries. It is useful for matching BootManager's direct module
order, but it is not on the `DataFlashFS` dependency chain.

The full BootManager-sized buffers now checked into `iop/__precompiled` are:

| File | SHA-256 | Size |
|---|---|---:|
| `ccdriver.irx` | `fce71823295a13f737856b056d2a638e3df1044d22e16fb87a4e9bba21518028` | `12137` |
| `ccrpc.irx` | `97f6f40f9f5409daa7e53ccbbb89cb8d470018783a05b47ef9e0a245e335d040` | `2345` |
| `dffs.irx` | `5bed54983a9a09a1bdf121ed4957e3eafd1b82198b6bb11e3f6a0eacd6329380` | `25741` |

## ccdriver Startup Path

`ccdriver.irx` starts at `0x0370`.

Confirmed flow:

1. Print `Crystal Chip Driver v%X.%X ********`.
2. Call `RegisterLibraryEntries` (`loadcore` ordinal `6`) with the `ccdriver`
   export table.
3. If registration fails, return failure.
4. If registration succeeds, call local init/self-test function `0x0000`.
5. If that self-test succeeds, return resident success.
6. If the self-test fails, call `ReleaseLibraryEntries` (`loadcore` ordinal
   `7`) and return failure.

The part that matters for hardware testing is that a normal module start enters
the local self-test. It does not just register a library and wait for DFFS to
probe later.

The local self-test at `0x0000` is invasive:

1. Sets a global delay to `100`.
2. Calls `ccdriver#5` detect.
3. Reads 32 bytes from flash offset `0`.
4. Writes the bitwise inverse of those 32 bytes.
5. Reads the same 32 bytes back repeatedly to verify the write.
6. Restores the original 32 bytes.

For older/simple flash it uses `ccdriver#6/#7`. For newer/geometry-backed flash
it uses `ccdriver#13/#14`, the same read/write paths used by DFFS.

The hardware test that hangs at `dffs ccdriver` is therefore hanging inside
`SifExecModuleBuffer(ccdriver_irx, ...)`, before the recovered `ccdriver`
module returns from this startup path. That hang can happen before `dffs.irx`
is loaded, and the module may already have attempted writes to flash offset
`0`.

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

The command sender at `0x03f0` writes this framing to `0xbfc001ad`:

```text
0xc0, 0xca, command byte 0, command byte 1, command byte 2, command byte 3, checksum
```

It then reads one status byte and treats a nonzero high nibble as busy/failure
until the caller-provided retry count expires. Other helpers read two-byte
complement-coded responses, where the second byte must be the bitwise
complement of the first.

Confirmed higher-level protocol pieces:

| Address | Behavior |
|---:|---|
| `0x01320` | Send `0x87 00 00 xx`, then read one complement-coded byte |
| `0x00760` | Query status through command `0xd7` |
| `0x007d8` | Wait until returned status has bit `0x80` set |
| `0x0091c` | Newer-flash read command path, opcode `0xe8` |
| `0x00a24` | Newer-flash erase command path, opcode `0x50` |
| `0x00a94` | Newer-flash block/load command path, opcode `0x53` |
| `0x00c9c` | Newer-flash program-data command path, opcode `0x84` |
| `0x00e0c` | Newer-flash commit command path, opcode `0x83` |

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

The partition and filesystem setup is FAT-like. The partition sector has
signature bytes `0x55 0xaa`, and DFFS has a default partition-table creation
path. The mount code validates FAT16/FAT32-style boot-sector fields, then
builds caches and directory state for IOMAN calls.

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

Both functions divide the requested byte offset by the geometry unit size from
the `ccdriver#5` info struct, then pass a unit index/remainder into lower-level
command helpers.

The DFFS read wrappers use offsets like:

```text
(partition_start_sector + logical_sector) * bytes_per_sector
```

The initialized sector size observed in the mount path is `0x200`, so DFFS is
effectively presenting a 512-byte-sector FAT-like volume over the Crystal Chip
data flash geometry.

## File Size Reporting

The recovered `dffs.irx` is a legacy IOMAN filesystem driver. Its `dread`
handler copies the FAT directory entry's 32-bit file-size field into
`io_stat_t.size`, but its `getstat` and `chstat` operation slots both point at
the same stub that returns `-1`.

Practical consequence: file sizes for DFFS must come from directory enumeration
metadata, not from a later `fileXioGetStat()` call. The DFFS volume lives on
small Crystal Chip NOR/data flash, so the high 32-bit size field is forced to
zero in wLaunchELF's DFFS listing path.

## Directory Path Quirk

The recovered `dffs.irx` directory-open function stores a private handle, locks
the filesystem semaphore, then resolves the requested path through the FAT-like
path resolver. That resolver handles `""`, `/`, and `/.` as root, but below
root it treats a trailing separator as another component. As a result, `/BM`
resolves to the `BM` directory entry while `/BM/` resolves into `BM` first and
then tries to open an empty final component inside it.

This matches the hardware result where `dffs:/` listed `BM`, but entering
`dffs:/BM/` showed no contents. wLaunchELF now normalizes DFFS directory opens
by removing trailing separators from non-root paths before calling
`fileXioDopen()`.

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

Ghidra confirms the important hook points:

| Function | Behavior |
|---:|---|
| `0x13a8` | Module entry; patches `ioman`, then runs callbacks for watched libraries |
| `0x0734` | Patches IOMAN exports for `open`, `AddDrv`, and `DelDrv`; saves originals at `0x2310`, `0x2308`, `0x230c` |
| `0x03bc` | Hooked IOMAN `open`; normalizes slashes, recognizes `dffs:`, `mass:`, `mc0:`, and `mc1:` paths |
| `0x029c` | Allocates IOP memory, copies an embedded/path module image, loads and starts it through `modload` |
| `0x153c` | Walks the IOP module list at `0x800` by module name |
| `0x14d0` | Walks LOADCORE library exports by library name |

The watched library callback table has five entries:

| Library | Callback | Current meaning |
|---|---:|---|
| `loadcore` | `0x0214` | Patches a LOADCORE export and flushes/relinks it |
| `secrman` | `0x07d4` | Finds an instruction pattern and forces a success return |
| `cdvdman` | `0x0a14` | Applies multiple CDVDMAN code patches controlled by flags at `0x1e00` |
| `modload` | `0x1118` | Hooks module load/start functions so later loads can be patched too |
| `atad` | `0x1244` | Patches ATAD code when the matching pattern is found |

The hooked IOMAN `open` path is the DFFS-specific part. When the path begins
with `dffs:`, it checks whether `CrystalChipDriver` and `DataFlashFS` are
already loaded. If not, and if nonzero module image pointer/size globals are
available, it calls the internal module loader for them. This matches the
developer statement about keeping DFFS accessible, but it also explains why
`ccmodman` is not a clean standalone filesystem dependency: it expects a set of
BootManager globals and an IOP module layout that wLaunchELF does not naturally
provide.

## ccrpc Role

`ccrpc.irx` starts one thread and registers SIF RPC server `0x43434857`
(`CCHW`). Its RPC handler at `0x00e4` maps commands directly to `ccdriver`
ordinals:

| RPC command | `ccdriver` ordinal | Meaning |
|---:|---:|---|
| `0` | `5` | Detect/info; returns chip type, flags, mode, and flash capacity |
| `1` | `6` | Older/simple flash read wrapper |
| `2` | `7` | Older/simple flash write wrapper |
| `3` | `8` | Older/simple flash command/write wrapper |
| `4` | `9` | Crystal Chip command wrapper |
| `5` | `10` | Crystal Chip command wrapper |
| `6` | `12` | Crystal Chip command wrapper |
| `7` | `11` | Crystal Chip command wrapper |
| `8` | `13` | Newer/geometry-backed flash read |
| `9` | `14` | Newer/geometry-backed flash write |
| `10` | `15` | Flash erase/format-style helper |

This is useful for BootManager UI/hardware services, but `DataFlashFS` does not
import or call `ccrpc`.

## Hardware Test Results

Known results from hardware builds:

| Mode | Observed result |
|---|---|
| `Full recovered stack - ccmodman, ccdriver, dffs` | Hangs at `Loading dffs ccdriver drivers` |
| `DataFlashFS only - reset IOP, load dffs` | Hangs at `Loading dffs fs drivers` |
| `DataFlashFS only - preserve IOP, load dffs` | Does not reach the main page |
| `Probe only - preserve IOP, load nothing` | Does not reach the main page |

The two preserve-IOP results argue against a simple "BootManager leaves the
whole DFFS stack resident for us" approach. The reset/direct-load results show
that both the low-level driver startup and the DFFS mount path can stall when
started from the current wLaunchELF runtime.

## Current Working Theory

The hang at `dffs ccdriver` is consistent with this sequence:

1. wLaunchELF resets IOP.
2. Recovered `ccmodman.irx` starts and returns.
3. Recovered `ccdriver.irx` starts.
4. Since no resident `ccdriver` library exists after reset, it enters its own
   hardware detect/self-test path.
5. That path talks to `0xbfc001ad` and retries command transfers.
6. The chip or firmware state does not answer as expected, so the module never
   returns to `SifExecModuleBuffer`.

The strongest remaining direct-load comparison mode is therefore:

```text
BootManager order - ccdriver, ccrpc, dffs
```

That builds:

```text
DFFS=1 DFFS_LOAD_RECOVERED=1 DFFS_LOAD_IOPMAN=0 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=1 DFFS_LOAD_CCRPC=1 IOP_RESET=1
```

This uses the full BM2-sized buffers and matches BM2's direct order for the
Crystal-specific modules.

The other useful comparison mode is:

```text
Firmware iopman - iopman, dffs
```

That loads the firmware `iopman` wrapper unpacked from `IOPMAN.N2E`, then tries
`dffs.irx`.

If both still hang, the next practical software path is not another load-order
permutation. It is a small replacement `ccdriver` that exports only the DFFS
dependency surface (`ccdriver#5`, `#13`, and `#14`), avoids the startup
read/write/restore self-test, uses bounded timeouts on the `0xbfc001ad` byte
protocol, and reports explicit error codes.
