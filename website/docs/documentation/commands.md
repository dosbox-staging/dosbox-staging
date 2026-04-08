# DOS commands & programs

DOSBox Staging provides a set of built-in commands and programs available at
the DOS prompt. These fall into three categories:

- **Shell commands** --- built into the command interpreter (COMMAND.COM).
  These are clean-room reimplementations of classic MS-DOS commands, designed
  to be compatible but not copied from MS-DOS source code.

- **Built-in programs** --- executable files on the Z: drive, created by
  DOSBox Staging at startup. Some reimplement standard DOS utilities (ATTRIB,
  MORE, TREE); others are DOSBox-specific tools for managing the emulator
  (MOUNT, CONFIG, MIXER).

- **Bundled third-party programs** --- FreeDOS utilities shipped as binaries
  on the Y: drive.

!!! tip

    Every command supports the `/?` flag for detailed usage help. Type
    `COMMAND /?` at the DOS prompt to see the full documentation for any
    command listed below --- for example, `MOUNT /?` or `DIR /?`.


## Shell commands

These commands are built into the command interpreter and are always
available. They do not appear as files on any drive.

<div class="compact" markdown>

| Command | Description |
|---------|-------------|
| `CALL`  | Start a batch program from within another batch program |
| `CD`    | Display or change the current directory |
| `CLS`   | Clear the DOS screen |
| `COPY`  | Copy one or more files |
| `DATE`  | Display or change the internal date |
| `DEL`   | Remove one or more files |
| `DIR`   | Display a list of files and subdirectories in a directory |
| `ECHO`  | Display messages and enable/disable command echoing |
| `EXIT`  | Exit from the DOS shell |
| `FOR`   | Run a specified command for each string in a set |
| `GOTO`  | Jump to a labeled line in a batch program |
| `IF`    | Perform conditional processing in batch programs |
| `LH`    | Load a DOS program into upper memory |
| `MD`    | Create a directory |
| `PATH`  | Display or set a search path for executable files |
| `PAUSE` | Wait for a keystroke to continue |
| `RD`    | Remove a directory |
| `REM`   | Add comments in a batch program |
| `REN`   | Rename one or more files |
| `SET`   | Display or change environment variables |
| `SHIFT` | Left-shift command-line parameters in a batch program |
| `TIME`  | Display or change the internal time |
| `TYPE`  | Display the contents of a text file |
| `VER`   | Display the DOS version |
| `VOL`   | Display the disk volume and serial number, if they exist |

</div>

Some commands have aliases: `CD` / `CHDIR`, `DEL` / `DELETE` / `ERASE`,
`LH` / `LOADHIGH`, `MD` / `MKDIR`, `RD` / `RMDIR`, `REN` / `RENAME`.


## Built-in programs

These programs reside on the Z: drive and are always in the PATH. They
include both reimplementations of standard DOS utilities and DOSBox
Staging-specific tools for managing the emulator.

<div class="compact" markdown>

| Command      | Description |
|--------------|-------------|
| `ATTRIB`     | Display or change file attributes |
| `AUTOTYPE`   | Perform scripted keyboard entry into a running DOS game |
| `BOOT`       | Boot DOSBox Staging from a DOS drive or disk image |
| `CHOICE`     | Wait for a keypress and set an ERRORLEVEL value |
| `CLIP`       | Copy text to the clipboard or retrieve the clipboard's content |
| `CONFIG`     | Perform configuration management and other miscellaneous actions |
| `HELP`       | Display help information for DOS commands |
| `INTRO`      | Display a full-screen introduction to DOSBox Staging |
| `KEYB`       | Configure a keyboard layout and screen font |
| `LOADFIX`    | Load a program in a specific memory region and then run it |
| `LOADROM`    | Load a ROM image of the video BIOS or IBM BASIC |
| `LS`         | Display directory contents in wide list format |
| `MAKEIMG`    | Create a new empty disk image |
| `MEM`        | Display the amount of used and free memory |
| `MIXER`      | Display or change the sound mixer settings |
| `MODE`       | Set the display mode or the keyboard's typematic rate |
| `MORE`       | Display command output or text file one screen at a time |
| `MOUNT`      | Mount a directory or an image file to a drive letter |
| `MOUSE`      | Load the built-in mouse driver |
| `MOUSECTL`   | Manage physical and logical mice |
| `MOVE`       | Move files and rename files and directories |
| `RESCAN`     | Scan for changes on mounted DOS drives |
| `SERIAL`     | Manage the serial ports |
| `SETVER`     | Display or set the DOS version reported to applications |
| `SHOWPIC`    | Display an image file |
| `SUBST`      | Assign an internal directory to a drive |
| `TREE`       | Display directory tree in a graphical form |

</div>

!!! note

    `IMGMOUNT` is deprecated and forwards to `MOUNT`.


## Bundled third-party programs

DOSBox Staging ships a small number of
[FreeDOS](https://www.freedos.org/) utilities as pre-compiled binaries on
the Y: drive. When the Y: drive is
[automounted](system/general.md#automount) (enabled by default), the
`Y:\DOS` directory is added to the PATH automatically, making these commands
available from any drive.

These are **not** DOSBox Staging reimplementations --- they are third-party
open-source programs included for convenience.

<div class="compact" markdown>

| Command    | Description | Origin |
|------------|-------------|--------|
| `DEBUG`    | Interactive DOS debugger | [DOS-debug](https://github.com/Baron-von-Riedesel/DOS-debug) |
| `DELTREE`  | Delete a directory tree including all files and subdirectories | [FreeDOS](https://gitlab.com/FreeDOS/base/deltree) |
| `XCOPY`    | Extended file copy with subdirectory support | [FreeDOS](https://github.com/joncampbell123/dosbox-x/tree/master/src/builtin/xcopy) |

</div>
