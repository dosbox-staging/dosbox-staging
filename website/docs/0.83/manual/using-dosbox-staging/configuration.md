# Configuration

DOSBox Staging uses plain-text configuration files to control every aspect of
the emulation. If you've ever edited an `.ini` file, the [syntax](#syntax)
will feel familiar.

The two main configuration file types in DOSBox Staging are the [primary
configuration](#primary-configuration) and [local
configuration](#local-configuration). They have the exact same format, and you
can configure the same things in both --- they only differ by their
purpose and their location on the filesystem.


## Primary configuration

The primary configuration file holds **global settings** for DOSBox Staging.
It is always loaded if it exists; if it doesn't, it will be created with the
default settings when you start the emulator.

The primary configuration file is stored in a platform-specific location:

<div class="compact" markdown>

| Platform | Path
| ---      | ---
| Windows  | `%LOCALAPPDATA%\DOSBox\dosbox-staging.conf`
| macOS    | `~/Library/Preferences/DOSBox/dosbox-staging.conf`
| Linux    | `~/.config/dosbox/dosbox-staging.conf`

</div>

On Linux, the primary configuration is also looked up in other common XDG path
locations.

Run DOSBox Staging with the [`--printconf`](command-line.md#-printconf)
argument from the command line to see the exact path on your system. You can
use [`--editconf`](command-line.md#-editconf) to open the primary
configuration in your default text editor.

## Local configuration

DOSBox Staging also supports **local configurations**, which are also referred
to as **per-game configurations**. If DOSBox finds a configuration file named
`dosbox.conf` in its startup folder (working directory), it will load it
_after_ the primary configuration, thus potentially overriding global
settings.

Local configs are typically used in setups where you create a subfolder for
each game in your "DOS games" folder. Broad settings controlling general
emulator behaviour and settings you want to apply for every game should live
in your primary config, then per-game overrides in the local configs. Because
local configs are effectively "overlaid" over the global configuration, we
call this [configuration layering](#configuration-layering).

This folder structure illustrates how a per-game setup would typically look:

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
guide](../../getting-started/passport-to-adventure.md#layered-configurations)
walks through creating several game configs in detail using layered
configurations.

If you start DOSBox Staging from a different folder, you can still set the
[working directory](command-line.md#-working-dir-path) via command line
arguments, then the local `dosbox.conf` will be loaded from that folder.


## Portable setup

If a `dosbox-staging.conf` file is placed in the same folder as the DOSBox
Staging executable; DOSBox will use that folder as its configuration folder
instead of the platform-specific location. This is useful for running DOSBox
from a USB drive or keeping everything self-contained in a single folder --- a
setup commonly preferred by Windows users.

!!! tip

    You can "convert" a non-portable installation into a portable one by
    moving the primary configuration file, `dosbox-staging.conf`, from its
    platform-specific location into the folder where the DOSBox Staging
    executable resides.

    Conversely, you can "convert" it back to a non-portable installation by
    deleting `dosbox-staging.conf` from the folder where the DOSBox Staging
    executable lives, then start up Staging --- a new default configuration will be
    created in the platform-specific location. Alternatively, move the primary
    configuration into the platform-specific location if you want to keep it.


## Configuration layering

In addition to the [primary](#primary-configuration) and [local
configuration](#local-configuration), you can specify additional configuration files
to be loaded with the [`--conf`](command-line.md#-conf-config_file) command
line argument. The possibilities don't end there; you can also set config
values directly via [`--set
<setting>=<value>`](command-line.md#-set-settingvalue) arguments.

The configurations are applied in this order (later values override earlier
ones):

1. The [primary configuration file](#primary-configuration) is loaded first (unless the
   [`--noprimaryconf`](command-line.md#-noprimaryconf) command line parameter
   is used).
2. The [local configuration](#local-configuration) (a `dosbox.conf` file in the
   working directory) is loaded next if it exists (unless
   [`--nolocalconf`](command-line.md#-nolocalconf) is used).
3. **Additional configuration files** specified via
   [`--conf`](command-line.md#-conf-config_file) are applied in the order
   given.
4. **Individual configuration overrides** via [`--set
   <setting>=<value>`](command-line.md#-set-settingvalue) are applied last and
   take highest priority.

This layering system lets you keep your general preferences in the primary
configuration and only override what's needed per game.

Refer to this section of the [Getting Started
guide](../../getting-started/passport-to-adventure.md#layered-configurations) for a
more detailed description.


## Syntax

Configuration files are divided into sections, each of which starts with a
`[section]` header. Settings use `key = value` syntax. Lines starting with `#`
are comments and are ignored.

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

!!! warning "End-of-line comments"

    You cannot use `#` for end-of-line comments, e.g., this will result in an
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
command per line, that are run sequentially when DOSBox Staging starts up.
This is similar to how the `AUTOEXEC.BAT` file works on a real DOS PC.

Here's an example configuration that launches a game executable `PRINCE`
from the C: drive, then exits DOSBox Staging after you quit the game (taken from the
[Getting started
guide](../../getting-started/enhancing-prince-of-persia.md#final-configuration);
you'll find more such configuration examples there):

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

    The `[autoexec]` section must be the last section in the configuration file.

See the [`autoexec_section`](../system/general.md#autoexec_section) setting for
how autoexec sections from multiple configuration files are handled.


## Generating a default config

If you want to start fresh, delete your primary configuration file (use
[`--printconf`](command-line.md#-printconf) to find the file) or run
[`--eraseconf`](command-line.md#-eraseconf). DOSBox Staging will create a new
primary configuration with default settings on the next launch.

See [Command-line usage](command-line.md) for all available launch options.

## Changing settings at runtime

Most configuration settings can be changed while DOSBox Staging is running,
directly from the DOS prompt. This is useful for experimenting with settings
without restarting --- you can try different shaders, adjust audio levels, or
tweak CPU speed on the fly and hear or see the difference immediately.

There are two ways to change a setting from the DOS prompt:

**Full form**: `CONFIG -set section setting = value`

For example:

``` { . .dos-prompt }
CONFIG -set render shader = sharp
```

**Shortcut form:** `setting = value`, or simply `setting value`

For example:

``` { . .dos-prompt }
shader = sharp
shader sharp
```

The shortcut form works for most settings and is the quickest way to
experiment. If a value is invalid, an error is displayed in the DOS console
and in the logs so you can see what went wrong.

Some settings --- such as [`machine`](../system/general.md#machine) ---
require a reboot to take effect. After changing such a setting, use `CONFIG
-r` to restart DOSBox Staging. The setting's help text will tell you if a
restart is needed.

To get help for any setting, use `CONFIG -h setting`, `CONFIG -h section
setting`, or the `setting /?` shortcut:

``` { . .dos-prompt }
CONFIG -h sblaster sbtype
CONFIG -h sbtype
sbtype /?
```

You can also list all settings in a section with `CONFIG -h section`:

``` { . .dos-prompt }
CONFIG -h gus
```

!!! tip

    Run `CONFIG -wc current.conf` to write the current configuration to a new
    config file called `current.conf` in the current working directory (you
    can pick any name), which is handy after you've found the right values by
    experimenting at runtime.


## Configuration best practices

- [Local configurations](#local-configuration) are great for customising
  your settings per game. This is especially true if you're interested in
  playing games from different [DOS eras](../introduction/dos-eras.md) that
  require very different hardware configurations.

- As DOSBox Staging comes with sensible defaults, you can keep your
  local configs quite minimal. There’s absolutely no need to
  specify every single setting in your local game-specific configs.
  Fully-populated configs are very cumbersome to manage if you have a large
  game library. The Getting Started guide contains
  several such local configuration examples ([Prince of
  Persia](../../getting-started/enhancing-prince-of-persia.md#final-configuration),
  [Passport to
  Adventure](../../getting-started/passport-to-adventure.md#final-configuration),
  [Beneath A Steel
  Sky](../../getting-started/beneath-a-steel-sky.md#final-configuration),
  [Star Wars: Dark
  Forces](../../getting-started/star-wars-dark-forces.md#final-configuration)).

- A good, low-maintenance approach is to limit the primary
  configuration to settings that affect the emulator's general behaviour ---
  things like [`fullscreen`](../graphics/display-and-window.md#fullscreen),
  [`pause_when_inactive`](../system/general.md#pause_when_inactive),
  [`language`](../system/localisation.md#language), and setting the [master
  volume](../sound/mixer.md#volume). Settings that configure the hardware a
  particular game needs can then go into the local configs. If you change the
  emulated hardware in the primary config, you risk breaking games that were
  configured for a certain hardware setup in their own setup utility.
