# Localisation

DOSBox Staging automatically detects your host operating system's language,
country, and keyboard layout, so in most cases everything works out of the box.
The auto-detection checks the list of preferred languages in your OS
preferences, matches keyboard layouts from your configured host layouts, and
selects an appropriate country code. If you need to override the defaults ---
perhaps you're running a German game on an English system, or you need a
specific date format --- the settings below let you fine-tune the regional
behaviour.


## Interface language

The [`language`](#language) setting controls the language of DOSBox Staging's
own interface messages (not the DOS programs themselves). It can be changed at
runtime via `config -set language=pl`. Available translations: German, English,
Spanish, French, Italian, Dutch, Polish, Brazilian Portuguese, and Russian.

DOSBox Staging uses the [gettext](https://www.gnu.org/software/gettext/)
`.po` translation file format, which makes contributing translations
straightforward with tools like [Poedit](https://poedit.net/).


## Country and date/time formatting

The [`country`](#country) setting controls DOS-level formatting conventions:
date and time format, decimal separators, currency symbols, and so on. The
default `auto` detects the appropriate country from your host OS settings.

The [`locale_period`](#locale_period) setting controls whether formatting
follows historic DOS conventions (how things looked on a real DOS PC of the
era), modern conventions (consistent with current-day practice), or your host
OS's native settings. The default `native` mode reuses your desktop's display
formats where the DOS locale system can represent them.


## Keyboard layout and code pages

The [`keyboard_layout`](#keyboard_layout) setting selects the DOS keyboard
layout, determining which characters are produced by which keys. The default
`auto` detects your layout from the host OS. A layout can include a code page
suffix --- for example, `uk 850` selects the British layout with a Western
European screen font.

Code pages control which character set is available on screen. DOSBox Staging
bundles the FreeDOS ISO, KOI, MAC, and WIN code page packages, providing
broad coverage of Latin, Cyrillic, and Greek scripts. After startup, use the
`KEYB` command to manage keyboard layouts and code pages at runtime (run
`HELP KEYB` for details).

To see what's available, start DOSBox Staging with the following command line
arguments:

- [`--list-countries`](../using-dosbox-staging/command-line.md#-list-countries)
  --- lists all supported countries with their numeric codes

- [`--list-layouts`](../using-dosbox-staging/command-line.md#-list-layouts)
  --- lists all supported keyboard layouts with their codes

- [`--list-code-pages`](../using-dosbox-staging/command-line.md#-list-code-pages)
  --- lists all bundled code pages (screen fonts)


## Configuration settings

The [`language`](#language) setting is in the `[dosbox]` section; the rest are
in `[dos]`.


### Interface language

##### language

:   Select the language of DOSBox Staging's interface messages.

    Possible values:

    - `auto` *default*{ .default } -- Detect the language from the host OS
      preferred languages list.
    - `<value>` -- Load a translation from the given file.

    !!! note

        The following language files are available: `de`, `en`, `es`, `fr`,
        `it`, `nl`, `pl`, `pt_BR`, and `ru`. English is built-in; the rest
        is stored in the bundled `resources/translations` folder.


### Regional settings

##### country

:   Set DOS country code (`auto` by default). This affects country-specific
    information such as date, time, and decimal formats. If set to `auto`, it
    selects the country code reflecting the host OS settings.

    !!! note

        The list of country codes can be displayed using the
        [`--list-countries`](../using-dosbox-staging/command-line.md#-list-countries)
        command-line argument.


##### keyboard_layout

:   Keyboard layout code (`auto` by default). Set to `auto` to guess the
    values from the host OS settings. The layout can be followed by the code
    page number, e.g., `uk 850` selects a Western European screen font. After
    startup, use the `KEYB` command to manage keyboard layouts and code pages
    (run `HELP KEYB` for details).

    !!! note

        The list of supported keyboard layout codes can be displayed using
        the
        [`--list-layouts`](../using-dosbox-staging/command-line.md#-list-layouts)
        command-line argument (e.g., `uk` is the British English layout).


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
