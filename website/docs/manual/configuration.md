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
to as **per-game configs**. If DOSBox finds a config named `dosbox.conf` in its
startup folder (working directory), it will load it _after_ the primary
configuration, thus potentially overriding global settings.

Local configs are typically used in setups where you create a subfolder for
each game in your "DOS games" folder. Broad settings applicable for each game
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


command-line/#-working-dir-path


## Portable setup

If a `dosbox-staging.conf` file is placed in the same directory as the DOSBox
Staging executable, DOSBox will use that directory as its configuration
directory instead of the platform-specific location. This is useful for
running DOSBox from a USB drive or keeping everything self-contained in a
single folder --- a setup commonly preferred by Windows users.


## Config layering

Multiple config files can be loaded, and later values override earlier ones:

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
you will find more such config examples there):

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

- **Full form**: `config -set SECTION SETTING = VALUE` (e.g.,
  `config -set render shader = sharp`)
- **Shortcut form**: `SETTING = VALUE` (e.g., `shader = sharp`)

The shortcut form works for most settings and is the quickest way to
experiment. If a value is invalid, the error is displayed in the DOS console
so you can see what went wrong.

To get help for any setting, use the `/?` shortcut:

```
sbtype /?
shader /?
```

This is equivalent to `config -h SETTINGNAME` but much quicker to type.

Some settings --- such as [`machine`](system/general.md#machine) --- require
a reboot to take effect. After changing such a setting, use `config -r` to
restart DOSBox. The setting's help text will tell you if a restart is needed.

!!! tip

    You can use `config -wc` to write the current settings to a new config
    file, which is handy after you've found the right values by experimenting
    at runtime.


## Configuration best practices

[Local configurations](#local-configuration) are great for customising
settings per game without using a fully-populated config for every single
game. That approach is very inflexible (e.g., if you want all your DOS games
to start in fullscreen mode, you'd need to set `fullscreen = off` in the
configuration of every single game if you use fully-populated configs).


