# Configuration

DOSBox Staging uses plain-text configuration files to control every aspect of
the emulation. If you've ever edited an `.ini` file, the [syntax](#syntax)
will feel familiar.

The two main configuration types in DOSBox Staging are the [primary
configuration](#primary-configuration) and [local
configuration](#local-configuration). They have the exact same format and you
can configure the same things in both --- they only different by their
purpose and their location on the filesystem.


## Primary configuration

The primary configuration holds **global settings** for DOSBox Staging. It is
always loaded if it exists; if it doesn't, it will be created when you start
the emulator with the default settings.

The primary config file is stored in a platform-specific location:

<div class="compact" markdown>

| Platform | Path                                               |
| ---      | ---                                                |
| Windows  | `%LOCALAPPDATA%\DOSBox\dosbox-staging.conf`        |
| macOS    | `~/Library/Preferences/DOSBox/dosbox-staging.conf` |
| Linux    | `~/.config/dosbox/dosbox-staging.conf`             |

</div>

On Linux, the primary config is looked up in other common XDG path locations as
well.

Run DOSBox Staging with the [`--printconf`](command-line.md#-printconf)
argument from the command line to see the exact path on your system. You can
use [`--editconf`](command-line.md#-editconf) to open the primary config in
your default text editor.

## Local configuration

DOSBox Staging also supports **local configurations**, which are also referred
to as **per-game configuration**. If DOSBox finds a config named
`dosbox.conf` in its startup folder (working directory), it will load it
_after_ the primary configuration, thus potentially overriding global
settings.

Local configs are typically used in setups where you create a subfolder for
each game in your "DOS games" folder. Broad settings controlling general
emulator behaviour and settings applicable for applicable for each game
should live in your primary config, then per-game overrides in the local
configs. 

This folder structure illustrates how a per-game setup would typically look
like:

```
DOS Games
├── Azrael's Tear
│   └── dosbox.conf
│       ...
├── Dungeon Master
│   └── dosbox.conf
│       ...
├── Ultima Underworld
│   └── dosbox.conf
│       ...
...
```

The [Getting Started
guide](../getting-started/passport-to-adventure.md#layered-configurations)
walks through creating several per-game configs in detail.

If you start DOSBox Staging from a different folder, you can still set the
[working directory](../command-line/#-working-dir-path) via command line
arguments, then the local `dosbox.conf` will be loaded from that directory.


## Portable setup

If a `dosbox-staging.conf` file is placed in the same directory as the DOSBox
Staging executable, DOSBox will use that directory as its configuration
directory instead of the platform-specific location. This is useful for
running DOSBox from a USB drive or keeping everything self-contained in a
single folder --- a setup commonly preferred by Windows users.

!!! tip

    You can "convert" a non-portable installation intto a portable one by moving
    the primary config from its platform-specific location into the directory
    where the DOSBox Staging executable resides.


## Config layering

In addition to the [primary](#primary-configuration) and [local
configuration](#local-configuration), you can specify additional config files
to be loaded with the [`--conf`](command-line.md#-conf-config_file) command
line argument. The possibilities don't end there; you can also set config
values directly via [`--set
<setting>=<value>`](command-line.md#-set-settingvalue) arguments.

The various configuration mechanism are applied in this order (later values
override earlier ones):

1. The primary config file is loaded first (unless the
   [`--noprimaryconf`](command-line.md#-noprimaryconf) command line parameter
   is used).
2. A local `dosbox.conf` in the working directory is loaded next (unless
   [`--nolocalconf`](command-line.md#-nolocalconf) is used).
3. Additional config files specified via
   [`--conf`](command-line.md#-conf-config_file) are applied in the order
   given.
4. Individual [`--set <setting>=<value>`](command-line.md#-set-settingvalue)
   overrides are applied last and take highest priority.

This layering system lets you keep your general preferences in the primary
config and only override what's needed per game.

Refer to this section of the [Getting Started
guide](../getting-started/passport-to-adventure.md#layered-configurations) for a
more detailed description.


## Syntax

Config files are divided into sections, each starting with a `[section]`
header. Settings use `key = value` syntax. Lines starting with `#` are
comments and are ignored.

```ini
[render]
# Use the 'sharp' shader instead of the default CRT emulation
shader = sharp

[cpu]
# Set 486DX2/66 speed
cpu_cycles = 25000

[autoexec]
c:
mixer opl 50
prince
```

The `[autoexec]` section is special; see [Autoexec](#autoexec) for details.

!!! warning "End of line comments"

    You cannot use `#` for end of line comments, e.g., this will result in an
    error:

    ```ini
    [cpu]
    cpu_cycles = 25000  # Set 486DX2/66 speed
    ```

## Autoexec

The `[autoexec]` section is the last section in the [configuration
file](configuration.md). Each line in this section is executed at startup as a
DOS command.

Unlike other configuration sections, the `[autoexec]` section does not contain
individual settings. Instead, it's a freeform block of DOS commands, one
command per line, that are run sequentially when DOSBox starts up. This is
similar to how the `AUTOEXEC.BAT` file works on a real DOS PC.

Here's an example configuration that launches a game executable `PRINCE`
from the C: drive, then exits DOSBox Staging after you quit the game (taken from the
[Getting started
guide](../getting-started/enhancing-prince-of-persia.md#final-configuration);
you'll find more such config examples there):

```ini
[sdl]
fullscreen = on
pause_when_inactive = yes

[mixer]
reverb = large
chorus = normal

[autoexec]
c:
prince
exit
```

!!! important

    The `[autoexec]` section must be the last section in the config file.

See the [autoexec_section](system/general.md#autoexec_section) setting for
how autoexec sections from multiple config files are handled.


## Generating a default config

If you want to start fresh, delete your primary config file (use
[`--printconf`](command-line.md#-printconf) to find the file) or run
[`--eraseconf`](command-line.md#-eraseconf). DOSBox Staging will create a new
primary config with default settings on next launch.

See [Command-line usage](command-line.md) for all available launch options.

## Changing settings at runtime

Most configuration settings can be changed while DOSBox is running, directly
from the DOS prompt. This is useful for experimenting with settings without
restarting --- you can try different shaders, adjust audio levels, or tweak
CPU speed on the fly and hear or see the difference immediately.

There are two ways to change a setting:

- **Full form**: `CONFIG -set section setting = value`(e.g.,
  `CONFIG -set render shader = sharp`)

- **Shortcut form**: `setting = value`, or simply `setting value` (e.g.,
  `shader = sharp`, or just `shader sharp`)

The shortcut form works for most settings and is the quickest way to
experiment. If a value is invalid, the error is displayed in the DOS console
so you can see what went wrong.

To get help for any setting, use the `/?` shortcut:

```
sbtype /?
shader /?
```

This is equivalent to `CONFIG -h setting` but much quicker to type.

Some settings --- such as [`machine`](system/general.md#machine) --- require
a reboot to take effect. After changing such a setting, use `CONFIG -r` to
restart DOSBox. The setting's help text will tell you if a restart is needed.

!!! tip

    You can use `CONFIG -wc` to write the current settings to a new config
    file, which is handy after you've found the right values by experimenting
    at runtime.


## Configuration best practices

- [Local configurations](#local-configuration) are great for customising
  your settings per game. This is especially true if you're interested in
  playing games from different [DOS eras](../dos-eras/) that require very different
  hardware configurations.

- As DOSBox Staging comes with sensible defaults, you can keep your
  local configs quite minimal. There’s absolutely no need to
  specify every single setting in your local game-specific configs.
  Fully-populated configs are very cumbersome to manage if you have a large
  game library. The Getting Started guide contains
  several such local config examples ([Prince of Persia](../../getting-started/enhancing-prince-of-persia/#final-configuration),
  [Passport to Adventure](../../getting-started/passport-to-adventure/#final-configuration),
  [Beneath A Steel Sky](../../getting-started/beneath-a-steel-sky/#final-configuration),
  [Star Wars: Dark Forces](../../getting-started/star-wars-dark-forces/#final-configuration)).

- A good, easy-to-manage approach is to only change settings in the primary
  config that affect the general workings of the emulator (e.g.,
  [fullscreen](../graphics/display-and-window/#fullscreen),
  [pause_when_inactive](../graphics/display-and-window/#pause_when_inactive),
  [language](../system/general/#language), setting the [master
  volume](../sound/mixer/#volume), etc.) Settings that set up specific
  hardware required by a game can then go into the local configs. If you
  reconfigure hardware in the primary config, there's always a risk that
  games configured for a certain hardware in their setup utility will stop
  working.
