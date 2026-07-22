# The DOS shell

When you start DOSBox Staging, you're greeted by a command prompt ---
something like `Z:\>` or `C:\>`. This is the **shell**, the DOS command
interpreter that reads what you type, executes commands, and displays the
results. It's the primary way you interact with the emulated DOS environment.

On a real PC, this role was filled by `COMMAND.COM`, the default command
processor shipped with MS-DOS. DOSBox Staging provides its own clean-room
reimplementation that is compatible with the original but includes several
modern quality-of-life enhancements such as [command
history](#command-history), [tab completion](#tab-completion), and [clipboard
integration](#clipboard-integration).


## How DOS works

DOS was designed as a **single-user, single-tasking** operating system.
Generally, only one program runs at a time --- there are no background
processes, no task switching, and no concurrent users. When you launch a game
or application, it takes over the entire machine. When it exits, you're back
at the shell prompt.

DOSBox Staging faithfully emulates this model. The shell prompt is where you
mount drives, configure settings, navigate directories, and launch programs.
Once a program is running, the shell is suspended until the program finishes.

The prompt itself shows your current **drive letter** and **directory**. For
example, `C:\GAMES>` tells you that you're on the C drive in the `GAMES`
directory.

!!! note "Terminology"

    DOS refers to folders as **directories**, while modern operating systems
    generally use the term **folder**. They refer to the same thing, and
    throughout this manual we use the two terms interchangeably.

!!! info "Breaking the single-tasking paradigm"

    The "single-tasking" nature of DOS is actually not completely true. It was
    possible to install "background processes" via [TSR
    (Terminate-and-Stay-Resident)](TODO) utilities even in early MS-DOS
    versions that could change the behaviour of the programs running in the
    "foreground" or play music in the background. Various clever solutions
    existed to bring primitive multi-tasking capabilities into the DOS
    environment (e.g, [Sidekick](), TODO more examples). These solutions
    enjoyed limited degrees of success --- the real breakthrough and switch to
    multi-tasking happened with the mass adoption of Windows 95. Many of these
    solutions work in DOSBox Staging, but you won't need them for running
    games.


## Internal and external commands

DOS commands come in two varieties:

- **Internal commands** are built into the shell itself --- they are always
  available and don't correspond to any file on disk. Examples include `DIR`,
  `CD`, `COPY`, `SET`, and `TYPE`.

- **External commands** are separate executable programs (`.COM`, `.EXE`, or
  `.BAT` files) that live on a drive. DOSBox Staging's own utilities ---
  `MOUNT`, `MIXER`, `IMGMOUNT`, and others --- reside on the [Z:
  drive](storage.md#dosbox-staging-drives), which is always available
  regardless of what other drives you have mounted.

See [DOS commands](commands.md) for the complete command reference.


## Getting around


### Editing the command line

The shell supports full command-line editing. You can move the cursor, insert
and delete characters, and recall previous commands --- all features that
required a separate `DOSKEY` utility on real MS-DOS systems (shipped with
MS-DOS 5.0 in 1991), but are built into DOSBox Staging's DOS shell.

<div class="compact" markdown>

| Key                       | Action
| ------------------------- | --------------------------------------------
| ++left++ / ++right++      | Move cursor one character
| ++home++ / ++end++        | Jump to start / end of line
| ++backspace++             | Delete character before cursor
| ++delete++                | Delete character at cursor
| ++escape++                | Clear the current line
| ++f3++                    | Complete from the last command
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
other file types, so the most likely match appears first. When used with the
`CD` command, only directories are shown.

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
in the DOS environment. You can view all current variables with the `SET`
command, or set one with `SET NAME=VALUE`.

When environment variable expansion is enabled, you can reference variables
in commands using the `%VARIABLE%` syntax. For example, if `PATH` is set,
typing `ECHO %PATH%` prints its value.

By default, variable expansion is enabled when the emulated DOS version is
7.0 or above (matching the behaviour of FreeDOS and MS-DOS 7.0's
`COMMAND.COM`). You can force it on or off with the
[`expand_shell_variable`](../system/dos.md#expand_shell_variable) setting.

!!! note

    Environment variable expansion on the command line (`%VAR%`) is separate
    from batch file parameter expansion (`%0` through `%9`), which is always
    available regardless of this setting.


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
| `DIR /B > FILELIST.TXT`  | Create `FILELIST.TXT` that contains the list of files in the current directory
| `DIR | MORE`             | Show the current directory's contents and paginate the output
| `TYPE README.TXT | CLIP` | Copy the contents of `README.TXT` to the clipboard

The [Getting Started guide](../../getting-started/passport-to-adventure.md#installing-the-game)
demonstrates piping with a practical example.


### Text modes

The `MODE` command lets you switch the shell's text mode to display more (or
fewer) columns and lines. For example:

```
MODE 80,43
MODE 132x50
MODE CON COLS=80 LINES=25
```

Available modes depend on the emulated graphics adapter. On the default S3
SVGA adapter, all 80-column and 132-column modes are available (e.g.,
`80x25`, `80x43`, `80x50`, `132x25`, `132x43`, `132x50`, `132x60`).
Earlier adapters like CGA and EGA support fewer modes.

Run `MODE /?` the full list of supported modes and additional options such as
setting the keyboard repeat rate.

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

DOS localisation is a complex topic; keyboard layouts, display fonts, and file
name handling all depend on code page settings and can interact in surprising
ways. The [`KEYB`](commands.md) command can be used to switch keyboard
layouts; see the [Localisation](../system/localisation.md) chapter for details
on keyboard layouts, code pages, and regional settings.


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
