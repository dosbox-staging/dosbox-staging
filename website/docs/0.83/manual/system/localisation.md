# Localisation

DOSBox Staging does not try to detect your host operating system's language,
country, or keyboard layout. It starts the way a typical PC did in the early
1990s: **US English**, the **US keyboard layout**, and **code page 437** ---
the character set of the original IBM PC and the default in MS-DOS.

DOS had no automatic regional detection. If you wanted another country's
keyboard layout, character sets, and locale formats, you had to set them up
manually. If you didn't, your machine was a US English machine by default.
Most games and programs were written and tested on such machines, and might
therefore break under any other locale settings. For example, many draw their
menus and borders with [box-drawing
characters](https://en.wikipedia.org/wiki/Box-drawing_characters) from code
page 437, and others expect US date and number formats. Under a different
setup, they can show garbled text or misbehave in less obvious ways.

Date, time, and number formatting follows the same idea --- the default
historic [locale period](#locale_period) displays them the way a DOS PC in the
1980s and 90s would have. However, you can change that to mimic more modern
locale practices.

If you're playing a game in another language or want a different locale
format, the localisation settings discussed in this section let you change the
regional behaviour. Concretely, "localisation" here covers three independent
things, each with its own setting --- you can change any one without touching
the others:

| Setting | Controls | Config key |
|---|---|---|
| [Interface language](#interface-language) | The language of DOSBox Staging's own menus and messages | [`language`](#language) |
| [Country and locale period](#country-and-datetime-formatting) | DOS-level date, time, and number formatting | [`country`](#country), [`locale_period`](#locale_period) |
| [Keyboard layout and code pages](#keyboard-layout-and-code-pages) | Which characters your keys produce, and which characters the screen can display | [`keyboard_layout`](#keyboard_layout) |

## Interface language

The [`language`](#language) setting controls the language of DOSBox Staging's
own interface messages, including the [DOS
commands](../using-dosbox-staging/commands.md) built into the emulator. It can
be changed at runtime, e.g., by running `language pl`. The setting has no
effect on actual DOS programs you install yourself and the small number of
[bundled third party DOS
programs](../using-dosbox-staging/commands.md#bundled-third-party-programs),
so it doesn't matter for DOS compatibility.

The currently bundled translations are German, English, Spanish, French,
Italian, Dutch, Polish, Brazilian Portuguese, and Russian.

!!! warning

    DOSBox Staging's interface language translations are a community-based
    effort. Some translations may be incomplete or behind the current English
    version, so translated interface messages may occasionally be missing or
    out of date.

!!! note

    DOSBox Staging uses the [gettext](https://www.gnu.org/software/gettext/)
    `.po` translation file format, which makes contributing translations
    straightforward with tools like [Poedit](https://poedit.net/).


## Country and locale period

The [`country`](#country) setting controls DOS-level formatting conventions:
date and time format, decimal separators, currency symbols, and so on.

The [`locale_period`](#locale_period) setting controls whether formatting
follows **historic** DOS conventions (how things looked on a real DOS PC of the
era), or **modern** conventions (consistent with current-day practices).

## Keyboard layout and code pages

The [`keyboard_layout`](#keyboard_layout) setting selects the **DOS keyboard
layout**, determining which characters are produced by which keys on your
keyboard. This is very similar to the keyboard layout setting of your host
operating system --- for example, if you have a German physical keyboard,
you'd usually want to select the German keyboard layout in your OS
preferences, otherwise some keys would produce other symbols on the screen
than what their keycaps indicate.

Optionally, a layout can include a numeric **code page** suffix to override
the default code page automatically chosen by Staging --- for example, `uk
850` selects the British layout with a Western European code page (also called
**screen font**). On a real MS-DOS machine, you configure the keyboard layout
and the code page separately with different commands. DOSBox Staging
simplifies this; most users should just set something like `keyboard_layout
de` which will set both together. Still, they're still two different things
underneath, and understanding the difference is the key to the rest of this
section.

### Keyboard layout vs code page

**Keyboard layout** is about your *keys*. It's the mapping that decides which
character code each physical key on your keyboard produces --- the reason `Y`
and `Z` swap places on a German keyboard, or why a key combination produces an
accented letter on a French one. The keyboard layout maps your physical keys
to up to 256 character codes.

**Code page** is about your *screen*. It's the set of 256 character glyphs DOSBox
Staging can actually display on the screen --- *what* to draw for each of
those 256 character codes. It's literally a set of 256 bitmap images.

!!! info "Code page = screen font"

    Whenever you see "code page" in a DOS context, think **screen font**.
    Changing the code page doesn't touch your keyboard layout (physical key to
    character code mappings) --- it changes _which character glyphs_ appear on
    screen for a given character code. A program can only show accented
    letters, box-drawing characters, Cyrillic, Greek, and so on if the active
    code page (screen font) actually contains the bitmap images for them.

Keyboard layouts and code pages are independent: you could, in principle, type
on a physical German keyboard set to a German keyboard layout while the screen
displays the plain US character set (screen font), or type on a US keyboard
while the screen is set up to display Russian character images. Even so, each
keyboard layout has a **default code page** it's normally paired with --- for
example, the `de` (German) layout defaults to code page 858, and the `ru`
(Russian) layout defaults to a Cyrillic code page. Depending on what you need,
you can:

<div class="compact" markdown>

- Load a layout with its own **default code page** (the common case).
- Load a layout and **override the code page** it uses --- `uk 850` selects
  the British layout, but with the Western-European 850 code page instead of
  the layout's usual 437.
- **Change only the code page** while keeping whatever layout is already
  loaded --- this is what the `CHCP` command is for, covered below.

</div>

### Choosing a code page

DOSBox Staging bundles a large collection of code pages, covering far more
than DOS ever shipped with by default. They're grouped into a few families:

<div class="compact" markdown>

- A broad **standard set**, covering the classic, widely-used MS-DOS code
  pages (437, 850, 852, 866, and many more).
- An **ISO pack**, covering the ISO 8859 family of Latin code pages.
- A **KOI pack**, covering KOI8 Cyrillic code pages.
- A **MAC pack**, covering the classic Mac OS regional code pages.
- A **WIN pack**, covering the Windows ANSI code pages (1250, 1251, 1252,
  and so on).

</div>

Together these give broad coverage of Latin, Cyrillic, and Greek scripts,
plus a scattering of others --- see [below](#finding-out-whats-available) for
how to list them all.

!!! note "Custom code page files"

    If none of the bundled code pages fit your needs, `KEYB` also accepts a
    file name for a custom code page file in the standard DOS `.CPI` format
    (MS-DOS, DR-DOS, and Windows NT `.CPI` files are all supported directly;
    it needs to actually contain the code page you're asking for). This
    takes priority over the bundled code pages. FreeDOS-style `.CPX` files
    are compressed and not read directly --- decompress them first with the
    third-party [upx](https://upx.github.io/) tool.

!!! important "Graphics adapter requirements"

    Changing the screen font requires an [EGA](../../graphics/adapters.md#ega)
    or [(S)VGA](../../graphics/adapters.md#vga-and-svga) emulated video
    adapter. On older adapters (CGA, MDA, Hercules, PCjr), the hardware itself
    only has one built-in font, so DOSBox Staging always displays code page
    437 and `KEYB`/`CHCP` cannot switch it. If you need to change the screen
    font, make sure your [`machine`](../system/machine-types.md) setting is
    EGA or better.


### Finding out what's available

To see what's available, start DOSBox Staging with the following command line
arguments:

<div class="compact" markdown>

- [`--list-countries`](../using-dosbox-staging/command-line.md#-list-countries)
  --- lists all supported countries with their numeric codes
- [`--list-layouts`](../using-dosbox-staging/command-line.md#-list-layouts)
  --- lists all supported keyboard layouts with their codes
- [`--list-code-pages`](../using-dosbox-staging/command-line.md#-list-code-pages)
  --- lists all bundled code pages (screen fonts)

</div>

You can also use `KEYB /list` at the DOS prompt to show the list of layout
codes with the currently active one highlighted.


### Changing the layout permanently

To make a keyboard layout and code page your default, set them in
your [primary configuration](../../using-dosbox-staging/configuration#primary-configuration):

```ini
[dos]
keyboard_layout = de 858
```

This uses the same `LAYOUT [CODEPAGE]` pattern as the `KEYB` command's
arguments.

!!! note

    [`keyboard_layout`](#keyboard_layout) is only read when DOSBox Staging
    starts. Changing it afterwards at runtime has no effect on an
    already-running session (e.g., with `keyboard_layout fr`). If you want to
    change the keyboard layout or code page mid-session, use the `KEYB`
    command instead.


### KEYB and CHCP

Two commands help you manage keyboard layout and code pages from the DOS
prompt at runtime:

- **`KEYB`** --- changes the keyboard layout, and (unless told otherwise) the
  code page (screen font) that goes with it

- **`CHCP`** --- stands for **CH**ange **C**ode **P**age; changes *only* the
  code page (screen font), leaving the current keyboard layout untouched

Run `KEYB /?` or `CHCP /?` at the DOS prompt for the full syntax and options.

The following examples show how to use these commands in common scenarios.

**Set a keyboard layout and whatever font comes with it**

``` { . .dos-prompt }
KEYB de
```

This sets the German keyboard layout with the default code page (screen font) for
that layout.

**Set a keyboard layout *and* a specific font, overriding the
layout's default**

``` { . .dos-prompt }
KEYB de 858
```

This uses the `LAYOUT [CODEPAGE]` pattern mentioned earlier --- it works the
same way whether you type it after `KEYB` at the prompt or set it as the
[`keyboard_layout`](#keyboard_layout) value in your configuration.

**Keep the current keyboard layout, and only change the screen font**

``` { . .dos-prompt }
CHCP 850
```

This is the one to reach for when a program assumes a specific code page
(usually 437) to display correctly, but you don't want your keyboard layout to
change. For example, the game [Tommy's
Manor](https://www.mobygames.com/game/49191/tommys-manor/) expects code page
437 on screen; if you're running a non-US keyboard layout, `CHCP 437` fixes
the game's display without losing your keyboard layout.

!!! note

    `CHCP` can only switch to a code page that your *currently loaded*
    keyboard layout actually supports. Not every layout supports every code
    page --- the `us` layout is the one exception, and works with any code
    page. If `CHCP` refuses the code page you want, you have two options: pick
    a different code page that your current layout does support, or find a
    layout that does support the one you want and load both together with
    `KEYB LAYOUT CODEPAGE` (keep in mind this changes your keyboard layout
    too, not just the font). See [here](#finding-out-whats-available) for the
    full list of what's available.

**Use the genuine ROM font, not one of the bundled files**

``` { . .dos-prompt }
KEYB us /rom
```

This loads the layout using the video adapter's own built-in font instead of
one of the bundled code page files. It only has an effect when the resulting
code page is 437 (the ROM only contains that one font) --- for any other code
page, DOSBox Staging falls back to a bundled or custom file regardless.


### Checking what's currently loaded

Running `KEYB` on its own, with no arguments, shows the currently loaded
keyboard layout and code page --- it doesn't change or reset anything.

This is particularly useful for layouts that support more than one script.
Several layouts --- for example Russian, Greek, or Arabic ones --- can produce
both Latin letters and characters from their native script, and you can switch
between the two with a keyboard shortcut. Running `KEYB` with no arguments
shows you these shortcuts for the currently loaded layout.


## Configuration settings

### Interface language

You can set the interface language in the `[dosbox]` configuration section.

##### language

:   Set the language of DOSBox Staging's interface messages (`en` by
    default).

    Possible values are `de`, `en`, `es`, `fr`, `it`, `nl`, `pl`, `pt_BR`, and
    `ru`. 

    !!! note

        English is built-in; the rest is stored in the bundled
        `resources/translations` folder.

### Regional settings

You can set these in the `[dos]` configuration section.

##### country

:   Set the DOS country code (`1` by default, which stands for US English).
    This affects country-specific information such as date, time, and decimal
    formats.

    !!! note

        The list of country codes can be displayed using the
        [`--list-countries`](../using-dosbox-staging/command-line.md#-list-countries)
        command-line argument.


##### keyboard\_layout

:   Set the keyboard layout (`us` by default). The layout can be followed by the
    code page number to override the default one selected by the layout; e.g.,
    `uk 850` sets the British layout with a Western European code page (screen
    font).

    !!! note "Notes"

        - On real MS-DOS, you must configure the keyboard layout and the
          screen font separately; DOSBox Staging sets both from the provided
          layout and code.

        - The list of keyboard layout codes can be displayed using the
          [`--list-layouts`](../using-dosbox-staging/command-line.md#-list-layouts)
          command-line argument; e.g., `uk` is the British English layout.

        - The list of code pages can be displayed using the
          [`--list-code-pages`](../using-dosbox-staging/command-line.md#-list-code-pages)
          command-line argument; e.g., `437` is the original OEM-US code page.

        - Use the 'KEYB' command to manage keyboard layouts and code pages at
          runtime (run 'KEYB /?' for details).

        - Use the 'CHCP' command to change the code page (screen font) only
          while keeping the current keyboard layout (run 'CHCP /?' for details).


##### locale\_period

:   Select which era of locale data to use.

    Possible values:

    <div class="compact" markdown>

    - `historic` -- If data is available for the given country, mimic old DOS
      behaviour when displaying time, dates, or numbers.

    - `modern` -- Follow current-day practices for a user experience more
      consistent with typical host systems.

    </div>
