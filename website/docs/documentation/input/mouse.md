# Mouse

DOSBox Staging includes a built-in mouse driver which is enabled by default,
so you don't need to load a real DOS mouse driver manually (e.g., `MOUSE.COM`
or `CTMOUSE`). The vast majority of DOS games that require a mouse will work
out-of-the-box --- you'll only need to tinker with the mouse settings for a
small handful of problematic games, or if you have very specific niche needs.
The built-in driver provides lower latency and smoother movement than a real
DOS mouse driver and uses less memory, so it's the best option for most
people.

To capture the mouse, click inside the DOSBox window. By default, the DOSBox
Staging window's titlebar indicates if the mouse is captured. To release the
mouse, press the middle mouse button or ++ctrl+f10++ / ++cmd+f10++ (see
[Keyboard shortcuts](../appendices/shortcuts.md)). If your mouse has no middle
button, the keyboard shortcut is your friend. 

The [`mouse_raw_input`](#mouse_raw_input) setting bypasses your host OS's
mouse acceleration, giving you consistent sensitivity regardless of your
desktop settings. This is particularly useful for games where precise mouse
control matters (e.g., fast-paced FPS games).

Many DOS games differ wildly in how they handle mouse movement. In many games,
horizontal and vertical mouse movement has different sensitivity. You can
"normalise" the X and Y axis sensitivity or adjust the global sensitivity with
the [`mouse_sensitivity`](#mouse_sensitivity) setting.

## DOS-era mice

The standard PC mouse throughout the DOS era had two buttons and no scroll
wheel. Scroll wheels did not exist on consumer mice until Microsoft
introduced the IntelliMouse in 1996, well into the twilight years of DOS
gaming. The vast majority of DOS software was written for --- and tested with
--- a simple two-button mouse.

DOSBox Staging emulates a two-button mouse by default
([`builtin_dos_mouse_driver_model`](#builtin_dos_mouse_driver_model) =
`2button`), and this is the safest setting. Only a small number of games
from the 90s support a third mouse button, and some games will misbehave if
an unexpected middle button press is detected. If you know a game supports
three buttons, enable it explicitly by setting the model to `3button`. The
`wheel` option exists for the rare DOS application or Windows 3.1 setup that
can use a scroll wheel, but no DOS game is known to use it.


## Serial mice

DOSBox Staging can emulate several serial (COM port) mouse types:

- **Microsoft mouse** --- 2 buttons, the most widely compatible type.
- **Logitech mouse** --- 3 buttons, mostly compatible with Microsoft mouse.
- **Wheel mouse** --- 3 buttons + wheel, mostly compatible with Microsoft mouse.
- **Mouse Systems mouse** --- 3 buttons, an older protocol NOT compatible with
  Microsoft mice. A few programs require this type.

To enable a serial mouse, use the `SERIAL 1 mouse` command (this attaches it
to the COM1 port), or edit the [Serial ports](../networking/serial-ports.md)
configuration.
By default, DOSBox Staging tries to determine what type of mouse the program
expects. See [com_mouse_model](#com_mouse_model) for details.


## Dual mouse gaming

Multiple physical mice can be mapped to specific emulated mouse interfaces,
enabling two-player split-screen gaming using two USB mice on a single
computer (e.g., in [The Settlers](https://www.mobygames.com/game/425/serf-city-life-is-feudal/) and [The Settlers II](https://www.mobygames.com/game/598/the-settlers-ii-veni-vidi-vici/)). Use the
`MOUSECTL` command on the DOS command line to set the per-interface mouse
configurations.

See the [Dual mouse gaming](https://github.com/dosbox-staging/dosbox-staging/wiki/Dual-Mouse-Gaming) wiki page for setup details.


## Seamless mode

For Windows 3.1 running inside DOSBox, seamless mouse integration is
available when using a compatible mouse driver --- with this enabled, the
mouse cursor can be moved freely between the host desktop and the DOSBox
Staging window (see [`mouse_capture`](#mouse_capture)).



## Configuration settings

You can set the mouse parameters in the `[mouse]` configuration section.


##### builtin_dos_mouse_driver

:   Built-in DOS mouse driver mode. It bypasses the PS/2 and serial (COM)
    ports and communicates with the mouse directly. This results in lower
    input lag, smoother movement, and increased mouse responsiveness, so only
    disable it and load a real DOS mouse driver if it's really necessary
    (e.g., if a game is not compatible with the built-in driver).

    Possible values:

    - `on` *default*{ .default } -- Simulate a mouse driver TSR program
      loaded from `AUTOEXEC.BAT`. This is the most compatible way to emulate
      the DOS mouse driver, but if it doesn't work with your game, try the
      `no-tsr` setting.
    - `no-tsr` -- Enable the mouse driver without simulating the TSR
      program. Let us know if it fixes any software not working with the `on`
      setting.
    - `off` -- Disable the built-in mouse driver. You can still start it at
      runtime by executing the bundled `MOUSE.COM` from drive Z.

    !!! note

        - The [ps2_mouse_model](#ps2_mouse_model) and
          [com_mouse_model](#com_mouse_model) settings have no effect on the
          built-in driver.

        - The driver is auto-disabled if you boot into real MS-DOS or Windows
          9x under DOSBox. Under Windows 3.1, the driver is not disabled,
          but the Windows 3.1 mouse driver takes over.

        - To use a real DOS mouse driver (e.g., `MOUSE.COM` or
          `CTMOUSE.EXE`), configure the mouse type with
          [ps2_mouse_model](#ps2_mouse_model) or
          [com_mouse_model](#com_mouse_model), then load the driver.


##### builtin_dos_mouse_driver_model

:   Set the mouse model to be simulated by the built-in DOS mouse driver.

    Possible values:

    <div class="compact" markdown>

    - `2button` *default*{ .default } -- 2 buttons, the safest option for
      most games. The majority of DOS games only support 2 buttons, some
      might misbehave if the middle button is pressed.
    - `3button` -- 3 buttons, only supported by very few DOS games. Only use
      this if the game is known to support a 3-button mouse.
    - `wheel` -- 3 buttons + wheel, supports the CuteMouse WheelAPI version
      1.0. No DOS game uses the mouse wheel, only a handful of DOS
      applications and Windows 3.1 with special third-party drivers.

    </div>


##### builtin_dos_mouse_driver_move_threshold

:   The smallest amount of mouse movement that will be reported to the guest
    (`1` by default). Some DOS games cannot properly respond to small
    movements, which were hard to achieve using the imprecise ball mice of the
    era; in such case increase the amount to the smallest value that results
    in a proper cursor motion.

    Possible values:

    - `1-9` -- The smallest amount of movement to report, for both
      horizontal and vertical axes. `1` reports all the movements (default).
    - `x,y` -- Separate values for horizontal and vertical axes, can be
      separated by spaces, commas, or semicolons.

    !!! note

        Known games requiring the threshold to be set to `2`:

        - [Ultima Underworld: The Stygian Abyss](https://www.mobygames.com/game/690/ultima-underworld-the-stygian-abyss/)
        - [Ultima Underworld II: Labyrinth of Worlds](https://www.mobygames.com/game/691/ultima-underworld-ii-labyrinth-of-worlds/)


##### builtin_dos_mouse_driver_options

:   Additional built-in DOS mouse driver settings as a list of space or comma
    separated options (unset by default).

    Possible values:

    <div class="compact" markdown>

    - `immediate` -- Update mouse movement counters immediately, without
      waiting for interrupt. May improve mouse latency in fast-paced games
      (arcade, FPS, etc.), but might cause issues in some titles.
    - `modern` -- Emulate v7.0+ Microsoft mouse driver behaviour (instead of
      v6.0 and earlier). Only `Descent II` with the official Voodoo patch has
      been found to require this so far.
    - `no-granularity` -- Disable mouse position granularity. Only enable if
      the game needs it. Only `Joan of Arc: Siege & the Sword` in Hercules
      mode has been found to require this so far.

    </div>


##### com_mouse_model

:   Set the default COM (serial) mouse model, or in other words, the type of
    the virtual mouse plugged into the emulated COM ports. The setting has no
    effect on the built-in mouse driver (see
    [builtin_dos_mouse_driver](#builtin_dos_mouse_driver)).

    Possible values:

    <div class="compact" markdown>

    - `2button` -- 2 buttons, Microsoft mouse.
    - `3button` -- 3 buttons, Logitech mouse; mostly compatible with
      Microsoft mouse.
    - `wheel` -- 3 buttons + wheel; mostly compatible with Microsoft mouse.
    - `msm` -- 3 buttons, Mouse Systems mouse; NOT compatible with Microsoft
      mouse.
    - `2button+msm` -- Automatic choice between `2button` and `msm`.
    - `3button+msm` -- Automatic choice between `3button` and `msm`.
    - `wheel+msm` *default*{ .default } -- Automatic choice between `wheel`
      and `msm`.

    </div>

    !!! note

        Enable COM port mice in the `[serial]` section.


##### mouse_capture

:   Set the mouse capture behaviour.

    Possible values:

    - `onclick` *default*{ .default } -- Capture the mouse when clicking any
      mouse button in the window.
    - `onstart` -- Capture the mouse immediately on start. Might not work
      correctly on some host operating systems.
    - `seamless` -- Let the mouse move seamlessly between the DOSBox window
      and the rest of the desktop; captures only with middle-click or hotkey.
      Seamless mouse does not work correctly with all the games. Windows 3.1
      can be made compatible with a custom mouse driver.
    - `nomouse` -- Hide the mouse and don't send mouse input to the game.

    !!! note

        Use `seamless` mode for touch screens.


##### mouse_middle_release

:   Release the captured mouse by middle-clicking, and also capture it in
    seamless mode.

    Possible values: `on` *default*{ .default }, `off`


##### mouse_multi_display_aware

:   Allow seamless mouse behavior and mouse pointer release to work in
    fullscreen mode on systems with more than one display.

    Possible values: `on` *default*{ .default }, `off`

    !!! note

        You should disable this if it incorrectly detects multiple displays
        when only one should actually be used. This might happen if you are
        using mirrored display mode or using an AV receiver's HDMI input for
        audio-only listening.


##### mouse_raw_input

:   Bypass the mouse acceleration and sensitivity settings of the host
    operating system. Works in fullscreen or when the mouse is captured in
    windowed mode.

    Possible values: `on` *default*{ .default }, `off`


##### mouse_sensitivity

:   Set global mouse sensitivity (`100` by default).

    Possible values:

    - `<value>` -- Set sensitivity for both axes as a percentage (e.g.,
      `150`).
    - `X,Y` -- Set X and Y axis sensitivity separately as percentages (e.g.,
      `100,150`). The two values can be separated by a space or a semicolon
      as well.

    !!! note

        - Negative values invert an axis, zero disables it.

        - Sensitivity can be fine-tuned further per mouse interface with the
          internal `MOUSECTL.COM` command.


##### ps2_mouse_model

:   Set the PS/2 AUX port mouse model, or in other words, the type of the
    virtual mouse plugged into the emulated PS/2 mouse port. The setting has
    no effect on the built-in mouse driver (see
    [builtin_dos_mouse_driver](#builtin_dos_mouse_driver)).

    Possible values:

    <div class="compact" markdown>

    - `standard` -- 3 buttons, standard PS/2 mouse.
    - `intellimouse` -- 3 buttons + wheel, Microsoft IntelliMouse.
    - `explorer` *default*{ .default } -- 5 buttons + wheel, Microsoft
      IntelliMouse Explorer.
    - `none` -- No PS/2 mouse.

    </div>


##### virtualbox_mouse

:   VirtualBox mouse interface. Fully compatible only with 3rd party Windows
    3.1 driver.

    Possible values: `on` *default*{ .default }, `off`

    !!! note

        Requires PS/2 mouse to be enabled.


##### vmware_mouse

:   VMware mouse interface. Fully compatible only with 3rd party Windows 3.1
    driver.

    Possible values: `on` *default*{ .default }, `off`

    !!! note

        Requires PS/2 mouse to be enabled.
