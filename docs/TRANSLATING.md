# General translation workflow

## Preparing a file with English strings

You need to be sure you have the latest English messages file — you need the
latest version of DOSBox Staging for this purpose. Do one of the following:

1. Run DOSBox Staging using the command:
   `dosbox -lang en -c "config -wl en.lng" -c "exit"`
2. Inside DOSBox Staging execute the following commands:
   - `config -set language=en` — to make sure the English language is used
   - `config -wl en.lng` — to write the language file
   You can supply a full host OS path instead of simply `en.lng`.

Any of these will produce a file with the current strings to translate; use it
as a reference.

## Cleaning up a translation file

In the same way, you can create a cleaned-up translation file for the given
language — i.e. for French (`fr`) this would be:

```
dosbox -lang fr -c "config -wl fr.lng" -c "exit"
```

or, if you prefer to save the file from inside DOSBox Staging:

```
config -set language=fr
config -wl fr.lng
```

The cleaned-up language file is sorted, it has unused strings removed, and
missing strings are replaced with their English counterparts.

## Updating the translation

Update a given translation by editing the corresponding `*.lng` file with a text
editor and saving it with UTF-8 encoding and Unix line endings.

Suitable editors are often called ASCII editors, code editors, etc. —
Microsoft Word is not suitable, but Notepad (Windows), Notepad++ (Windows),
TextMate (macOS), or Kate (Linux) will do.

Please don't use tab characters — configure your editor to expand tabs to
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

## Scripts - `extras/translations` directory

### `normalize.sh` (Linux, macOS) or `normalize.bat` (Windows)

UTF-8 can store the same exact text in several different ways. There are some standard
ones, however, called _normalized forms_. These scripts convert the translation files
to the NFC-normalized form (recommended by DOSBox Staging and used by default
by most UTF-8 capable text editors), with Unix line endings.
