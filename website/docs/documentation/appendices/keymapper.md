# Keymapper reference

The table below lists every mappable action in DOSBox Staging. Actions with
no default binding can be assigned to any key or button combination through
the key mapper.

| Event name     | Description                      | Default (Win/Linux) | Default (macOS)   |
| ---            | ---                              | ---                 | ---               |
| `mapper`       | Open key mapper                  | ++ctrl+f1++         | ++cmd+f1++        |
| `shutdown`     | Exit DOSBox                      | ++ctrl+f9++         | ++cmd+f9++        |
| `fullscr`      | Toggle fullscreen                | ++alt+enter++       | ++alt+enter++     |
| `restart`      | Restart DOSBox                   | ++ctrl+alt+home++   | ++cmd+ctrl+home++ |
| `pause`        | Pause emulation                  | ++alt+pause++       | ++cmd+p++         |
| `capmouse`     | Toggle mouse capture             | ++ctrl+f10++        | ++cmd+f10++       |
| `mute`         | Mute audio                       | ++ctrl+f8++         | ++cmd+f8++        |
| `speedlock`    | Toggle turbo speed               | ++alt+f12++         | ++alt+f12++       |
| `cycledown`    | Decrease CPU cycles              | ++ctrl+f11++        | ++cmd+f11++       |
| `cycleup`      | Increase CPU cycles              | ++ctrl+f12++        | ++cmd+f12++       |
| `reloadshader` | Reload current shader            | ++ctrl+f2++         | ++cmd+f2++        |
| `stretchax`    | Toggle stretch axis              | *(none)*            | *(none)*          |
| `incstretch`   | Increase viewport stretch        | *(none)*            | *(none)*          |
| `decstretch`   | Decrease viewport stretch        | *(none)*            | *(none)*          |
| `previmageadj` | Select previous image adjustment | *(none)*            | *(none)*          |
| `nextimageadj` | Select next image adjustment     | *(none)*            | *(none)*          |
| `decimageadj`  | Decrease image adjustment        | *(none)*            | *(none)*          |
| `incimageadj`  | Increase image adjustment        | *(none)*            | *(none)*          |
| `screenshot`   | Screenshot (default format)      | ++ctrl+f5++         | ++cmd+f5++        |
| `rawshot`      | Raw screenshot                   | *(none)*            | *(none)*          |
| `upscshot`     | Upscaled screenshot              | *(none)*            | *(none)*          |
| `rendshot`     | Rendered screenshot              | ++alt+f5++          | ++alt+f5++        |
| `recwave`      | Record audio                     | ++ctrl+f6++         | ++cmd+f6++        |
| `caprawmidi`   | Record MIDI                      | ++ctrl+alt+f6++     | ++cmd+alt+f6++    |
| `video`        | Record video                     | ++ctrl+f7++         | ++cmd+f7++        |
| `caprawopl`    | Record OPL output                | *(none)*            | *(none)*          |
| `swap`         | Swap disk image                  | ++ctrl+f4++         | ++cmd+f4++        |
| `hercpal`      | Cycle Hercules palette           | ++f11++             | ++f11++           |
| `monocgapal`   | Cycle mono CGA palette           | ++f11++             | ++f11++           |
| `comp_sel`     | Select composite knob            | ++f10++             | ++f10++           |
| `comp_inc`     | Increase composite control       | ++f11++             | ++f11++           |
| `comp_dec`     | Decrease composite control       | ++alt+f11++         | ++alt+f11++       |
| `cgacomp`      | Toggle CGA composite mode        | ++f12++             | ++f12++           |
| `debugger`     | Enable debugger                  | ++alt+pause++       | ++alt+pause++     |
| `jxsl`         | Joystick X scalar left           | ++ctrl+left++       | ++cmd+left++      |
| `jxsr`         | Joystick X scalar right          | ++ctrl+right++      | ++cmd+right++     |
| `jxol`         | Joystick X offset left           | ++alt+left++        | ++alt+left++      |
| `jxor`         | Joystick X offset right          | ++alt+right++       | ++alt+right++     |
| `jxrs`         | Joystick X reset                 | ++ctrl+delete++     | ++cmd+delete++    |
| `jysd`         | Joystick Y scalar down           | ++ctrl+down++       | ++cmd+down++      |
| `jysu`         | Joystick Y scalar up             | ++ctrl+up++         | ++cmd+up++        |
| `jyod`         | Joystick Y offset down           | ++alt+down++        | ++alt+down++      |
| `jyou`         | Joystick Y offset up             | ++alt+up++          | ++alt+up++        |
| `jyrs`         | Joystick Y reset                 | ++ctrl+home++       | ++cmd+home++      |

!!! note

    Video adapter-specific actions (`hercpal`, `monocgapal`, `comp_sel`,
    `comp_inc`, `comp_dec`, `cgacomp`) are only active when the
    corresponding machine type is emulated.

    Joystick calibration actions are only active when
    `use_joy_calibration_hotkeys` is enabled.
