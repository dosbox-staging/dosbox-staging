# DOS

DOSBox Staging doesn't just run old programs --- it emulates the MS-DOS
operating environment underneath them. Games talk to this emulated DOS
directly: they check its version number and rely on quirks of how its shell
behaves. When an old game misbehaves, one possible reason is that the emulated
DOS doesn't match what the game expects. These settings let you close that
gap.


## DOS version and compatibility

The [`ver`](#ver) setting controls which DOS version is reported to programs.
The default `5.0` is the most widely compatible. Some older games require
`3.3`, while a few late-era programs need `6.22` or `7.1`. If you need to
report a different version to just one program without changing the global
setting, use [`setver_table_file`](#setver_table_file) with `SETVER.EXE`.

Setting the DOS version to 7.0 or above also turns on environment variable
expansion in the shell by default (see
[`expand_shell_variable`](#expand_shell_variable)).

The [`file_locking`](#file_locking) setting emulates `SHARE.EXE` file
locking, which some Windows 3.1 applications need to work properly. It's on
automatically when Windows 3.1 is running. If a DOS game crashes with file
locking on, try setting it to `off`.


## Shell behaviour

The shell supports persistent
[command history](../using-dosbox-staging/shell.md#command-history), stored
via [`shell_history_file`](#shell_history_file); set that to empty to turn
history off. [`expand_shell_variable`](#expand_shell_variable) controls
whether things like `%PATH%` get expanded in commands, and
[`shell_config_shortcuts`](#shell_config_shortcuts) lets you type `sbtype
sb16` instead of `config -set sbtype sb16`.


## Automount

[`automount`](#automount_1) turns automatic drive mounting on or off. For how
automounting actually works --- the `drives/` folder structure, mount-config
files, and so on --- see
[Automounting](../using-dosbox-staging/storage.md#automounting).


## Regional settings

For language, country, keyboard layout, and code page settings, see the
dedicated [Localisation](localisation.md) chapter.


## Configuration settings

### Shell & version

You can set the DOS parameters in the `[dos]` configuration section.

##### ver

:   Set DOS version in `MAJOR.MINOR` format (`5.0` by default). A single
    number is treated as the major version. Common settings are `3.3`, `5.0`,
    `6.22`, and `7.1`.

##### expand_shell_variable

:   Enable expanding environment variables such as `%PATH%` in the DOS
    command shell. FreeDOS and MS-DOS 7.0+ `COMMAND.COM` support this
    behaviour.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Enabled if DOS version is 7.0 or
      above.
    - `on` -- Enable expansion of environment variables.
    - `off` -- Disable expansion of environment variables.

    </div>


##### file_locking

:   Enable file locking via emulating `SHARE.EXE`. This is required for some
    Windows 3.1 applications to work properly. It generally does not cause
    problems for DOS games except in rare cases (e.g., Astral Blur demo). If
    you experience crashes related to file permissions, you can try disabling
    this.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Enable file locking only when Windows
      3.1 is running.
    - `on` -- Always enable file locking.
    - `off` -- Always disable file locking.

    </div>


##### setver_table_file

:   File containing the list of applications and assigned DOS versions, in a
    tab-separated format, used by `SETVER.EXE` as a persistent storage
    (empty by default).


##### shell_history_file

:   File containing persistent command line history (`shell_history.txt` by
    default). Setting it to empty disables persistent shell history.


### Startup & shell shortcuts

You can set these parameters in the `[dosbox]` configuration section.

##### autoexec_section

:   How autoexec sections are handled from multiple config files.

    Possible values:

    <div class="compact" markdown>

    - `join` *default*{ .default } -- Combine them into one big section.
    - `overwrite` -- Use the last one encountered, like other config
      settings.

    </div>


##### automount

:   Mount `drives/[c]` folders as drives on startup, where `[c]` is a
    lower-case drive letter from `a` to `y`. The `drives` folder can be
    provided relative to the current folder or via built-in resources. Mount
    settings can be optionally provided using a `[c].conf` file alongside the
    drive's folder.

    Possible values: `on` *default*{ .default }, `off`


##### startup_verbosity

:   Controls verbosity prior to displaying the program.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- `low` if exec or dir is passed,
      otherwise `high`.
    - `high` -- Show welcome banner and early stdout.
    - `low` -- Show early stdout only.
    - `quiet` -- Don't show welcome banner or early stdout.

    </div>


##### shell_config_shortcuts

:   Allow shortcuts for simpler configuration management. E.g., instead of
    `config -set sbtype sb16`, it is enough to execute `sbtype sb16`, and
    instead of `config -get sbtype`, you can just execute the `sbtype`
    command.

    Possible values: `on` *default*{ .default }, `off`


##### allow_write_protected_files

:   Many games open all their files with writable permissions; even files that
    they never modify. This setting lets you write-protect those files while
    still allowing the game to read them. A second use-case: if you're using a
    copy-on-write or network-based filesystem, this setting avoids triggering
    write operations for these write-protected files.

    Possible values: `on` *default*{ .default }, `off`


##### mcb_fault_strategy

:   How software-corrupted memory chain blocks should be handled.

    Possible values:

    <div class="compact" markdown>

    - `repair` *default*{ .default } -- Repair (and report) faults using
      adjacent blocks.
    - `report` -- Report faults but otherwise proceed as-is.
    - `allow` -- Allow faults to go unreported (hardware behaviour).
    - `deny` -- Quit (and report) when faults are detected.

    </div>
