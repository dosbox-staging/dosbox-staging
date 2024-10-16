
# General translation workflow

## Preparing a file with English strings

Be sure you have the latest English messages file, `en.lng` (the one present
in this directory is likely outdated), by running:

```
dosbox -lang en -c "config -wl en.lng" -c "exit"
```

or do the following:

1. In the configuration file, go to the `[dosbox]` section and set `language = en`.
2. Run the most current version of DOSBox Staging.
3. Inside DOSBox execute the command `config -wl en.lng` (you can supply a host OS
  path, instead of simply `en.lng`).

This will produce a file with the current strings to translate; use it as a
reference.

## Updating the translation

Update a given translation by editing the corresponding `*.lng` file with a text
editor and saving it with UTF-8 encoding and Unix line endings.

Suitable editors are often called ASCII editors, code editors, etc.---Microsoft Word
is not suitable, but Notepad (Windows), Notepad++ (Windows), TextMate (macOS),
or Kate (Linux) will do.

Please don't use tab characters---configure your editor to expand tabs to
spaces. If you use tabs, there is a danger that the alignment of indented
multi-line text will appear wrong.

Tools exist (often called diff tools) that can help you to compare two `en.lng`
files generated from different versions of DOSBox, they make it easy to figure out
what has changed in the English strings and what needs to be addressed in your
updated translations. These include Meld (cross-platform), WinMerge (Windows),
and Kompare (Linux).

Depending on your host OS, use the correct script to normalize the output. Although
DOSBox Staging should be able to handle non-normalized translation files, and
normally text editors save normalized files already, it is still highly recommended
to use the script to make sure no problem occurs.

## String-specific tips

### Format strings

Many strings contain strangely looking sequences starting from the percentage sign,
like `%s` or `%02d`. These are so-called _printf format strings_, used by C/C++
programming languages; they are replaced with another string or number, using the
specified format.

You might need to understand the notation to properly format the output (like tables,
when the translated headers have different length than in English). Search the web
to learn more about it, here are a few links:

- [printf article on Wikipedia](https://en.wikipedia.org/wiki/Printf)
- [printf article on cplusplus.com](https://cplusplus.com/reference/cstdio/printf)
- [printf article on cppreference.com](https://en.cppreference.com/w/cpp/io/c/fprintf)

### Configuration option descriptions

For example `CONFIG_FULLSCREEN`.

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

### Startup screen strings

For example `SHELL_STARTUP_BEGIN`, `SHELL_STARTUP_CGA`.

If you modify these strings, test the startup screen with `startup_verbosity = high`
for all the relevant `machine = (...)` settings. It is really easy to make a mistake
here.

### Country names

For example `COUNTRY_NAME_USA` or `COUNTRY_NAME_BIH_LAT`. Usually the country code
is taken from the _ISO 3166-1 alpha-3_ norm, but sometimes they can contain
an additional suffix specifying a language or alphabet, or a single 'country'
can describe the whole geographic region, like on real MS-DOS.

After translating the country names, check them by running DOSBox with
`--list-countries` command line argument.

### Keyboard layout names

For example `KEYBOARD_LAYOUT_NAME_US` or `KEYBOARD_LAYOUT_NAME_PL214`.

After translating, check them by running DOSBox with `--list-layouts` command line
argument, also check the `keyb /list` internal command output.

# Scripts

## `normalize.sh` (Linux, macOS) or `normalize.bat` (Windows)

UTF-8 can store the same exact text in several different ways. There are some standard
ones, however, called _normalized forms_. These scripts convert the translation files
to the NFC-normalized form (recommended by DOSBox Staging and used by default
by most UTF-8 capable text editors), with Unix line endings.

## `update-sources.sh` (Linux, macOS)

This script performs some updates on the translation files - creates an up to date
`en.lng` file, removes obsolete strings from translation files, adds missing strings
(in English), reorders (sorts) translation file content, and finally normalizes them.
