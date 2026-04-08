# Configuration

DOSBox Staging uses plain-text configuration files to control every aspect
of the emulation. If you've ever edited an `.ini` file, the format will
feel familiar.


## Config file locations

The **primary configuration file** is stored in a platform-specific
location:

| Platform | Path |
|---|---|
| Windows | `%LOCALAPPDATA%\DOSBox\dosbox-staging.conf` |
| macOS | `~/Library/Preferences/DOSBox/dosbox-staging.conf` |
| Linux | `~/.config/dosbox/dosbox-staging.conf` |

Run DOSBox Staging with the [`--printconf`](command-line.md#-printconf) argument from the command line to see the exact path on your
system. On macOS and Linux, you can use
[`--editconf`](command-line.md#-editconf) to open it directly in a text
editor.

A **local configuration file** named `dosbox.conf` can be placed in a
game's directory. DOSBox Staging automatically loads it when started from
that directory, making it easy to maintain per-game settings.

### Portable setup

TODO


## Config layering

Multiple config files can be loaded, and later values override earlier ones:

1. The primary config file is loaded first (unless the
   [`--noprimaryconf`](command-line.md#-noprimaryconf) command line parameter
   is used).
2. A local `dosbox.conf` in the working directory is loaded next (unless
   [`--nolocalconf`](command-line.md#-nolocalconf) is used).
3. Additional config files specified via [`--conf`](command-line.md#-conf) are
   applied in the order given.
4. Individual [`--set <setting>=<value>`](command-line.md#-set-settingvalue)
   overrides are applied last and take highest priority.

This layering system lets you keep your general preferences in the primary
config and only override what's needed per game.


## Syntax

Config files are divided into sections, each starting with a `[section]`
header. Settings use `key = value` syntax. Lines starting with `#` are
comments.

```ini
[render]
# Use a sharp pixel shader instead of the default CRT emulation
shader = sharp

[cpu]
# Set 486 DX2/66 speed
cpu_cycles = 25000

[autoexec]
c:
mixer opl 50
prince
```

The `[autoexec]` section is special — it contains DOS commands that run at
startup, similar to `AUTOEXEC.BAT`. It must always be the last section in
the file. See [Autoexec](autoexec.md) for details.

!!! warning "End of line comments"

    You cannot use `#` for end of line comments, e.g., this will result in an
    error:

    ```ini
    [cpu]
    cpu_cycles = 25000  # Set 486 DX2/66 speed
    ```



## Generating a default config

If you want to start fresh, delete your primary config file (or run
[`--eraseconf`](command-line.md#-eraseconf) and DOSBox Staging will create a new one with default
settings on next launch. You can also use
[`--printconf`](command-line.md#-printconf) to find the file and edit it manually.

See [Command-line usage](command-line.md) for all available launch options.
