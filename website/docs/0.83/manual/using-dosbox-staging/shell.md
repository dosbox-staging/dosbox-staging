# The DOS shell

When DOSBox Staging starts, you'll usually spend a few moments at the DOS
command prompt before launching a game or application. This is the **DOS
shell**, the command interpreter that lets you navigate drives and
directories, mount storage, configure the environment, and start programs.
Most interactions with DOS begin here, so it's worth becoming familiar with a
few shell basics.

On a real PC, this role was filled by `COMMAND.COM`, the default command
processor shipped with MS-DOS. DOSBox Staging provides its own shell that is
largely compatible with the original while adding modern conveniences such as
[command history](#command-history), [tab completion](#tab-completion), and
[clipboard integration](#clipboard-integration).


## How DOS works

DOS was designed as a **single-user, single-tasking** operating system.
Normally only one program runs at a time --- there are no background
processes, no task switching, and no concurrent users. When you launch a game
or application, it takes over the entire machine. When it exits, you're back
at the shell prompt.

DOSBox Staging faithfully emulates this model. The shell prompt is where you
mount drives, configure settings, navigate directories, and launch programs.
Once a program starts, the shell waits until it exits before accepting more
commands.

The prompt itself shows your current **drive letter** and **directory**. For
example, `C:\GAMES>` tells you that you're on the **C: drive** in the `GAMES`
directory. The trailing `>` character indicates the shell is waiting for
input.

!!! note "Terminology"

    DOS refers to folders as **directories**, while modern operating systems
    generally use the term **folder**. They refer to the same thing, and
    throughout this manual we use the two terms interchangeably.

!!! info "Breaking the single-tasking paradigm"

    The "single-tasking" nature of DOS is actually not completely true. It was
    possible to install "background processes" via [TSR
    (Terminate-and-Stay-Resident)](https://en.wikipedia.org/wiki/Terminate-and-stay-resident_program)
    utilities even in early MS-DOS versions that could change the behaviour of
    the programs running in the "foreground" or play music in the background.
    Various clever programs existed to bring primitive multi-tasking
    capabilities into the DOS environment (e.g, Borland Sidekick, Quarterdeck
    DESQView, and DoubleDOS). These solutions enjoyed limited degrees of
    success --- the real breakthrough and switch to multi-tasking happened
    with the mass adoption of Windows 95.


## DOS filenames

DOS filenames traditionally follow the **8.3** convention: a maximum of 8
characters for the name and 3 for the extension (e.g., `DIR.EXE`). Filenames
are case-insensitive, so `DIR`, `dir`, and `Dir` all refer to the same file.
Names cannot contain spaces, and only a limited set of special characters is
allowed.

While DOSBox Staging supports long filenames on mounted host drives, many DOS
programs still expect or create traditional 8.3 names, so you'll encounter
them frequently.


## Internal and external commands

DOS commands come in two varieties:

- **Internal commands** are built into the shell itself --- they are always
  available and don't correspond to any file on disk. Examples include `DIR`,
  `CD`, `COPY`, `SET`, and `TYPE`.

- **External commands** are separate executable programs (`.COM`, `.EXE`, or
  `.BAT` files) that live on a drive. DOSBox Staging's own utilities ---
  `MOUNT`, `BOOT`, `MIXER`, `MOUSECTL`, `SHOWPIC` and many
  others --- reside on the [Y: and Z:
  drives](storage.md#dosbox-staging-drives), which are always available
  regardless of what other drives you have mounted.

This distinction explains why commands like `DIR` are always available, while
others depend on what software is installed or which drives are mounted.

When running a program, you normally omit its filename extension. For example,
typing `DOOM` is equivalent to typing `DOOM.EXE`, provided that `DOOM.EXE` is
the program DOS finds. If no extension is given, DOS searches for executable
files in the standard order: `.COM`, then `.EXE`, then `.BAT`, running the
first matching file it finds.

Most DOS commands provide built-in help. Appending the `/?` option displays a
brief usage summary:

``` { . .dos-prompt }
DIR /?
COPY /?
MOUNT /?
```

For a description of every command supported by DOSBox Staging, see [DOS
commands](commands.md).


## Getting around


### Editing the command line

The DOS shell supports full command-line editing. You can move the cursor, insert
and delete characters, and recall previous commands. On real MS-DOS, these
editing features were typically provided by the separate `DOSKEY` utility, but
are built into DOSBox Staging's DOS shell.

<div class="compact" markdown>

| Key                       | Action
| ------------------------- | --------------------------------------------
| ++left++ / ++right++      | Move cursor one character
| ++home++ / ++end++        | Jump to start / end of line
| ++backspace++             | Delete character before cursor
| ++delete++                | Delete character at cursor
| ++up++ / ++down++         | Navigate command history
| ++tab++ / ++shift+tab++   | Cycle through filename completions
| ++ctrl+v++                | Paste from host clipboard

</div>


### Tab completion

Press ++tab++ to complete file and directory names based on what you've typed
so far. If there are multiple matches, repeated presses of ++tab++ cycle
forward through them; ++shift+tab++ cycles backward. Typing any other key
accepts the current completion and resumes normal editing.

Tab completion prioritises executable files (`.COM`, `.EXE`, `.BAT`) over
non-executable file types, so the most likely match appears first. When used
with the `CD` command (pressing ++tab++ after typing `CD` and a space
character), only directories are shown.

The [Getting Started
guide](../../getting-started/setting-up-prince-of-persia.md#installing-the-game)
walks through tab completion with a practical example.


### Command history

Use ++up++ and ++down++ to scroll through previously entered commands. The
history is persistent across sessions --- your commands are saved to a
file (by default `shell_history.txt`) and restored when you next start DOSBox
Staging.

See the [`shell_history_file`](../system/dos.md#shell_history_file) setting to
change the history file location or disable persistent history.


### Clearing the screen

Run the `CLS` command to clear the screen and return the cursor to the
top-left corner. This is handy after a program leaves behind a screenful of
output.


### Clipboard integration

DOSBox Staging can exchange text with your host operating system's clipboard:

- Press ++ctrl+v++ to **paste** the first line from your host clipboard into
  the command line. This is especially useful when following a guide on the
  host side --- you can copy commands or file paths and paste them straight
  into DOSBox Staging instead of retyping them.

- Use the `CLIP` command to **copy** text to the clipboard, or to retrieve
  its contents. Combined with [piping](#piping-and-redirection), you can
  send command output straight to the host clipboard (for example, `DIR |
  CLIP`).


## Working with the shell

### Environment variables

DOS programs can read **environment variables** which are named values stored
in the DOS environment. Many DOS games and utilities use environment variables
to locate resources or configure their behaviour.

You can view all current variables with the `SET` command, or set one with
`SET NAME=VALUE`. When environment variable expansion is enabled, you can
reference variables in commands using the `%VARIABLE%` syntax. For example, if
`PATH` is set, typing `ECHO %PATH%` prints its value.

By default, variable expansion is enabled when the emulated DOS version is
7.0 or above (matching the behaviour of FreeDOS and MS-DOS 7.0's
`COMMAND.COM`). You can force it on or off with the
[`expand_shell_variable`](../system/dos.md#expand_shell_variable) setting.

!!! note

    Environment variable expansion on the command line (`%VAR%`) is separate
    from batch file parameter expansion (`%0` through `%9`), which is always
    available regardless of this setting.


### The `PATH` variable

When you type the name of a command or program, DOS first looks in the current
directory. If it doesn't find a matching executable there, it searches each
directory listed in the `PATH` environment variable until it finds one.

Display the current search path with:

``` { . .dos-prompt }
SET PATH
```

Set a new search path with:

``` { . .dos-prompt }
SET PATH=C:\DOS;C:\UTILS
```

In this example, DOS searches the current directory first, then `C:\DOS`
and `C:\UTILS`.

The `PATH` variable lets you run commonly used programs from any directory
without typing their full path. For example, if `EDIT.EXE` resides in
`C:\DOS` and that directory is listed in `PATH`, you can simply type:

``` { . .dos-prompt }
EDIT README.TXT
```

instead of:

``` { . .dos-prompt }
C:\DOS\EDIT README.TXT
```

DOSBox Staging's own commands, such as `MOUNT` and `SHOWPIC`, are available
regardless of the `PATH` setting because they are built into the DOSBox
Staging shell.


### Piping and redirection

The shell supports standard I/O redirection and piping, letting you chain
commands together or save output to files:

<div class="compact" markdown>

| Syntax                        | Description
| ----------------------------- | -------------------------------------------
| `command > file`              | Redirect command output to a file (overwrite)
| `command >> file`             | Append command output to a file
| `command < file`              | Read input from a file and pass it to the command
| `command1 \| command2`        | Pipe output of one command into another

</div>

Some practical examples:

<div class="compact" markdown>

| Command                  | Explanation
| ----                     | ----
| `ECHO Y | CHOICE`        | Pass the `Y` option to the `CHOICE` command
| `DIR /B > FILELIST.TXT`  | Create `FILELIST.TXT` that contains the list of files in the current directory
| `DIR | MORE`             | Show the current directory's contents and paginate the output
| `TYPE README.TXT | CLIP` | Copy the contents of `README.TXT` to the clipboard

Chained piping is also supported, such as `DIR | SORT | MORE` for displaying
sorted directory output one screen at a time (provided that you have the
`SORT` command from MS-DOS or FreeDOS in your [path](#the-path-variable)).

The [Getting Started
guide](../../getting-started/passport-to-adventure.md#installing-the-game)
demonstrates piping with a practical example.

!!! note

    If the current directory and C: are both read-only, or no C: drive is
    mounted at all, the environment variable `%TEMP%` (or `%TMP%`) needs to be
    set within DOS pointing to a writable directory so that piping will work
    properly (e.g. `SET TEMP=C:\TEMP`).


### Text modes

The `MODE` command lets you switch the shell's text mode to display more (or
fewer) columns and lines. For example:

``` { . .dos-prompt }
MODE 80,43
MODE 132x50
MODE CON COLS=80 LINES=25
```

Available modes depend on the emulated graphics adapter. On the default S3
SVGA adapter, all 80-column and 132-column modes are available (e.g.,
`80x25`, `80x43`, `80x50`, `132x25`, `132x43`, `132x50`, `132x60`).
Earlier adapters like CGA and EGA support fewer modes.

Run `MODE /?` for the full list of supported modes and additional options such
as setting the keyboard repeat rate.

!!! warning

    Some programs expect the standard **80&times;25** text mode when they start
    up and will fail or display garbage if a different mode is active.
    In such cases try switching back to the default mode first with  `MODE
    80x25`.


### Character encodings

DOS predates Unicode. Instead of a single universal character set, DOS uses
**code pages** which are tables that map byte values to characters.

The first 128 characters (0--127) are standard ASCII and are the same
everywhere: control characters, the Latin alphabet, digits, and common
punctuation. The upper 128 characters (128--255) vary by code page and may
include accented letters, currency symbols, box-drawing characters, and other
glyphs.

The default code page is **437** (US English), which includes the box-drawing
characters many DOS applications use for their text-mode interfaces. Localised
DOS installations used different code pages (e.g., **850** for Western
European languages or **866** for Cyrillic).

Supporting multiple languages in DOS involves keyboard layouts, code pages,
display fonts, and regional settings, all of which interact with each other.
The `KEYB` command can be used to switch keyboard layouts; see the
[Localisation](../system/localisation.md) chapter for details on keyboard
layouts, code pages, and regional settings.


## Batch files & automation


### Batch files

Batch files with the `.BAT` extension are plain-text scripts containing a
sequence of DOS commands. When you run a batch file, the shell executes each
line in order, just as if you had typed them at the prompt.

Additionally, batch files support control flow (`IF`, `GOTO`, `FOR`),
subroutine calls (`CALL`), user interaction (`CHOICE`, `PAUSE`), and parameter
passing (`%1` through `%9` with `SHIFT`).

See the [batch file commands](commands.md#batch-file-commands) section of
the DOS commands reference for the full list.


### The autoexec section

The `[autoexec]` section in your [configuration file](configuration.md) works
like a built-in batch file that runs automatically when DOSBox Staging starts.
This is the standard place to mount drives, set environment variables, and
launch games.

See [Autoexec](configuration.md#autoexec) for details and examples.
