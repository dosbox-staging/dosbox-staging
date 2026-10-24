# Logging

DOSBox Staging records what it is doing (and what went wrong) in its logs.
Each log line has a timestamp, an importance level (info, warning, or error),
and a message. Many warnings and errors include a hint on how to fix the
problem, such as listing the valid values for a setting.

Check the logs first when something isn't working, such as a typo in a
configuration setting or a game that won't start.

## Log files

By default, DOSBox Staging writes a log file on every run to the `logs` folder
in your [configuration
directory](../using-dosbox-staging/configuration.md#primary-configuration)
(you can change the log directory with [log_dir](../system/overview.md#log_dir)).

Log file names are in the `dosbox-YYYYMMDD-HHMMSS.log` format, using the date
and time the file was created (e.g., `dosbox-20261018-223940.log`). If you
sort the folder by name in ascending order, the last file in the list is the
log from your most recent run.

The ten most recent log files are kept, including the current one. When you
have ten log files in the folder and start DOSBox Staging, the oldest one is
deleted.

Use [log_mode](../system/overview.md#log_mode) to choose where log messages
go: the console, a file, both (this is the default), or you can also turn
logging off.

!!! important "Reporting issues"

    Always attach the log file when you report an issue, and make sure
    it's the one from the run where the problem happened. Without the logs, we
    have very little to go on when diagnosing problems.


## Console

When console logging is enabled, the log messages are shown live in a console
window. Warnings are yellow and errors are red. Where the console appears
depends on your platform:

- **Windows:** Opens by default. Use the
  [`--noconsole`](command-line.md/#-noconsole) command-line argument to hide it.

- **macOS:** Hidden by default, see the note below.

- **Linux:** Start DOSBox Staging from a terminal and the messages will appear
  there.

!!! tip

    Keep the console visible while you set up a game. Config mistakes show up
    there as soon as DOSBox Staging starts.

!!! note "macOS notes"

    To make the console window appear, copy the **Start DOSBox Staging
    (logging)** icon from the DOSBox Staging `.dmg` into your game folder
    (note the word "logging" in the name of the icon).

    When you use this icon for the first time, you'll need to right-click or
    ++ctrl++-click it, select the topmost **Open** menu item, and then press
    the **Open** button in the appearing dialog. After the first start, you
    can simply double-click on it.


## Configuration settings

Logging settings are in the `[dosbox]` section:

<div class="compact" markdown>

- [`log_mode`](../system/overview.md#log_mode)
- [`log_dir`](../system/overview.md#log_dir)

</div>

