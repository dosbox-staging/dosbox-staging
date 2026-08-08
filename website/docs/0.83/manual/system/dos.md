# DOS

The `[dos]` section (plus a few shell-related settings from `[dosbox]`)
controls the emulated DOS environment itself --- the reported DOS version,
shell behaviour, and file-locking.

<!-- TODO: memory management (XMS/EMS/UMB) and PCjr memory layout used to
     live in this file; they've moved to memory.md and machine-types.md
     respectively. Rewrite this intro so it doesn't promise content that's
     no longer here. -->

The reported DOS version defaults to 5.0, which covers the vast majority of
games from the entire DOS era. A handful of very late titles check for DOS
7.0, and some very early ones expect DOS 3.0, but these are edge cases you'll
likely never hit.

## Regional settings

For language, country, keyboard layout, and code page settings, see the
dedicated [Localisation](localisation.md) chapter.

## DOS version and compatibility

The [`ver`](#ver) setting controls which DOS version is reported to programs.
The default `5.0` is the most widely compatible. Some older games require
`3.3`, while a few late-era programs need `6.22` or `7.1`. Setting the DOS
version to 7.0 or above automatically enables environment variable expansion
in the DOS shell (see [`expand_shell_variable`](#expand_shell_variable)).

The [`setver_table_file`](#setver_table_file) setting provides persistent
storage for the `SETVER` command, which lets you report a different DOS version
to specific programs without changing the global version.

The [`file_locking`](#file_locking) setting emulates `SHARE.EXE` file locking.
The default `auto` only enables it when Windows 3.1 is running, as it's
required for some Windows 3.1 applications. In rare cases (e.g., Astral Blur
demo), it can cause crashes in DOS games --- set it to `off` if that happens.

## Shell settings

The DOS shell supports persistent
[command history](../using-dosbox-staging/shell.md#command-history) and
environment variable expansion. The
[`shell_history_file`](#shell_history_file) setting controls where the history
is stored; set it to empty to disable persistence. The
[`expand_shell_variable`](#expand_shell_variable) setting controls whether
variables like `%PATH%` are expanded in commands --- the default `auto` enables
this for DOS 7.0+ (matching real FreeDOS and MS-DOS 7 behaviour).

<!-- TODO: autoexec_section and automount are arguably "shell/startup"
     behaviour too — folded in below from general.md's "DOS & shell" section.
     Consider whether this section needs a better heading once everything's
     in one place. -->

The [`autoexec_section`](#autoexec_section) setting controls how multiple
config files' autoexec sections are combined. [`automount`](#automount)
controls whether `drives/[c]` folders are auto-mounted on startup.
[`startup_verbosity`](#startup_verbosity) controls how much is printed before
your program runs, and [`shell_config_shortcuts`](#shell_config_shortcuts)
enables shorthand commands like `sbtype sb16` instead of
`config -set sbtype sb16`.

[`allow_write_protected_files`](#allow_write_protected_files) and
[`mcb_fault_strategy`](#mcb_fault_strategy) control lower-level file and
memory-chain-block error handling — mostly relevant if a game is misbehaving
in unusual ways.

## Interrupt stacks

The [`stacks`](#stacks) setting replicates the `STACKS=count,size` directive
that MS-DOS 3.2 introduced in `CONFIG.SYS`. It maintains a small private pool
of stacks; whenever a hardware interrupt fires, DOSBox Staging switches to a
free slot before invoking the handler, then restores the original stack on the
way out.

The default `auto` enables the feature on AT-class
[`machine`](general.md#machine) types, using the same defaults MS-DOS used ---
9 stacks of 128 bytes each. It is disabled automatically on PC/XT-class
machines (e.g., `cga`, `pcjr`, `tandy`), since this was the default under DOS.
You will rarely need to change this; it exists for the edge case where a
specific program misbehaves with interrupt stacks enabled, or needs a
different stack count or size.

!!! note

    When a hardware interrupt fires --- a timer tick, a key press, and so on ---
    the CPU briefly pauses the running program and executes the interrupt
    handler, using the program's own stack for that purpose. Most programs leave
    enough stack space for this to work without issue. A small number of older
    programs allocate a very tight stack, however, and can crash or corrupt
    their own data if an interrupt handler (and any handlers it triggers in
    turn) together need more space than the program left available.


## Configuration settings

### Shell & version

You can set these parameters in the `[dos]` configuration section.

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


### Interrupt stacks

You can set these parameters in the `[dos]` configuration section.

##### stacks

:   Use DOS-style private stacks for hardware interrupts ('auto' by default).
    When a wrapped hardware interrupt fires, DOSBox Staging switches to one
    stack from a private pool before invoking the previous handler. Disabling
    this means each running program must have enough stack space for hardware
    interrupts (and any chained handlers) itself. Most programs work correctly
    without it; a few legacy programs depend on it to avoid corrupting their
    own stack.

    Note: the current implementation wraps the timer interrupt (INT 08h, IRQ0)
    and the keyboard interrupt (INT 09h, IRQ1). MS-DOS's STACKS feature wraps
    additional hardware-IRQ vectors; coverage may be expanded in future
    versions.

    - `auto` *default*{ .default } -- Enable on AT-class machine types with
      the MS-DOS defaults (9 stacks of 128 bytes each). Disable on PC/XT-class
      machine types (e.g., `cga`, `pcjr`, `tandy`).

    - `count,size` -- Allocate `count` private stacks of `size` bytes each,
      e.g. 'stacks = 9,128'. Equivalent to the DOS 'STACKS=count,size' setting;
      'count' must be 8-64, 'size' must be 32-512.

    - `0,0` -- Disable; use the interrupted program's stack.
