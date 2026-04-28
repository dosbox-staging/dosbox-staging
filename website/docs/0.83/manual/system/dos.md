# DOS

The `[dos]` section controls the emulated DOS environment itself --- version
number, memory management, and regional settings.

The reported DOS version defaults to 5.0, which covers the vast majority of
games from the entire DOS era. A handful of very late titles check for DOS
7.0, and some very early ones expect DOS 3.0, but these are edge cases you'll
likely never hit.

### Memory management

The first IBM PCs only supported up to 640 KB of memory --- the so-called
"conventional memory" that DOS programs can access directly. As PCs gained more
RAM, a patchwork of standards emerged to make the extra memory available:

- **XMS** (Extended Memory) --- the main pool of memory above 1 MB. This is
  what the [`memsize`](general.md#memsize) setting controls (16 MB by default).
  Nearly all games from the late DOS era use XMS.

- **EMS** (Expanded Memory) --- an older scheme that maps 64 KB pages of
  memory into the conventional memory area. Some early to mid-1990s games
  require EMS; a few older titles may actually malfunction if it's enabled.

- **UMB** (Upper Memory Blocks) --- small pockets of memory between 640 KB
  and 1 MB, used to free up conventional memory by loading drivers and TSRs
  into them.

All three are enabled by default and you can safely ignore them. On the rare
occasion a game misbehaves (usually an old title that chokes on EMS),
disabling the offending memory type is a quick fix.

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

## PCjr memory

The [`pcjr_memory_config`](#pcjr_memory_config) setting controls memory layout
on the emulated PCjr. The default `expanded` provides 640 KB and is compatible
with most games. A few very old PCjr titles ([Jumpman](https://www.mobygames.com/game/80/jumpman/), [Troll](https://www.mobygames.com/game/14214/troll/)) require the
`standard` 128 KB layout.

## Interrupt stacks

The [`stacks`](#stacks) setting replicates the `STACKS=count,size` directive
that MS-DOS 3.2 introduced in `CONFIG.SYS`. It maintains a small private pool
of stacks; whenever a hardware interrupt fires, DOSBox Staging switches to a
free slot before invoking the handler, then restores the original stack on the
way out.

The default `auto` enables the feature on AT-class [`machine`](general.md#machine) types, using the
same defaults MS-DOS used --- 9 stacks of 128 bytes each. It is disabled
automatically on PC/XT-class machines (e.g., `cga`, `pcjr`, `tandy`), since this
was the default under DOS. You will rarely need to change this; it exists for
the edge case where a specific program misbehaves with interrupt stacks
enabled, or needs a different stack count or size.

!!! note

    When a hardware interrupt fires --- a timer tick, a key press, and so on ---
    the CPU briefly pauses the running program and executes the interrupt
    handler, using the program's own stack for that purpose. Most programs leave
    enough stack space for this to work without issue. A small number of older
    programs allocate a very tight stack, however, and can crash or corrupt
    their own data if an interrupt handler (and any handlers it triggers in
    turn) together need more space than the program left available.

## Configuration settings

You can set the DOS parameters in the `[dos]` configuration section.


### Memory

##### ems

:   Enable EMS support. Enabled provides the best compatibility but certain
    applications may run better with other choices, or require EMS support to
    be disabled to work at all.

    Possible values: `on` *default*{ .default }, `off`


##### xms

:   Enable XMS memory support.

    Possible values: `on` *default*{ .default }, `off`


##### umb

:   Enable UMB memory support.

    Possible values: `on` *default*{ .default }, `off`


##### pcjr_memory_config

:   Set PCjr memory layout.

    Possible values:

    <div class="compact" markdown>

    - `expanded` *default*{ .default } -- 640 KB total memory with
      applications residing above 128 KB. Compatible with most games.
    - `standard` -- 128 KB total memory with applications residing below
      96 KB. Required for some older games (e.g., Jumpman, Troll).

    </div>


### Shell & version

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


### Interrupt stacks

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

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Enable on AT-class machine types with
      the MS-DOS defaults (9 stacks of 128 bytes each). Disable on PC/XT-class
      machine types (e.g., `cga`, `pcjr`, `tandy`).
    - `count,size` -- Allocate `count` private stacks of `size` bytes each,
      e.g. 'stacks = 9,128'. Equivalent to the DOS 'STACKS=count,size' setting;
      'count' must be 8-64, 'size' must be 32-512.
    - `0,0` -- Disable; use the interrupted program's stack.

    </div>
