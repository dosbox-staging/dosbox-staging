# DOS commands

DOSBox Staging provides a comprehensive set of commands available at the DOS
prompt. Most are clean-room reimplementations of classic MS-DOS commands,
designed to be compatible with their original counterparts but not derived
from MS-DOS source code. A number of additional commands are specific to
DOSBox Staging and provide emulator management features like mounting drives
and controlling the audio mixer.

!!! tip

    Type `HELP` at the DOS prompt to see the most commonly used commands, or
    `HELP /ALL` for the complete list. Every command supports the `/?` flag
    for detailed usage help --- for example, `MOUNT /?` or `DIR /?`.


## DOSBox Staging commands

These commands are specific to DOSBox Staging and have no MS-DOS equivalent.

<div class="compact" markdown>

| Command      | Description |
|--------------|-------------|
| `AUTOTYPE`   | Perform scripted keyboard entry into a running DOS game |
| `BOOT`       | Boot DOSBox Staging from a DOS drive or disk image |
| `CLIP`       | Copy text to the clipboard or retrieve the clipboard's content |
| `CONFIG`     | Perform configuration management and other miscellaneous actions |
| `LOADROM`    | Load a ROM image of the video BIOS or IBM BASIC |
| `MAKEIMG`    | Create a new empty disk image |
| `MIXER`      | Display or change the sound mixer settings |
| `MOUNT`      | Mount a directory or an image file to a drive letter |
| `MOUSECTL`   | Manage physical and logical mice |
| `RESCAN`     | Scan for changes on mounted DOS drives |
| `SERIAL`     | Manage the serial ports |
| `SHOWPIC`    | Display an image file |

</div>

!!! note

    `IMGMOUNT` is still accepted as an alias for `MOUNT`.


## File & directory commands

<div class="compact" markdown>

| Command  | Aliases            | Description |
|----------|--------------------|-------------|
| `ATTRIB` |                    | Display or change file attributes |
| `CD`     | `CHDIR`            | Display or change the current directory |
| `COPY`   |                    | Copy one or more files |
| `DEL`    | `DELETE`, `ERASE`  | Remove one or more files |
| `DIR`    |                    | Display a list of files and subdirectories in a directory |
| `LS`     |                    | Display directory contents in wide list format |
| `MD`     | `MKDIR`            | Create a directory |
| `MORE`   |                    | Display command output or text file one screen at a time |
| `MOVE`   |                    | Move files and rename files and directories |
| `RD`     | `RMDIR`            | Remove a directory |
| `REN`    | `RENAME`           | Rename one or more files |
| `SUBST`  |                    | Assign an internal directory to a drive |
| `TREE`   |                    | Display directory tree in a graphical form |

</div>


## Batch file commands

<div class="compact" markdown>

| Command  | Description |
|----------|-------------|
| `CALL`   | Start a batch program from within another batch program |
| `CHOICE` | Wait for a keypress and set an ERRORLEVEL value |
| `ECHO`   | Display messages and enable/disable command echoing |
| `FOR`    | Run a specified command for each string in a set |
| `GOTO`   | Jump to a labeled line in a batch program |
| `IF`     | Perform conditional processing in batch programs |
| `PAUSE`  | Wait for a keystroke to continue |
| `REM`    | Add comments in a batch program |
| `SHIFT`  | Left-shift command-line parameters in a batch program |

</div>


## Miscellaneous commands

<div class="compact" markdown>

| Command    | Aliases    | Description |
|------------|------------|-------------|
| `CLS`      |            | Clear the DOS screen |
| `DATE`     |            | Display or change the internal date |
| `EXIT`     |            | Exit from the DOS shell |
| `HELP`     |            | Display help information for DOS commands |
| `KEYB`     |            | Configure a keyboard layout and screen font |
| `LH`       | `LOADHIGH` | Load a DOS program into upper memory |
| `LOADFIX`  |            | Load a program in a specific memory region and then run it |
| `MEM`      |            | Display the amount of used and free memory |
| `MODE`     |            | Set the display mode or the keyboard's typematic rate |
| `MOUSE`    |            | Load the built-in mouse driver |
| `PATH`     |            | Display or set a search path for executable files |
| `SET`      |            | Display or change environment variables |
| `SETVER`   |            | Display or set the DOS version reported to applications |
| `TIME`     |            | Display or change the internal time |
| `TYPE`     |            | Display the contents of a text file |
| `VER`      |            | Display the DOS version |
| `VOL`      |            | Display the disk volume and serial number, if they exist |

</div>


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
