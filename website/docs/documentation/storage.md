# Storage

## How DOS uses drives

DOS identifies storage devices by drive letters, not by names or mount points.
Each drive letter (A: through Z:) maps to exactly one storage device or
partition. Unlike modern operating systems, there is no single unified file
tree --- each drive is its own independent root.

The letter assignments follow a fixed convention:

- **A:** and **B:** are reserved for floppy disk drives. If only one physical
  floppy drive is present, B: acts as a "phantom" mapped to the same drive
  --- DOS prompts you to swap disks when you access it.

- **C:** is the first hard disk partition. A single physical hard disk can be
  divided into multiple partitions, each getting its own letter (C:, D:, E:,
  etc.).

- The **first available letter after the hard disk partitions** is typically
  assigned to the CD-ROM drive. On a system with a single hard disk
  partition, the CD-ROM is usually D:; with two partitions, it becomes E:,
  and so on.

In DOSBox Staging, you assign drive letters yourself when you mount
directories or disk images, so you have full control over the layout. The
conventions above are worth following because many games assume them ---
especially C: for the hard disk and D: for the CD-ROM.


## DOS storage media

If you grew up with modern computers where everything lives on a single
internal SSD, the DOS storage landscape might seem alien. DOS PCs used three
types of storage media, each with its own drive letter:

