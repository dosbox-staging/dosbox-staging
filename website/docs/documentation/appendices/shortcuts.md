# Keyboard shortcuts

DOSBox Staging has two layers of key bindings: **shortcuts** that control
the emulator itself (fullscreen toggle, screenshots, CPU speed, etc.) and
**DOS key mappings** that pass keyboard input through to the emulated DOS
environment.

All bindings can be customised in the key mapper, accessible via
++ctrl+f1++ on Windows/Linux or ++cmd+f1++ on macOS. Shortcuts use a
"primary modifier" key: ++ctrl++ on Windows and Linux, ++cmd++ on macOS.


## Default shortcuts

### General

| Windows / Linux   | macOS             | Action             |
| ---               | ---               | ---                |
| ++ctrl+f1++       | ++cmd+f1++        | Open key mapper    |
| ++alt+enter++     | ++alt+enter++     | Toggle fullscreen  |
| ++alt+pause++     | ++cmd+p++         | Pause emulation    |
| ++ctrl+f9++       | ++cmd+f9++        | Shut down DOSBox   |
| ++ctrl+alt+home++ | ++cmd+ctrl+home++ | Restart DOSBox     |
| ++alt+f12++       | ++alt+f12++       | Toggle turbo speed |


### CPU

| Windows / Linux | macOS       | Action              |
| ---             | ---         | ---                 |
| ++ctrl+f11++    | ++cmd+f11++ | Decrease CPU cycles |
| ++ctrl+f12++    | ++cmd+f12++ | Increase CPU cycles |


### Audio

| Windows / Linux | macOS      | Action     |
| ---             | ---        | ---        |
| ++ctrl+f8++     | ++cmd+f8++ | Mute audio |


### Capture

| Windows / Linux | macOS          | Action                           |
| ---             | ---            | ---                              |
| ++ctrl+f5++     | ++cmd+f5++     | Take screenshot (default format) |
| ++alt+f5++      | ++alt+f5++     | Take rendered screenshot         |
| ++ctrl+f6++     | ++cmd+f6++     | Start/stop audio recording       |
| ++ctrl+alt+f6++ | ++cmd+alt+f6++ | Start/stop MIDI recording        |
| ++ctrl+f7++     | ++cmd+f7++     | Start/stop video recording       |


### Display

| Windows / Linux | macOS      | Action                |
| ---             | ---        | ---                   |
| ++ctrl+f2++     | ++cmd+f2++ | Reload current shader |


### Input

| Windows / Linux | macOS       | Action                  |
| ---             | ---         | ---                     |
| ++ctrl+f10++    | ++cmd+f10++ | Toggle mouse capture    |
| ++ctrl+f4++     | ++cmd+f4++  | Swap mounted disk image |


### Joystick calibration

These shortcuts are only active when `use_joy_calibration_hotkeys` is
enabled in the [joystick settings](../input/joystick.md).

| Windows / Linux | macOS          | Action              |
| ---             | ---            | ---                 |
| ++ctrl+left++   | ++cmd+left++   | X scalar left       |
| ++ctrl+right++  | ++cmd+right++  | X scalar right      |
| ++alt+left++    | ++alt+left++   | X offset left       |
| ++alt+right++   | ++alt+right++  | X offset right      |
| ++ctrl+delete++ | ++cmd+delete++ | Reset X calibration |
| ++ctrl+up++     | ++cmd+up++     | Y scalar up         |
| ++ctrl+down++   | ++cmd+down++   | Y scalar down       |
| ++alt+up++      | ++alt+up++     | Y offset up         |
| ++alt+down++    | ++alt+down++   | Y offset down       |
| ++ctrl+home++   | ++cmd+home++   | Reset Y calibration |


### Video adapter-specific

These shortcuts are only available when emulating specific machine types.

| Shortcut    | Machine type | Action                           |
| ---         | ---          | ---                              |
| ++f11++     | Hercules     | Cycle Hercules palette           |
| ++f11++     | CGA (mono)   | Cycle mono CGA palette           |
| ++f12++     | CGA (colour) | Toggle CGA composite mode        |
| ++f10++     | CGA / PCjr   | Select composite adjustment knob |
| ++f11++     | CGA / PCjr   | Increase composite control       |
| ++alt+f11++ | CGA / PCjr   | Decrease composite control       |

