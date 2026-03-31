# Configuration

DOSBox Staging uses plain-text configuration files to control every aspect of
the emulation. If you've ever edited an `.ini` file, the format will feel
familiar.


## Config file locations

The **primary configuration file** is stored in a platform-specific
location:

<div class="compact" markdown>

| Platform | Path                                               |
| ---      | ---                                                |
| Windows  | `%LOCALAPPDATA%\DOSBox\dosbox-staging.conf`        |
| macOS    | `~/Library/Preferences/DOSBox/dosbox-staging.conf` |
| Linux    | `~/.config/dosbox/dosbox-staging.conf`             |

</div>

Run DOSBox Staging with the [`--printconf`](command-line.md#-printconf)
argument from the command line to see the exact path on your system. On macOS
and Linux, you can use [`--editconf`](command-line.md#-editconf) to open the
primary config in your default text editor.

A **local configuration file** named `dosbox.conf` can be placed in a game's
directory. DOSBox Staging automatically loads it when started from that
directory, making it easy to maintain per-game settings. The [Getting Started
guide](../getting-started/passport-to-adventure.md#layered-configurations)
walks through creating several per-game configs in detail.

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

