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

Run `--printconf` to see the exact path on your system, or `--editconf` to
open it directly in a text editor.

A **local configuration file** named `dosbox.conf` can be placed in a
game's directory. DOSBox Staging automatically loads it when started from
that directory, making it easy to maintain per-game settings.


## Config layering

Multiple config files can be loaded, and later values override earlier ones:

1. The primary config file loads first (unless `--noprimaryconf` is used).
2. A local `dosbox.conf` in the working directory loads next (unless
   `--nolocalconf` is used).
3. Any files specified via `--conf` on the command line load in the order
   given.
4. Individual `--set` overrides are applied last and take highest priority.

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
cpu_cycles = 25000
```

The `[autoexec]` section is special — it contains DOS commands that run at
startup, similar to `AUTOEXEC.BAT`. It must always be the last section in
the file. See [Autoexec](autoexec.md) for details.


## Generating a default config

If you want to start fresh, delete your primary config file (or run
`--eraseconf`) and DOSBox Staging will create a new one with default
settings on next launch. You can also use
`--printconf` to find the file and edit it manually.

See [Command-line usage](command-line.md) for all available launch options.
