# Tandy 3 Voice

The **Tandy 3 Voice** audio was introduced with the Tandy 1000 IBM PC clone
released in 1984, which was as a clone of the short-lived IBM PCjr.

Life was simple in the 80s---most games only supported the PC speaker, but
some of them had enhanced sound on the Tandy 1000 IBM PC compatible.

The enhanced sound hardware of the Tandy lineup was an integral, non-removable
part of the computer, and was not available as an add-on card for non-Tandy
machines. Therefore, most Tandy games simply check if they're running on a
Tandy, and if so, make use of its integrated sound and graphics capabilities.

To play Tandy games with Tandy sound, you need to enable the emulation of a
generic Tandy computer as follows:

``` ini
[dosbox]
machine = tandy
```

See [`machine`](../../system/general.md#machine) for more details on machine
types.

Later Tandy 1000 models added support for digital audio as well.

``` ini
[sblaster]
sbtype = none
```

!!! warning

    Due to resource conflicts, DOSBox cannot emulate the Tandy DAC and Sound Blaster digital
    audio at the same time. If you want to emulate the Tandy DAC in addition
    to the Tandy synthesiser, you need to disable Sound Blaster emulation by
    setting [`sbtype`](../sound-devices/adlib-cms-sound-blaster.md#sbtype) to
    `none` as shown above.

!!! note

    Tandy 3 Voice sound is sometimes referred to as **Tandy 1000** or
    **IBM PCjr** in the game’s sound setup program.

    Tandy digital sound is sometimes called **Tandy with DAC** or
    **Tandy 1000 SL, TL, HL series** or something similar.


## Configuration settings

Tandy 3 Voice settings are to be configured in the `[speaker]` section.


##### tandy

:   Set the Tandy/PCjr 3 Voice sound emulation.

    Possible values:

    - `auto` *default*{ .default } -- Automatically enable Tandy/PCjr sound
      for the `tandy` and `pcjr` machine types only.
    - `on` -- Enable Tandy/PCjr sound with DAC support, when possible.
      Most games also need the machine set to `tandy` or `pcjr` to work.
    - `psg` -- Only enable the card's three-voice programmable sound generator
      without DAC to avoid conflicts with other cards using DMA 1.
    - `off` -- Disable Tandy/PCjr sound.


##### tandy_dac_filter

:   Filter for the Tandy DAC output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### tandy_fadeout

:   Fade out the Tandy synth output after the last IO port write.

    Possible values:

    - `off` *default*{ .default } -- Don't fade out; residual output will
      play forever.
    - `on` -- Wait 0.5s before fading out over a 0.5s period.
    - `<custom>` -- Custom fade-out definition; see
      [opl_fadeout](adlib-cms-sound-blaster.md#opl_fadeout) for details on
      the format.


##### tandy_filter

:   Filter for the Tandy synth output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.
