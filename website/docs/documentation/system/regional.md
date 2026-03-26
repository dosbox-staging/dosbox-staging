# Regional settings

DOSBox Staging automatically detects your host operating system's language
and keyboard layout, so in most cases it just does the right thing out of
the box. If you need to override the defaults — perhaps you're running a
German game on an English system, or you need a specific date format — a
few settings let you fine-tune the regional behaviour.

The `country` setting controls DOS-level formatting conventions: date and
time format, decimal separators, currency symbols, and so on. The
`keyboard_layout` setting selects the DOS keyboard layout, determining
which characters are produced by which keys.

To see what's available on your system:

- [`--list-countries`](command-line.md#-list-countries) — lists all supported countries with their numeric codes
- [`--list-layouts`](command-line.md#-list-layouts) — lists all supported keyboard layouts with their codes
- [`--list-code-pages`](command-line.md#-list-code-pages) — lists all bundled code pages (screen fonts)

These regional settings are configured in the `[dos]` section:

- [country](system/dos.md#country) — set the DOS country code for
  date/time/number formatting
- [keyboard_layout](system/dos.md#keyboard_layout) — set the DOS keyboard
  layout
- [locale_period](system/dos.md#locale_period) — use a period as the
  decimal separator regardless of country setting

The `language` setting in the `[dosbox]` section controls the language of
DOSBox Staging's own user interface messages — see
[language](system/general.md#language) for details.
