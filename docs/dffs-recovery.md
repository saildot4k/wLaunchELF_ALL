# DFFS Recovery Notes

These notes summarize the `dffs:/` driver recovered from
`BM2UNPACKED.ELF` from Crystal Chip BootManager. The analyzed unpacked ELF has
SHA-256 `38ab7ede9d519156ebda08ec3178886d399b8657a394bc9e17f4069a2efaefe1`.
For reverse-engineering notes on the recovered IRX modules, see
[`cc-dffs-driver-analysis.md`](cc-dffs-driver-analysis.md).

## Recovered Modules

The BootManager ELF contains embedded IOP IRX modules for Crystal Chip flash
access. The practical filesystem stack is:

| Module | Recovered file | SHA-256 | Purpose |
|---|---|---|---|
| `modman` | `iop/__precompiled/ccmodman.irx` | `398f0b24f4fb430ff426b4c4f9c86e4fd66ae4275da5ac1f5f7ebbc7fa4843f3` | Crystal Chip BootManager support module that references DFFS modules and device paths |
| `CrystalChipDriver` | `iop/__precompiled/ccdriver.irx` | `fce71823295a13f737856b056d2a638e3df1044d22e16fb87a4e9bba21518028` | Low-level Crystal Chip flash driver |
| `ccrpc` | `iop/__precompiled/ccrpc.irx` | `97f6f40f9f5409daa7e53ccbbb89cb8d470018783a05b47ef9e0a245e335d040` | Crystal Chip RPC service used by BootManager, not required by `DataFlashFS` |
| `DataFlashFS` | `iop/__precompiled/dffs.irx` | `5bed54983a9a09a1bdf121ed4957e3eafd1b82198b6bb11e3f6a0eacd6329380` | IOMAN filesystem driver registering `dffs` |

The original quick recovery found short inner ELF images that omitted the
relocation data needed by a normal module loader:

| Module | ELF offset | Size |
|---|---:|---:|
| `modman` | `0x235d20` | `12596` bytes |
| `CrystalChipDriver` | `0x2391a0` | `9056` bytes |
| `DataFlashFS` | `0x23c120` | `21036` bytes |

The BootManager direct EE loader uses larger buffers whose size word lives just
before each buffer. Those are the buffers now checked in for direct load tests:

| Module | BM2 buffer offset | Size word | SHA-256 |
|---|---:|---:|---|
| `CrystalChipDriver` | `0x24df30` | `0x2f69` | `fce71823295a13f737856b056d2a638e3df1044d22e16fb87a4e9bba21518028` |
| `ccrpc` | `0x250eb0` | `0x0929` | `97f6f40f9f5409daa7e53ccbbb89cb8d470018783a05b47ef9e0a245e335d040` |
| `DataFlashFS` | `0x2517f0` | `0x648d` | `5bed54983a9a09a1bdf121ed4957e3eafd1b82198b6bb11e3f6a0eacd6329380` |

`modman` imports `loadcore`, `modload`, `sysmem`, `stdio`, `sifcmd`,
`sysclib`, `thbase`, and `ioman`. Its string table includes
`CrystalChipDriver`, `DataFlashFS`, `dffs:`, and `ccdriver`, which matches
Crystal Chip developer notes that BootManager took extra steps to keep DFFS
available.

`DataFlashFS` imports `ioman`, `sysmem`, `sysclib`, `thbase`, `stdio`,
`thsemap`, `intrman`, and `ccdriver`. The repository's embedded `iomanX.irx`
includes the legacy `ioman` export name, so no extra IOMAN compatibility module
is required. The `ccrpc` module found in BootManager is not imported by
`DataFlashFS` and is not needed for normal `dffs:/` file access.

Raw IRX import/export table inspection shows that `DataFlashFS` imports three
functions from the `ccdriver` library by ordinal: `5`, `13`, and `14`.
`CrystalChipDriver` exports the 8-byte library name `ccdriver`, matching those
imports.

`ccrpc` imports many `ccdriver` ordinals, but `DataFlashFS` does not import
`ccrpc`. That makes `ccrpc` a BootManager service compatibility test, not a
base filesystem requirement.

## Filesystem Behavior

The driver identifies itself with:

```text
Crystal Chip Flash File System v%X.%X
```

It registers the device name `dffs` and also contains the string `flashfs`.
The filesystem code is FAT-like and includes a default FAT16 boot sector
template with `MSWIN4.0`, `NO NAME`, and `FAT16` strings. This makes DFFS
closer to a tiny FAT16 volume over Crystal Chip data flash than to PS2 memory
card `mcfs`.

Public Crystal Chip BootManager notes also treat DFFS as a Crystal Chip 2.0
storage path and describe BootManager scripts with DFFS-specific run/install
handling:
https://ps2modchiptutorials.com/crystal-chips/cc-files/

The recovered driver contains messages for:

- missing Crystal Chip detection
- invalid partition sector signature
- overlapping partitions
- unsupported partition type
- invalid FAT16/FAT32 boot-sector fields
- creating a default partition table
- formatting flash after mount failure

The format path is important. The driver string:

```text
DFFS: Failed to mount, formatting...
```

indicates that loading/accessing the driver may format Crystal Chip data flash
if no valid DFFS volume is found. wLaunchELF_R3Z therefore lazy-loads the stack
only when a `dffs:` path is used.

String and entry-prologue inspection did not reveal any module argument or flag
to suppress this format path.

## Integration Model

`DFFS=1` exposes `dffs:/` in the FileBrowser root. With
`DFFS_LOAD_RECOVERED=1`, wLaunchELF_R3Z embeds and attempts the recovered
BootManager module order: `ccmodman.irx`, `ccdriver.irx`, then `dffs.irx`.
The loading screen identifies those stages as `dffs modman`, `dffs ccdriver`,
and `dffs fs` so non-debug hardware tests can report which module stalls. A
Crystal Chip hardware test reached `dffs ccdriver` and then hung, which means
the recovered `modman` entry returned but the recovered `ccdriver` module start
did not.

