# DOS

The `[dos]` section controls the emulated DOS environment itself — version
number, memory management, and regional settings.

The reported DOS version defaults to 5.0, which covers the vast majority of
games from the entire DOS era. A handful of very late titles check for DOS
7.0, and some very early ones expect DOS 3.0, but these are edge cases you'll
likely never hit.

XMS, EMS, and UMB are different memory management schemes from the DOS era. If
those acronyms mean nothing to you, don't worry --- they're all enabled by
default and you can safely ignore them. On the rare occasion a game misbehaves
(usually an old title that chokes on EMS), disabling the offending memory type
is a quick fix.

## Regional settings

DOSBox Staging automatically detects your host operating system's language
and keyboard layout, so in most cases it just does the right thing out of
the box. If you need to override the defaults — perhaps you're running a
German game on an English system, or you need a specific date format — a
few settings let you fine-tune the regional behaviour.

The [country](#country) setting controls DOS-level formatting conventions:
date and time format, decimal separators, currency symbols, and so on. The
[keyboard_layout](#keyboard_layout) setting selects the DOS keyboard layout,
determining which characters are produced by which keys.
[locale_period](dos.md#locale_period) lets you specify the decimal separator
regardless of the country setting.

To see what's available on your system, start DOSBox Staging with the
following command line arguments:

- [`--list-countries`](../command-line.md#-list-countries) --- lists all supported countries with their numeric codes
- [`--list-layouts`](../command-line.md#-list-layouts) --- lists all supported keyboard layouts with their codes
- [`--list-code-pages`](../command-line.md#-list-code-pages) --- lists all bundled code pages (screen fonts)

!!! note 

    Use the [language](../general/#language) setting in the `[dosbox]` section
    to set the language of DOSBox Staging's own interface.

## Configuration settings

You can set the DOS parameters in the `[dos]` configuration section.


### Regional settings

##### country

:   Set DOS country code (`auto` by default). This affects country-specific
    information such as date, time, and decimal formats. If set to `auto`, it
    selects the country code reflecting the host OS settings.

    !!! note

        The list of country codes can be displayed using the
        [`--list-countries`](../command-line.md#-list-countries) command-line
        argument.


##### keyboard_layout

:   Keyboard layout code (`auto` by default). Set to `auto` to guess the
    values from the host OS settings. The layout can be followed by the code
    page number, e.g., `uk 850` selects a Western European screen font. After
    startup, use the `KEYB` command to manage keyboard layouts and code pages
    (run `HELP KEYB` for details).

    !!! note

        The list of supported keyboard layout codes can be displayed using
        the [`--list-layouts`](../command-line.md#-list-layouts) command-line
        argument (e.g., `uk` is the British English layout).


##### locale_period

:   Set locale epoch.

    Possible values:

    <div class="compact" markdown>

    - `native` *default*{ .default } -- Re-use current host OS settings,
      regardless of the country set; use `modern` data to fill in the gaps
      when the DOS locale system is too limited to follow the desktop
      settings.
    - `historic` -- If data is available for the given country, mimic old DOS
      behaviour when displaying time, dates, or numbers.
    - `modern` -- Follow current day practices for user experience more
      consistent with typical host systems.

    </div>


### Memory

##### ems

:   Enable EMS support. Enabled provides the best compatibility but certain
    applications may run better with other choices, or require EMS support to
    be disabled to work at all.

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


##### umb

:   Enable UMB memory support.

    Possible values: `on` *default*{ .default }, `off`


##### xms

:   Enable XMS memory support.

    Possible values: `on` *default*{ .default }, `off`


### Shell & version

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


##### ver

:   Set DOS version in `MAJOR.MINOR` format (`5.0` by default). A single
    number is treated as the major version. Common settings are `3.3`, `5.0`,
    `6.22`, and `7.1`.
