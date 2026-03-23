# PC speaker

The PC Speaker was the integrated speaker from the earliest IBM PC’s. It was
really only meant for simple beeps, but ended up widely used by PC games as it
was the lowest common-denominator for PC sound, despite its very limited
capabilities.

It is effectively a single-channel square-wave generator, and with
considerable CPU overhead it was possible to get some half-decent sound out of
it as demonstrated by RealSound.

Sound quality was heavily dependent on the type of speaker and the placement
and PC case it was built into. Originally a dynamic speaker was used, but
later model PCs used a tiny piezo speaker instead which further reduced sound
quality and volume.

PC Speaker emulation is enabled by default, but can be disabled by setting
pcspeaker=false in the [speaker] section of the DOSBox-X config file.


With clever programming, the PC speaker could act as a crude synth, or even
play digital audio, enabling speech in games like the original Castle
Wolfenstein.

RealSound

spellcasting 101


Windwalker


## Configuration settings

PC speaker settings are to be configured in the `[speaker]` section.


##### pcspeaker

:   PC speaker emulation model.

    Possible values:

    - `impulse` *default*{ .default } -- A very faithful emulation of the PC
      speaker's output. Works with most games, but may result in garbled sound
      or silence in a small number of programs.
    - `discrete` -- Legacy simplified PC speaker emulation; only use this on
      specific titles that give you problems with the `impulse` model.
    - `none`/`off` -- Don't emulate the PC speaker.


##### pcspeaker_filter

:   Filter for the PC speaker output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../../analog-output-filters/#custom-filter-settings)
      for details.
