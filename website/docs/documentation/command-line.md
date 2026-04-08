# Command-line usage

## Overview

The simplest way to launch a game is to pass its path directly to DOSBox
Staging. It's smart enough to figure out what to do: if you pass a
directory, it's mounted as the C: drive; if you pass an executable, its
parent directory is mounted and the program runs automatically.

```
dosbox /path/to/game/
dosbox /path/to/game/GAME.EXE
dosbox disk.img
```

Config files can be layered with [`--conf`](#-conf-config_file); later files
override earlier ones. For quick one-off tweaks without editing a file, use
[`--set`](#-set-settingvalue):

```
dosbox --conf game.conf --set cpu_cycles=25000
```

Several [`--list-*` options](#discovery) are handy for discovering what's
available on your system: shaders, keyboard layouts, country codes, and code
pages.


## Command-line reference

##### PATH

:   - If `PATH` is a directory, it's mounted as the C: drive.
    - If it's a bootable disk image (IMA or IMG file), it's booted.
    - If it's a CD-ROM image (e.g., an ISO file), it's mounted as the D: drive.
    - If it's a DOS executable (a file with BAT, COM, or EXE extension), its parent directory is mounted as C: and the
    executable is run. When the executable exits, DOSBox Staging quits.


### Configuration

##### `--conf <config_file>`

:   Start with the options specified in `<config_file>`. Multiple `--conf`
    options can be specified; settings from later files override earlier ones.

    Example: `--conf base.conf --conf game.conf`


##### `--set <setting>=<value>`

:   Override a configuration setting. Multiple `--set` options can be
    specified. These take highest priority over all config files.

    Example: `--set mididevice=fluidsynth --set soundfont=gm.sf2`


##### `--printconf`

:   Print the location of the primary configuration file and exit.


##### `--editconf`

:   Open the primary configuration file in the default text editor in the
    terminal (e.g., the one set via `$EDITOR`).


##### `--eraseconf`

:   Delete the primary configuration file.


##### `--noprimaryconf`

:   Don't load the primary configuration file if it exists, and don't create
    and load it if it doesn't exist.


##### `--nolocalconf`

:   Don't load the local `dosbox.conf` configuration file from the current
    working directory.


##### `--working-dir <path>`

:   Set the working directory for DOSBox Staging. DOSBox will act as if
    started from this directory.


### Startup behaviour

##### `-c <command>`

:   Run the specified DOS command before handling [`PATH`](#path). Multiple
    `-c` options can be specified.


##### `--noautoexec`

:   Don't run DOS commands from any [autoexec](autoexec.md) sections.


##### `--exit`

:   Exit after running `-c` commands and `[autoexec]` sections.


##### `--fullscreen`

:   Start in [fullscreen](graphics/display-and-window.md#fullscreen) mode.


##### `--lang <lang_file>`

:   Start with the specified language file. Set to `auto` to detect the
    language from the host OS (this is the default).


##### `--machine <type>`

:   Emulate a specific machine type. The machine type affects both the
    emulated video and sound hardware. See [machine](system/general.md#machine)
    for further details.


### Discovery

##### `--list-countries`

:   List all supported countries with their numeric codes, for use with the
    [country](system/dos.md#country) config setting.


##### `--list-layouts`

:   List all supported keyboard layouts with their codes, for use with the
    [keyboard_layout](system/dos.md#keyboard_layout) config setting.


##### `--list-code-pages`

:   List all bundled code pages (screen fonts).


##### `--list-shaders`

:   List all available shaders and their paths, for use with the
    [shader](graphics/rendering.md#shader) config setting.


### Mapper

##### `--startmapper`

:   Launch the key mapper GUI directly. See
    [Keyboard shortcuts](appendices/shortcuts.md).


##### `--erasemapper`

:   Delete the default mapper file.


### Security & networking

##### `--securemode`

:   Enable secure mode, which disables the `MOUNT` and `IMGMOUNT` commands.


##### `--socket <num>`

:   Run nullmodem on the specified socket number.


### Help

##### `-h`, `-?`, `--help`

:   Print the help message and exit.


##### `-V`, `--version`

:   Print version information and exit.
