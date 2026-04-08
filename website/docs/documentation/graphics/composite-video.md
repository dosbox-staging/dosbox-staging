# Composite video


## Configuration settings

You can set the CGA composite video parameters in the `[composite]`
configuration section. CGA composite monitor emulation is only available for
`cga`, `pcjr`, and `tandy` machine types.


##### composite

:   Enable CGA composite monitor emulation. This allows the emulation of
    NTSC artifact colours from the raw CGA RGBI image data, just like on a
    real NTSC CGA composite monitor.

    Possible values:

    - `auto` *default*{ .default } -- Automatically enable composite
      emulation for the 640x400 composite mode if the game uses it. You need
      to enable composite mode manually for the 320x200 mode.
    - `on` -- Enable composite emulation in all video modes.
    - `off` -- Disable composite emulation.

    !!! note

        Fine-tune the composite emulation settings (e.g.,
        [hue](#hue)) using the composite hotkeys, then
        copy the new settings from the logs into your config.


##### convergence

:   Set the sharpness of the CGA composite image (`0` by default). Valid
    range is -50 to 50.


##### era

:   Era of CGA composite monitor to emulate.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- PCjr uses `new`, and CGA/Tandy use
      `old`.
    - `old` -- Emulate an early NTSC IBM CGA composite monitor model.
    - `new` -- Emulate a late NTSC IBM CGA composite monitor model.

    </div>


##### hue

:   Set the hue of the CGA composite colours (`0` by default). Valid range
    is -360 to 360. For example, adjust until the sky appears blue and the
    grass green in the game. This emulates the tint knob of CGA composite
    monitors which often had to be adjusted for each game.
