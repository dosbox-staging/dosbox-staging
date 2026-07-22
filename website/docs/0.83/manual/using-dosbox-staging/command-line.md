# Command-line usage

DOSBox Staging can be invoked from your host operating system's command line
with a path, disk image, or set of options. This provides a convenient way to
launch DOS software quickly, control startup behaviour, apply temporary
configuration overrides, and discover available resources.

For most users, the recommended approach is to organise each game in its own
folder and start DOSBox Staging from that folder. This allows the emulator to
automatically use the appropriate [working
directory](starting.md#the-working-directory), [local
configuration](configuration.md#local-configuration), and
[automounting](storage.md#automounting) features. See [Starting the
emulator](starting.md) for the recommended workflow.

The sections below cover common ways of invoking DOSBox Staging first,
followed by temporary overrides, resource discovery, and finally the complete
command-line reference.


## Quick launch

The simplest way to start DOSBox Staging with a game is to pass the game's
path directly to the executable. This is useful for quickly testing software
or launching a game without setting up a full per-game configuration.

- If you pass a folder, it is made available as the [C: drive](storage.md#dos-drive-letters).

- If you pass a DOS executable, its parent folder is made available as the
  **C:** drive and the program starts automatically.

- If you pass a bootable disk image, DOSBox Staging boots from it.

Examples:

```
dosbox /path/to/game/
dosbox /path/to/game/GAME.EXE
dosbox disk.img
```


### Specifying paths on Windows

When specifying a path using quotation marks on Windows, you must omit the
trailing `\` or use double backslashes before the final quotation mark. For
example:

```
dosbox "C:\Users\My User\My Game"
dosbox "C:\Users\My User\My Game\\"
```

The following will **not** work as the final `\"` will translate to a literal
quotation mark.

```
dosbox "C:\Users\My User\My Game\"
```

## Temporary overrides

Command-line options can be used to temporarily change how DOSBox Staging
starts without modifying configuration files.

Configuration files can be [layered](configuration.md#configuration-layering)
with the [`--conf <config__file>`](#-conf-config_file) option; later files
override earlier ones. This is useful for applying additional settings on top
of an existing configuration:

```
dosbox --conf extra.conf
```

For quick one-off changes, use [`--set
<setting>=<value>`](#-set-settingvalue). Multiple settings can be supplied by
using multiple `--set` options:

```
dosbox --set machine=ega --set cpu_cycles=5000
```

These approaches can also be combined:

```
dosbox --conf extra.conf --set cpu_cycles=25000
```

See [Configuration layering](configuration.md#configuration-layering) for
details on how settings from different sources are combined.


## Discovery

DOSBox Staging includes several [`--list-*` options](#discovery_1) that help
discover what is available on your system, including shaders, keyboard
layouts, country codes, and code pages.


## Command-line reference

You can optionally pass a `PATH` parameter and a list of options to the DOSBox
Staging executable:

```
dosbox [OPTIONS...] [PATH]
```

##### PATH

:   - If `PATH` is a directory, it's mounted as the C: drive.
    - If it's a [bootable disk image](storage.md#booting-from-images) (IMA or
      IMG file), it's booted.
    - If it's a [CD-ROM image](storage.md#mounting-cd-rom-images) (e.g., an ISO
      file), it's mounted as the D: drive.
    - If it's a DOS executable (a file with BAT, COM, or EXE extension), its
      parent directory is mounted as C: and the executable is run. When the
      executable exits, DOSBox Staging quits.


### Configuration

##### `--conf <config_file>`

:   Start with the options specified in `<config_file>`. Multiple `--conf`
    options can be specified; settings from later files override earlier ones.

    Example: `--conf base.conf --conf game.conf`


##### `--set <setting>=<value>`

:   Override a configuration setting. Multiple `--set` options can be
    specified. These take the highest priority over all config files.

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
    [working directory](starting.md#the-working-directory) if it exists.


##### `--working-dir <path>`

:   Set the [working directory](starting.md#the-working-directory) for DOSBox
    Staging; the emulator will act as if started from this directory. If a
    local `dosbox.conf` configuration exists in this folder, it will be loaded
    after the primary config (unless [`--nolocalconf`](#-nolocalconf) is
    specified).

    See the [Specifying paths on Windows](#specifying-paths-on-windows) if
    you're on Windows.


### Startup behaviour

##### `-c <command>`

:   Run the specified DOS command before handling [`PATH`](#path). Multiple
    `-c` options can be specified.


##### `--noautoexec`

:   Don't run DOS commands from any [autoexec](configuration.md#autoexec) sections.


##### `--exit`

:   Exit after running `-c` commands and the commands from the `[autoexec]`
    sections.


##### `--fullscreen`

:   Start in [`fullscreen`](../graphics/display-and-window.md#fullscreen) mode.


##### `--lang <lang_file>`

:   Start with the specified language file. Set to `auto` to detect the
    language from the host OS (this is the default).


##### `--machine <type>`

:   Emulate a specific machine type. The machine type affects both the
    emulated video and sound hardware. See
    [`machine`](../system/general.md#machine) for further details.


### Discovery

##### `--list-countries`

:   List all supported countries with their numeric codes, for use with the
    [`country`](../system/localisation.md#country) config setting.


##### `--list-layouts`

:   List all supported keyboard layouts with their codes, for use with the
    [`keyboard_layout`](../system/localisation.md#keyboard_layout) config setting.


##### `--list-code-pages`

:   List all bundled code pages (screen fonts).


##### `--list-shaders`

:   List all available shaders and their paths, for use with the
    [`shader`](../graphics/rendering.md#shader) config setting.


### Mapper

##### `--startmapper`

:   Launch the key mapper GUI directly (see [Key
    mapper](../input/keymapper.md#key-mapper)).


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
