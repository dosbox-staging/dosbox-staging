# Translation workflow

DOSBox Staging stores the translations in a `gettext` compatible file format,
but it does not use the `gettext` library. You can edit them with your favorite
`*.po` file editor, but please follow the workflow described here instead of
using tools like `xgettext`.

## Choosing the editor

### ASCII file editors

You can update the translations using any ASCII editor with UTF-8 support, like
**Notepad** (Windows), **Notepad++** (Windows), **TextMate** (macOS),
**Kate** (Linux, KDE), or **Text Editor** (Linux, GNOME).

Integrated development environments, like **Visual Studio Code**, should work
too.

> [!WARNING]
>
> Word processors, like **Microsoft Word** or **Apple Pages**, are not suitable!

### PO file editors

Since the `PO` file format is widespread, several dedicated editors exist, i.e.:
- **Poedit**, cross-platform
- **Lokalize**, a KDE tool for Linux
- **Gtranslator**, a GNOME tool for Linux
- [Loco](https://localise.biz/free/poeditor), web-based

For DOSBox Staging translation files, it is best to configure the editor to use
a monospaced (non-proportional) font:

- **Poedit**\
_File_ -> _Preferences_ -> _General_ -> _Appearance_ ->
_Use&nbsp;custom&nbsp;text&nbsp;field&nbsp;font_
- **Lokalize**\
_Settings_ -> _Configure&nbsp;Lokalize..._ -> _Appearance_ ->
_Editor&nbsp;font_ -> _Show&nbsp;only&nbsp;monospaces&nbsp;fonts_
- **Gtranslator**\
hamburger&nbsp;menu -> _Preferences_ -> _Editor_ -> _Font_

## Adding new translation

To start a new translation, execute the following commands in DOSBox Staging:

```bat
config -set language=en
config -wl xx.po
```

where _xx_ is a language code. Use a two-letter code according to the
[ISO 639](https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes)
standard.

This will create a `xx.po` file containing all the English messages, ready for
translating.

> [!NOTE]
>
> If justifiable, it is allowed to use a _xx\_XX_ form (`config -wl xx_XX.po`),
> where _XX_ is a two-letter country or territory code according to the
> [ISO 3166-1 alpha-2](https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2)
> standard.
> Example: the Brazilian Portuguese language differs significantly from the
> Portuguese spoken in Europe, therefore it is reasonable to use a `pt_BR.po`
> name for the translation file.

## Updating existing translation

Before you start editing any current `PO` file, you'll need to refresh its
content. Execute the following commands inside the current alpha build of
DOSBox Staging:

```bat
config -set language=xx
config -wl xx.po
```

where _xx_ is a language code.

This will create an updated `xx.po` file with removed outdated messages,
updated English messages, updated source code references, outdated translations
marked accordingly, and without the messages that have been removed. This newly
created file will be ready for updating the translation.

## PO file format

The `PO` file format is described in detail on the
[gettext documentation](https://www.gnu.org/software/gettext/manual/html_node/PO-Files.html)
web page; this chapter only provides a brief introduction.

The file consists of entries of roughly the same layout, separated by empty
lines. DOSBox Staging needs the file to be saved as UTF-8 — don't change the
encoding, otherwise the file won't work correctly.

### Standard metadata entry

The first entry in the file is reserved for the standard gettext metadata — it
always contains an empty English string (`msgid`); the translated string
(`msgstr`) contains the metadata entries.

```po
msgid ""
msgstr ""
"Project-Id-Version: dosbox-staging\n"
"Report-Msgid-Bugs-To: https://github.com/dosbox-staging/dosbox-staging/issues\n"
...
```

If you are editing the file by hand, using an ASCII editor, don't change this
entry.

### DOSBox Staging metadata entries

DOSBox Staging needs some additional, non-standard metadata marked with the
`#METADATA` keyword in both the the context (`msgctxt`) and location (`#:`)
fields, for example:

```po
#. Do not translate this, set it according to the instruction!
#: #METADATA
#, fuzzy
msgctxt "#METADATA"
msgid ""
"#SCRIPT\n"
"\n"
"Writing script used in this language, can be one of:\n"
"Latin, Arabic, Armenian, Cherokee, Cyrillic, Georgian, Greek, Hebrew"
msgstr ""
```

This particular entry tells DOSBox Staging what writing script is used by the
target language; this helps Staging determine which DOS code pages are
compatible with the translation.

The `fuzzy` flag (the list of flags starts with `#,`) tells us this entry needs
your attention — here the writing script is not specified. Most likely this is
going to be a Latin script — we need to put it into the `msgstr` and remove the
`fuzzy` flag, like this:

```po
#. Do not translate this, set it according to the instruction!
#: #METADATA
msgctxt "#METADATA"
msgid ""
"#SCRIPT\n"
"\n"
"Writing script used in this language, can be one of:\n"
"Latin, Arabic, Armenian, Cherokee, Cyrillic, Georgian, Greek, Hebrew"
msgstr "Latin"
```

> [!IMPORTANT]
>
> Never ever change any other flags — `fuzzy` flag is the only one you are
> allowed to remove (or add).
>
> Some dedicated `PO` editors use a different name for the `fuzzy` flag in
> their user interfaces, like _translation status_, _work needed_, etc.
>
> Some important messages (command help, config option help, introduction) are
> displayed in English if the current translation marks the message as `fuzzy`.
> This is to prevent displaying outdated (very likely no longer correct) help
> messages if the translation is not up to date.

### Translatable entries

Here is an example translatable entry:

```po
#: src/dos/programs/mount_common.cpp:76
#, fuzzy, c-format, no-wrap
msgctxt "PROGRAM_MOUNT_STATUS_1"
msgid "The currently mounted drives are:"
msgstr "Les lecteurs montés actuellement sont :"
```

- The location (after `#:`) tells where the message is actually located in the
source code — it is updated every time you write the translation file.
Some `PO` file editors can open the relevant source code if you configure the
root path to the project.
- The `msgctxt` (a context according to gettext terminology) is a unique message
identifier used by DOSBox Staging. Never ever change it, or the emulator won't
be able to use the entry!
- The `msgid` contains an English message — as above, don't change it!
- The `msgstr` contains a translated message, empty if the translation is
missing. This is the part you need to update :)
- The `fuzzy` flag we have already seen, is set by  DOSBox Staging to tell you
this particular entry needs your attention. This might be a missing translation,
an outdated translation (these are detected by comparing `msgid` to the current
English message in the code), or any other reason — see the warning logs when
loading the translation. After having updated the translation, remove the
`fuzzy` flag!

> [!NOTE]
>
> The legacy DOSBox translation file format did not allow detecting outdated
> translations — therefore, if the `PO` file hasn't been touched since being
> converted from the old format, all the messages will be marked `fuzzy`.

### Line breaks and escape sequences

A slightly more complex example:

```po
#: src/config/setup.cpp:274
#, c-format, no-wrap
msgctxt "CONFIG_NOSOUND"
msgid ""
"Enable silent mode ('off' by default).\n"
"Sound is still emulated in silent mode, but DOSBox outputs no sound to the host.\n"
"Capturing the emulated audio output to a WAV file works in silent mode."
msgstr ""
"Abilita la modalità silenziosa (predefinito 'off').\n"
"In questa modalità, l'emulazione audio è comunque abilitata ma DOSBox non\n"
"riprodurrà alcun suono. In modalità silenziosa, l'acquisizione dell'uscita\n"
"audio emulata in un file WAV funzionerà ugualmente."
```

This entry contains a multi-line message. It is important to note that a line
break occurs not where the line ends in the `PO` file, but where a special
_escape sequence_ `\n` is placed!

To avoid ambiguity, all the occurrences of the `\` symbol in the message have to
be replaced with a double backslash _escape sequence_, `\\`, in both English and
translated messages.

Similarly, all the double-quotes (`"`) have to be replaced with the _escape
sequence_ `\"`.

Most dedicated `PO` editors offer escape sequence syntax highlighting.

### C-style format strings

Many strings contain strange-looking sequences starting from the percentage
sign, like `%s` or `%02d`. These are so-called _printf format strings_, used by
C and C++ programming languages; they are replaced with another string or
number, using the specified format.

You'll need to understand the notation to properly format the output (like
tables, when the translated headers have different lengths than in English).
Search the web to learn more about it; here are a few links:

