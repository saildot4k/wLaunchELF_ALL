# __wLaunchELF_R3Z__

Based off of [wLE_ISR](https://github.com/israpps/wLaunchELF_ISR)

# Supported devices:
| Device | Description | Visibility |
|---|---|---|
| __mc:/__ | Memory Cards | Always |
| __usb:/__ | Fat/exFAT (BDM) USB | Always |
| __mmce:/__ | Multi Purpose Memory Card Emulator IE SD2PSX, PSxMemCard Gen2 or MemCard Pro 2. | Always |
| __mx4sio:/__ | SD card interface over memory card port. | Always |
| __hdd:/__ | APA formatted internal HDD | Hidden on deckard |
| __ata:/__ | BDM hard drive, exFAT for now till more are supported | Hiddon on deckard |
| __xfrom:/__ | PSX DESR-XXXX flash storage | Hidden on non-PSX |
| __dvr_hdd0:/__ | PSX DESR-XXXX digital video recorder hdd partition/side | Hidden on non-PSX |
| __cdfs:/__ | CD/DVD File System | Always |
| __udpfs:/__ | Network interface used with [PCM720s UDPFSD Server](https://github.com/pcm720/udpfsd) | Always |

Drivers are lazy loading for for maximum compatibility. MMCE and MX4SIO will incure an IOP reboot as the 2 are incompatible.

Dual HDD/ATA support is built in for future development.

## Features:
- RetroGem Game ID for [PIXEL FX RetroGem](https://www.pixelfx.co/hdmi-retro-gem)
- Launch PS1 VCDs with option for custom POPStarter path, falls back to defaults.
- Writes to history file for disc launches for mmce vmc change
- Applies deckard disc patches
- X/0 applied per region if no config file is found so O is only default for Japan
- Keyboard layout choices: QWERTY, DVORAK, AZERTY, QWERTZ, ABNT, ABC
- Language built: English, Spanish, Italian, French, German, Polish, Portuguese, Portuguese Brazilian, Hungarian
- All config options exposed in gui
- HDD/ATA drives hidden for deckard ps2 (SCPH-75K+)
- Xfrom/dvr_hdd0 hidden from non-PSX consoles
- Dual hdd support
- multi-usb support
- HDD Manager can inject APA partition headers from header files on `usb:`, `mmce0:`, `mmce1:`, or `udpfs:` for HDD-OSD icons/launch metadata
- All drivers besides core lazy loading for faster boot and support of everything
- Create and Extract PSU options so user knows what will happen
- Extra file extensions for Text Editor ShortCuts
- Timestamp manipulation feature to fix the date of any memory card folder containing any icon-based exploit _(\*tuna)_
- Launch KELFs (encrypted elfs)
- APA Header injection
- Support for PS3/PS4 Dualshocks thanks to Alex Parrado (DS34 build)

### HDD APA header injection

The HDD Manager R1 menu includes `Inject Header` for PFS partitions.
Source devices are `usb:`, `mmce0:`, `mmce1:`, and `udpfs:`.

Header files must be placed in a flat folder matching the partition name:

```text
device:/__Headers/<matching partition name>/
  system.cnf   required
  icon.sys     required on PS2
  list.ico     required on PS2
  boot.kelf    optional, or BOOT.KELF
  info.sys     optional, may be required for PSBBN/PSX XMB
  jkt_001.png  optional, used by PSBBN
  jkt_002.png  optional, used by PSX XMB
  jkt_cp.png   optional copyright image, used by PSX XMB
  BOOT.ELF     optional, useful with a KELF forwarder
```

If present, `info.sys`, `jkt_001.png`, `jkt_002.png`, and `jkt_cp.png` are copied
into the partition PFS `res/` folder. If present, `BOOT.ELF` is copied to the
partition PFS root.

Files injected into the APA header have fixed space limits:

```text
system.cnf   512 bytes
icon.sys     1,024 bytes
list.ico     1,112,064 bytes
boot.kelf    978,944 bytes, or BOOT.KELF
```

PFS resource files are copied from the same flat source folder. They are not
written into the APA header:

```text
info.sys     copied to pfs0:/res/info.sys
jkt_001.png  copied to pfs0:/res/jkt_001.png
jkt_002.png  copied to pfs0:/res/jkt_002.png
jkt_cp.png   copied to pfs0:/res/jkt_cp.png
BOOT.ELF     copied to pfs0:/BOOT.ELF
```

Expected PNG formats:

```text
jkt_001.png  256x256 PNG, 8-bit indexed color, used by PSBBN
jkt_002.png  76x108 PNG, 8-bit indexed color with 32-bit RGBA palette, used by PSX XMB
jkt_cp.png   290 pixels wide, 46-300 pixels high, 32-bit RGBA PNG, used by PSX XMB
```

`jkt_cp.png` is the PSX XMB copyright image. The PSX XMB displays a 46-pixel
high area; taller images scroll vertically.

Example `info.sys`:

```ini
title = [SYS] R3CONFIGURATOR
title_id = SYS-R3CONFI
title_sub_id = 0
release_date =
developer_id =
publisher_id = pcm720, R3Z3N
note =
content_web =
image_topviewflag = 0
image_type = 0
image_count = 1
image_viewsec = 600
copyright_viewflag = 0
copyright_imgcount = 0
genre =
parental_lock = 1
effective_date = 0
expire_date = 0
violence_flag = 0
content_type = 255
content_subtype = 0
```

### Build shortcuts

- `make all-ds34-off` builds a dedicated ELF without DS34 support.
- `make all-ds34-on` builds a dedicated ELF with DS34 support.
- `make all-ds34-variants` builds both variants in one run.

# **original readme**
wLaunchELF, formerly known as uLaunchELF, also known as wLE or uLE (abbreviated), is an open source file manager and executable launcher for the Playstation 2 console based off of the original LaunchELF. It contains many different features, including a text editor, hard drive manager, as well as network support, and much more.