**Floppy disks** (drives A: and B:) were the primary way software was
distributed. Games came on one or more floppies and usually had to be
**installed** to the hard disk before you could play --- unlike a console
cartridge, you couldn't just insert a floppy and go. Two form factors
existed: the flexible 5.25-inch disk (1981--late 1980s) and the sturdier
3.5-inch disk (late 1980s--mid 1990s). See
[Floppy disk formats](#floppy-disk-formats) for the full list of supported
sizes.

**Hard disks** (drive C: and up) held the operating system and installed
games. Capacities ranged from 10--20 MB in 1985 to over 1 GB by the
mid-1990s. The DOS FAT16 filesystem limits individual partitions to **2 GB**
--- larger disks needed multiple partitions.

**CD-ROMs** (typically drive D:) appeared in the early 1990s and became
mainstream by 1994--1995. A single CD held 650--700 MB --- roughly 500
floppy disks' worth. This enabled full-motion video, voice acting, and
CD-quality audio soundtracks. Some games ran entirely from the CD; others
installed partially to the hard disk.


## DOSBox Staging drives

DOSBox Staging provides two special drives in addition to whatever you mount:

- **Z:** contains the built-in programs and commands (MOUNT, CONFIG, MIXER,
  etc.). It is always present and always in the PATH. See
  [DOS commands & programs](commands.md) for the full list.

- **Y:** contains a small set of bundled third-party utilities (DEBUG,
  DELTREE, XCOPY). It is [automounted](system/general.md#automount) by
  default with its `Y:\DOS` directory added to the PATH.


## Automounting

The easiest way to set up drives is automounting. Place your game files in a
`drives/` directory structure, and DOSBox mounts them automatically on
startup. The
[Getting Started guide](../getting-started/setting-up-prince-of-persia.md#the-c-drive)
demonstrates this approach step by step.


### Directory structure

Create lowercase single-letter subdirectories under `drives/` to mount as DOS
drives:

```
drives/
  c/          -> mounted as C:
  d/          -> mounted as D:
```

The `drives/` folder is looked up relative to the current working directory or
from the built-in resources directory.


### Floppy and CD-ROM images

Automounting also supports disk images for floppy and CD-ROM drives:

- Place IMG or IMA floppy images in `drives/a/` or `drives/b/` to mount
  them as floppy drives.
- Place ISO, CUE/BIN, or MDS/MDF CD-ROM images in any drive directory (e.g.,
  `drives/d/`) to mount them as CD-ROM drives.

When image files are detected, DOSBox automatically uses the appropriate mount
type.


### Drive configuration files

Each drive directory can have an accompanying configuration file named
`[letter].conf` (e.g., `c.conf`) placed alongside the directory. Example:

```ini
[drive]
type     = dir
label    = MYGAME
readonly = off
```

Available options:

| Option             | Values                                        | Description                                       |
|--------------------|-----------------------------------------------|---------------------------------------------------|
| `type`             | `dir`, `overlay`, `floppy`, `cdrom`, `iso`    | Drive type. `iso` is an alias for `cdrom`.        |
| `label`            | any string                                    | Custom volume label.                              |
| `path`             | PATH specification                            | Extend the DOS PATH (e.g., `path = %path%;c:\tools`). |
| `override_drive`   | single letter (a--y)                          | Mount to a different drive letter.                |
| `readonly`         | `on`, `off`                                   | Mount as read-only.                               |
| `verbose`          | `on`, `off`                                   | Show mount command output during startup.         |


## Manual mounting

For more control, use the `MOUNT` command directly, either at the DOS prompt
or in the `[autoexec]` section of your config file. Type `MOUNT /?` at the
DOS prompt for the full reference.

!!! note

    `IMGMOUNT` is deprecated. Use `MOUNT` for both directories and disk
    images.


### Mounting directories

```
mount C /path/to/game/files
```

This mounts a host directory as a DOS hard disk drive. This is the most common
way to make game files accessible to DOS.

By default, directory-mounted hard disks report approximately 250 MB of free
space. Some games check for free disk space and refuse to install if there
isn't enough. Use the `-freesize` flag to adjust the reported free space (in
MB for hard disks, KB for floppies):

```
mount C /path/to/game -freesize 1024
```

#### Volume labels

Some games --- particularly those with copy protection --- check the volume
label of the drive. Use the `-label` flag to set it explicitly:

```
mount A /path/to/floppy -t floppy -label DISK1
```

This is most commonly needed for floppy-based games that verify the disk
label during copy protection checks.


#### Overlay mounts

An overlay mount adds a write layer on top of an existing drive. Modified
files are stored in the overlay directory on the host, leaving the original
drive data unchanged. This is useful for keeping a clean copy of game files
while still allowing the game to save data:

```
mount C /path/to/game
mount C /path/to/saves -t overlay
```


### Mounting floppy images

```
mount A floppy.img -t floppy
```

Supported floppy image formats: `.img`, `.ima` (raw sector images).

#### Floppy disk formats

The 5.25-inch disk was a flexible magnetic disk visible through a slot in its
cardboard sleeve. The 3.5-inch disk housed the magnetic medium in a rigid
plastic shell with a spring-loaded metal shutter. Capacities grew as
recording density improved: **DD** (Double Density), **HD** (High Density),
and **ED** (Extra Density) describe the magnetic coating and recording
method. Some software distributors squeezed extra capacity using non-standard
formats like Microsoft's **DMF** (Distribution Media Format) at 1.68 MB or
IBM's **XDF** at 1.84 MB.

DOSBox Staging supports all standard PC floppy formats and several extended
formats. The format is detected automatically from the image file size.

<div class="compact" markdown>

| Size   | Type  | Capacity | Notes                             |
|--------|-------|----------|-----------------------------------|
| 5.25"  | SS/DD | 160 KB   | Original IBM PC (1981)            |
| 5.25"  | SS/DD | 180 KB   | 9 sectors/track variant           |
| 5.25"  | DS/DD | 320 KB   | Double-sided (1982)               |
| 5.25"  | DS/DD | 360 KB   | Standard 5.25" DD                 |
| 5.25"  | DS/HD | 1.2 MB   | Standard 5.25" HD                 |
| 3.5"   | DS/DD | 720 KB   | Standard 3.5" DD                  |
| 3.5"   | DS/HD | 1.44 MB  | Standard 3.5" HD                  |
| 3.5"   | DS/HD | 1.68 MB  | Microsoft DMF                     |
| 3.5"   | DS/HD | 1.72 MB  | Microsoft DMF (82-track variant)  |
| 3.5"   | DS/ED | 2.88 MB  | Extra Density (rare)              |

</div>

### Mounting CD-ROM images

```
mount D game.cue -t iso
mount D game.iso -t iso
mount D game.mds -t iso
```

Supported CD-ROM image formats:

- **CUE/BIN** --- CUE sheet with data and optional audio tracks.
- **ISO** --- Standard ISO 9660 images (data only).
- **MDS/MDF** --- Alcohol 120% disc images.

For games with CD-DA music, use CUE/BIN or MDS/MDF format images with audio
tracks. See [CD-DA audio](sound/sound-devices/cd-da.md) for details on
supported audio track formats.


### Mounting multiple images

You can mount multiple images at the same drive letter:

```
mount A disk1.img disk2.img disk3.img -t floppy
```

Wildcard patterns are also supported:

```
mount D cd/*.cue -t iso
```

Press the ++ctrl+f4++ / ++cmd+f4++ hotkeys to cycle between the mounted images
during gameplay. This is essential for multi-disk games that prompt you to
insert the next disk.


### Mounting hard disk images

```
mount C hdd.img -t hdd
```

For images that require explicit geometry:

```
mount C hdd.img -t hdd -chs 304,64,63
```

FAT12, FAT16, and FAT32 filesystems are supported.


### Read-only mounts

Use the `-ro` flag to mount a drive as read-only:

```
mount D /path/to/shared -ro
```


### Booting from images

For bootable disk images (real MS-DOS, Windows 9x, etc.), mount with
`-fs none` using a drive number instead of a letter:

```
mount 0 boot.img -t floppy -fs none
boot -l a
```

```
mount 2 win95.img -t hdd -fs none -chs 304,64,63
boot -l c
```

Drive numbers 0 and 1 correspond to floppy drives A: and B:; 2 and 3
correspond to hard disks C: and D:.

!!! note "Booter games"

    Some early games (roughly 1981--1986) bypassed DOS entirely and booted
    directly from floppy disk. These "booter" games took over the whole
    machine, using non-standard disk formats as a form of copy protection
    and to squeeze out every last byte of available memory. They need the
    `boot` command shown above rather than `mount`.

??? note "Notable booter games"

    <div class="compact" markdown>

    - [Alley Cat (1984)](https://www.mobygames.com/game/190/alley-cat/)
    - [Archon (1984)](https://www.mobygames.com/game/227/archon-the-light-and-the-dark/)
    - [BC's Quest for Tires (1983)](https://www.mobygames.com/game/6561/bcs-quest-for-tires/)
    - [Bouncing Babies (1984)](https://www.mobygames.com/game/11736/bouncing-babies/)
    - [Dig Dug (1983)](https://www.mobygames.com/game/7563/dig-dug/)
    - [Flight Simulator 2.0 (1984)](https://www.mobygames.com/game/1552/microsoft-flight-simulator-20/)
    - [Hitchhiker's Guide to the Galaxy (1984)](https://www.mobygames.com/game/252/the-hitchhikers-guide-to-the-galaxy/)
    - [Karateka (1986)](https://www.mobygames.com/game/1583/karateka/)
    - [King's Quest (1984)](https://www.mobygames.com/game/123/kings-quest/)
    - [Lode Runner (1983)](https://www.mobygames.com/game/17011/lode-runner/)
    - [M.U.L.E. (1983)](https://www.mobygames.com/game/1826/mule/)
    - [Pac-Man (1983)](https://www.mobygames.com/game/13851/pac-man/)
    - [Paratrooper (1982)](https://www.mobygames.com/game/795/paratrooper/)
    - [Zork I (1982)](https://www.mobygames.com/game/4/zork-the-great-underground-empire/)

    </div>


### DOS filesystem limitations

DOS filenames follow the **8.3** convention: a maximum of 8 characters for
the name and 3 for the extension (e.g., `DUKE3D.EXE`). Names are
case-insensitive, cannot contain spaces, and only a limited set of special
characters is allowed. When mounting host directories, DOSBox Staging
automatically truncates long filenames to 8.3 format for DOS programs.

The FAT16 filesystem used by DOS limits individual partitions to a maximum
of **2 GB**. Larger hard disk images must be split into multiple partitions.
DOSBox Staging also supports FAT12 (used on floppy disks) and FAT32
(used by Windows 95/98, with a theoretical 2 TB limit).


### Unmounting drives

To unmount a previously mounted drive:

```
mount -u D
```


## Configuration settings

Storage-related settings are in the `[dosbox]` section:

<div class="compact" markdown>

- [`automount`](system/general.md#automount)
- [`hard_disk_speed`](system/general.md#hard_disk_speed)
- [`floppy_disk_speed`](system/general.md#floppy_disk_speed)

</div>

For emulated drive sounds (floppy chatter, hard disk clicking), see
[Disk noise](sound/disk-noise.md).
