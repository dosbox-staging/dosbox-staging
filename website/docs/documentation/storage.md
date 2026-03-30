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
internal SSD, the DOS storage landscape might seem alien. Here's a quick
overview of the media types you'll encounter.

**Floppy disks** were the primary removable storage medium throughout the DOS
era. The two main form factors were the 5.25-inch disk (common in the early
to mid 1980s; 360 KB or 1.2 MB capacity) and the 3.5-inch disk (from the
late 1980s onwards; 720 KB or 1.44 MB). Many games shipped on multiple floppy
disks --- it wasn't unusual for a late-era game to come on 10 or more disks
that had to be inserted one at a time during installation.

**Hard disks** became common in home PCs from the mid-1980s. Capacities grew
rapidly: 10--20 MB was typical around 1985, 40--120 MB by the early 1990s,
and 500 MB to 1 GB by the mid-1990s. Unlike console cartridges, most DOS
games had to be **installed** --- files were copied from the floppy disks (or
later, CD-ROM) onto the hard disk before you could play. This installation
step was a standard part of the PC gaming experience.

**CD-ROMs** appeared in the early 1990s and became mainstream by 1994--1995.
A single CD could hold 650--700 MB --- the equivalent of roughly 500 floppy
disks. This enormous capacity enabled full-motion video, CD-quality audio
soundtracks, and voice acting that simply wasn't possible on floppies. Some
games ran entirely from the CD; others required a partial installation to the
hard disk while still reading data from the disc during gameplay.


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
`drives/` directory structure, and DOSBox mounts them automatically on startup.


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

Supported floppy image formats: `.img`, `.ima` (raw sector images). Common
sizes from 360 KB (5.25" DD) to 2.88 MB (3.5" ED) are automatically detected.

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