The load sequence is configurable for hardware experiments:

| GitHub Actions `dffs_mode` | Driver load behavior | Purpose |
|---|---|---|
| `Full recovered stack - ccmodman, ccdriver, dffs` | Load `ccmodman`, `ccdriver`, then `dffs` after IOP reset | Original recovered order; known to reach and hang at `dffs ccdriver` on test hardware |
| `Skip ccdriver - ccmodman, dffs` | Load `ccmodman`, then `dffs` after IOP reset | Test whether `modman` supplies or reloads `ccdriver` without direct EE-side `ccdriver` startup |
| `Firmware iopman - iopman, dffs` | Load unpacked firmware `iopman`, then `dffs` after IOP reset | Test the firmware wrapper module recovered from `IOPMAN.N2E` |
| `BootManager order - ccdriver, ccrpc, dffs` | Load full BM2 `ccdriver`, `ccrpc`, then `dffs` after IOP reset | Closest direct EE-side match to BM2's recovered module order |
| `DataFlashFS only - reset IOP, load dffs` | Load only `dffs` after IOP reset | Control test for whether a resident `ccdriver` exists after the normal wLaunchELF IOP stack |
| `DataFlashFS only - preserve IOP, load dffs` | Do not reset IOP; load only `dffs` | Test whether BootManager left `ccdriver` resident |
| `Probe only - preserve IOP, load nothing` | Do not reset IOP or start recovered DFFS modules; only probe existing `dffs:` | Test whether BootManager left the whole DFFS stack resident |
| `DFFS disabled` | Build without `dffs:/` support | Disable DFFS integration |

Equivalent make flags:

| Build flags | Driver load behavior | Purpose |
|---|---|---|
| `DFFS_LOAD_RECOVERED=1` | Load `ccmodman`, `ccdriver`, then `dffs` | Full recovered BootManager-like sequence |
| `DFFS_LOAD_RECOVERED=1 DFFS_LOAD_CCDRIVER=0` | Load `ccmodman`, then `dffs` | Test whether `modman` or firmware already supplies `ccdriver` |
| `DFFS_LOAD_RECOVERED=1 DFFS_LOAD_IOPMAN=1 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=0` | Load firmware `iopman`, then `dffs` | Test the official firmware wrapper after normal IOP startup |
| `DFFS_LOAD_RECOVERED=1 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=1 DFFS_LOAD_CCRPC=1` | Load BM2 direct order: `ccdriver`, `ccrpc`, then `dffs` | Test whether BootManager's direct runtime stack is order-sensitive |
| `DFFS_LOAD_RECOVERED=1 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=0 IOP_RESET=1` | Load only `dffs` after reset | Control test; expected to fail unless `ccdriver` is already resident |
| `DFFS_LOAD_RECOVERED=1 DFFS_LOAD_CCMODMAN=0 DFFS_LOAD_CCDRIVER=0 IOP_RESET=0` | Load only `dffs` after preserving the current IOP | Test whether BootManager left `ccdriver` resident |
| `DFFS_LOAD_RECOVERED=0 IOP_RESET=0` | Do not start recovered DFFS modules; only probe existing `dffs:` | Test whether BootManager left the whole DFFS stack resident |

Current hardware results:

- `DataFlashFS only - preserve IOP, load dffs` did not reach the main page.
- `Probe only - preserve IOP, load nothing` did not reach the main page.
- `DataFlashFS only - reset IOP, load dffs` reached `Loading dffs fs drivers`
  and hung.
- `Full recovered stack - ccmodman, ccdriver, dffs` reached
  `Loading dffs ccdriver drivers` and hung.
- A later full-stack-style test could list `dffs:/` and showed `BM` at the
  root, but `dffs:/BM/` appeared empty.

Those results make a simple "preserve BootManager's IOP" approach unlikely.
The strongest remaining software paths are to retest the full BM2-sized buffers
and then replace `ccdriver` with a bounded, non-destructive implementation.

Access uses the existing generic `fileXio` operations for directory listing,
file open/read/write, mkdir, remove, rename, and recursive copy. The generic
file-operation wrappers also ensure the DFFS stack is loaded before direct
`dffs:` opens, stats, mkdirs, removes, and directory opens. Timestamp `ChStat`
is skipped on copy, matching the treatment of other small FAT-like device
stacks that may not implement timestamp updates.

One recovered `dffs.irx` compatibility issue is now handled in wLaunchELF:
the DFFS path resolver accepts a child directory as `/BM`, but a trailing slash
path such as `/BM/` reaches the resolver as an empty final component inside
`BM`. The browser stores directory paths with a trailing slash, so DFFS
directory opens now strip the final separator for non-root paths before calling
`fileXioDopen()`. The user-facing path remains `dffs:/BM/`.

File size handling is also DFFS-specific. The recovered driver reports file
sizes through `dread()` directory entries, but its `getstat` slot is a `-1`
stub. For DFFS, wLaunchELF therefore treats the listed 32-bit FAT file size as
authoritative, forces the high size word to zero, and uses cached listing sizes
when recursively calculating folder totals. Because the recovered driver does
not reliably pass through FAT attributes for long-filename or other pseudo
directory records, wLaunchELF also rejects non-printable DFFS names, raw-looking
LFN records, and any DFFS file or recursive folder size above the 4 MB maximum
Crystal Chip flash size.

ELF launch from `dffs:/` is supported through the normal loader handoff. The
embedded loader reads the target ELF with `SifLoadElf()` before optional IOP
reset, so the DFFS driver only needs to be present before handoff.
