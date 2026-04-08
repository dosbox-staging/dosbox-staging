# Storage

DOSBox Staging supports mounting host directories and disk images as DOS
drives. The simplest approach is automounting, which automatically mounts
directories and images from a `drives/` folder structure. For more control, you
can use the `MOUNT` command in the `[autoexec]` section of your configuration
or at the DOS command prompt.


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
or in the `[autoexec]` section of your config file.

!!! note

    `IMGMOUNT` is deprecated. Use `MOUNT` for both directories and disk
    images.


### Mounting directories

```
mount C /path/to/game/files
```

This mounts a host directory as a DOS drive. This is the most common way to
make game files accessible to DOS.


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
mount A cd/*.cue -t iso
```

Press the ++ctrl+f4++ / ++cmd+f4++ hotkeys to cycle between the mounted images
during gameplay. 


### Mounting hard disk images

```
mount C hdd.img -t hdd
```

For images that require explicit geometry:

```
mount C hdd.img -t hdd -chs 304,64,63
```

FAT12, FAT16, and FAT32 filesystems are supported.


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

## Configuration settings

Storage-related settings are in the `[dosbox]` section:

<div class="compact" markdown>

- [automount](system/dos.md#automount)
- [hard_disk_speed](system/dos.md#hard_disk_speed)
- [floppy_disk_speed](system/dos.md#floppy_disk_speed)

</div>
