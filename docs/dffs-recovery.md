# DFFS Recovery Notes

These notes summarize the `dffs:/` driver recovered from
`BM2UNPACKED.ELF` from Crystal Chip BootManager. The analyzed unpacked ELF has
SHA-256 `38ab7ede9d519156ebda08ec3178886d399b8657a394bc9e17f4069a2efaefe1`.

## Recovered Modules

The BootManager ELF contains embedded IOP IRX modules for Crystal Chip flash
access. The practical filesystem stack is:

| Module | Recovered file | SHA-256 | Purpose |
|---|---|---|---|
| `CrystalChipDriver` | `iop/__precompiled/ccdriver.irx` | `45def2973dbaa060fcb9423464d4bfbd5d5f3e583fd74eabd39a4a48a8075eda` | Low-level Crystal Chip flash driver |
| `DataFlashFS` | `iop/__precompiled/dffs.irx` | `29b0179ef589cc707cb7c0640060ab9ec2696c3e249111cc99f4ef9087d3940c` | IOMAN filesystem driver registering `dffs` |

In the unpacked BootManager ELF used for recovery, the unique embedded module
extents were:

| Module | ELF offset | Size |
|---|---:|---:|
| `CrystalChipDriver` | `0x2391a0` | `9056` bytes |
| `DataFlashFS` | `0x23c120` | `21036` bytes |

`DataFlashFS` imports `ioman`, `sysmem`, `sysclib`, `thbase`, `stdio`,
`thsemap`, `intrman`, and `ccdriver`. The repository's embedded
`iomanX.irx` includes the legacy `ioman` export name, so no extra IOMAN
compatibility module is required. The `ccrpc` module found in BootManager is
not imported by `DataFlashFS` and is not needed for `dffs:/` file access.

Raw IRX import/export table inspection shows that `DataFlashFS` imports three
functions from the `ccdriver` library by ordinal: `5`, `13`, and `14`.
`CrystalChipDriver` exports the 8-byte library name `ccdriver`, matching those
imports.

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

`DFFS=1` embeds `ccdriver.irx` and `dffs.irx` and exposes `dffs:/` in the
FileBrowser root. Access uses the existing generic `fileXio` operations for
directory listing, file open/read/write, mkdir, remove, rename, and recursive
copy. The generic file-operation wrappers also ensure the DFFS stack is loaded
before direct `dffs:` opens, stats, mkdirs, removes, and directory opens.
Timestamp `ChStat` is skipped on copy, matching the treatment of other small
FAT-like device stacks that may not implement timestamp updates.

ELF launch from `dffs:/` is supported through the normal loader handoff. The
embedded loader reads the target ELF with `SifLoadElf()` before optional IOP
reset, so the DFFS driver only needs to be present before handoff.
