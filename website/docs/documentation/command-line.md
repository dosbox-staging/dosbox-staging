# Command-line usage

The simplest way to launch a game is to pass its path directly to DOSBox
Staging. It's smart enough to figure out what to do: if you pass a
directory, it's mounted as the C: drive; if you pass an executable, its
parent directory is mounted and the program runs automatically.

```
dosbox /path/to/game/
dosbox /path/to/game/GAME.EXE
dosbox disk.img
```

Config files can be layered with `--conf` — later files override earlier
ones. For quick one-off tweaks without editing a file, use `--set`:

```
dosbox --conf game.conf --set cpu_cycles=25000
```

Several `--list-*` options are handy for discovering what's available on
your system: shaders, keyboard layouts, country codes, and code pages.


## Command-line reference

##### PATH

:   If `PATH` is a directory, it's mounted as C:. If it's a bootable disk
    image (IMA/IMG), it's booted. If it's a CD-ROM image (CUE/ISO), it's
    mounted as D:. If it's a DOS executable (BAT/COM/EXE), its parent
    directory is mounted as C: and the executable is run. When the
    executable exits, DOSBox Staging quits.


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

:   Open the primary configuration file in a text editor.


##### `--eraseconf`

:   Delete the primary configuration file.


##### `--noprimaryconf`

:   Don't load or create the primary configuration file.


##### `--nolocalconf`

:   Don't load the local `dosbox.conf` configuration file from the current
    working directory.


##### `--working-dir <path>`

:   Set the working directory for DOSBox Staging. DOSBox will act as if
    started from this directory.


### Startup behaviour

##### `-c <command>`

:   Run the specified DOS command before handling `PATH`. Multiple `-c`
    options can be specified.


##### `--noautoexec`

:   Don't run DOS commands from any `[autoexec]` sections.


##### `--exit`

:   Exit after running `-c` commands and `[autoexec]` sections.


##### `--fullscreen`

:   Start in fullscreen mode.


##### `--lang <lang_file>`

:   Start with the specified language file. Set to `auto` to detect the
    language from the host OS.


##### `--machine <type>`

:   Emulate a specific machine type. The machine type affects both the
    emulated video and sound hardware.

    Possible values: `hercules`, `cga`, `cga_mono`, `tandy`, `pcjr`, `ega`,
    `svga_s3` *(default)*, `svga_et3000`, `svga_et4000`, `svga_paradise`,
    `vesa_nolfb`, `vesa_oldvbe`.


### Discovery

##### `--list-countries`

:   List all supported countries with their numeric codes, for use with the
    `country` config setting.


##### `--list-layouts`

:   List all supported keyboard layouts with their codes, for use with the
    `keyboard_layout` config setting.


##### `--list-code-pages`

:   List all bundled code pages (screen fonts).


##### `--list-shaders`

:   List all available shaders and their paths, for use with the `shader`
    config setting.


### Mapper

##### `--startmapper`

:   Launch the key mapper GUI directly.


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