- [printf article on Wikipedia](https://en.wikipedia.org/wiki/Printf)
- [printf article on cplusplus.com](https://cplusplus.com/reference/cstdio/printf)
- [printf article on cppreference.com](https://en.cppreference.com/w/cpp/io/c/fprintf)

The `c-format` flag notes that the message can contain a C-style format string.
Some dedicated `PO` editors might offer format string syntax highlighting or
even error checking.

If DOSBox Staging detects a mismatched (incompatible) format string, it refuses
to use the translation for that particular message, prints out a warning log,
and marks the message as `fuzzy` the next time the `PO` file is saved.

## Steps before creating a pull request

There are still a couple of things to do after you are done with editing the
`PO` file.

UTF-8 can store the same exact text in several different ways. There are some
standard ones, however, called _normalized forms_. Although
DOSBox Staging should be able to handle non-normalized translation files, and
normally text editors save normalized files already, it is still highly
recommended to normalize the updated `PO` file.

In the `extras/translations` directory, you'll find two scripts: `normalize.sh`
(for Linux and macOS) and `normalize.bat` (for Windows). The scripts convert
the translation files in the `resources/translations` directory to the
NFC-normalized form (recommended by DOSBox Staging and used by default by most
UTF-8 capable text editors), with Unix line endings. 

Now you'll need to clean up the file. Once again, start DOSBox Staging (this
time with your updated `PO` file) and execute the following commands:

```bat
config -set language=xx
config -wl xx.po
```

This will write the `PO` file again, using the standard DOSBox Staging
formatting style. This is especially important if you used a dedicated `PO` file
editor.

> [!IMPORTANT]
>
> Don't try to convert the `PO` file to the binary `MO` format, it is not
> supported by DOSBox Staging.

Once you've done these steps and you've tested your translations (see the last
chapter for some tips), you can submit the updated translation for inclusion
into DOSBox Staging.

## Language-specific remarks

Sometimes the DOS code page does not contain all the characters needed by the
translation. Contrary to most Unicode engines, the DOSBox Staging internal one
was specially designed to be able to replace the missing characters with sane
alternatives (if it does not work correctly for your language — let us know,
the rules can be tweaked to some extent).

But it, too, has some limitations. For example it can only replace a single
character with another single character; it can't replace a digraph with two
characters, as this could break the message layout — make the line too long to
display or break the indentation.

## French

DOS code page 850 does not contain `Œ` and `œ` digraphs — please replace them
with `OE` and `oe`, respectively.
The digraphs are present on the modified code page 859, but we want to stay
compatible with the standard MS-DOS code page.

## Message-specific remarks

### Configuration option descriptions

For example, `CONFIG_FULLSCREEN`.

Do not exceed 80 characters on a line, or commands such as `config -h fullscreen`
won't be able to display the help properly, and might wrap the text or display blank
lines.

### Command or program help

For example `PROGRAM_MOUNT_HELP_LONG` or `SHELL_CMD_CLS_HELP_LONG`. Sometimes
the first line of the help message is extracted into a separate string,
like `SHELL_CMD_CLS_HELP`.

Do not exceed 80 characters in a line, or commands like `cls /?` won't be able to
display the help properly. Also test that the `help /all` command properly extracts
and displays the shortened command descriptions.

### ANSI colours and the 80-character line limit

Adhering to the 80 character line limit is very hard when using ANSI colour
tags (e.g., `[color=light-green]COMMAND[reset]`). The web-based ANSI Markup
Editor tool can make this a lot easier:

https://www.dosbox-staging.org/tools/ansi-editor/

Press the **Help** button and just follow the instructions.

> [!TIP]
>
> This is a single-page webapp; if you want to use it offline, just save
> the page's source to your computer as a `.html` file and open it in any
> browser.

### Startup screen strings

For example, `SHELL_STARTUP_BEGIN` or `SHELL_STARTUP_CGA`.

If you modify these strings, test the startup screen with `startup_verbosity = high`
for all the relevant `machine = (...)` settings. It is really easy to make a mistake
here.

### Country names

For example, `COUNTRY_NAME_USA` or `COUNTRY_NAME_BIH_LAT`.

Usually the country code is taken from the _ISO 3166-1 alpha-3_ norm, but
sometimes they can contain an additional suffix specifying a language or
alphabet, or a single 'country' can describe the whole geographic region, like
on real MS-DOS.

After translating the country names, check them by running DOSBox with
`--list-countries` command line argument.

### Keyboard layout names

For example, `KEYBOARD_LAYOUT_NAME_US` or `KEYBOARD_LAYOUT_NAME_PL214`.

After translating, check them by running DOSBox with `--list-layouts` command line
argument, also check the `keyb /list` internal command output.
