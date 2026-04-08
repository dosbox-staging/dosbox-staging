# Covox variants



## Covox Speech Thing

## Disney Sound Source

Covox introduced the Covox Speech Thing parallel port sound dongle in 1987,
while Disney introduced the Disney Sound Source in 1990, which is backwards
compatible with the Covox, but adds a FIFO buffer and logic to reduce CPU
overhead.

These parallel port devices only provide a DAC at a low frequency for playback
of digital samples, and has no FM sound.

DOSBox-X provides emulation for the Disney Sound Source, which is effectively
backward compatible with the earlier Covox Speech Thing. Also games compatible
with the Intersound MDO should work with this, as it is effectively a Covox
Speech Thing clone.

## Stereo-on-1 DAC


## Configuration settings

Parallel port DAC settings are to be configured in the `[speaker]` section.


##### lpt_dac

:   Type of DAC plugged into the parallel port.

    Possible values:

    <div class="compact" markdown>

    - `none`/`off` *default*{ .default } -- Don't use a parallel port DAC.
    - `disney` -- Disney Sound Source.
    - `covox` -- Covox Speech Thing.
    - `ston1` -- Stereo-on-1 DAC, in stereo up to 30 kHz.

    </div>


##### lpt_dac_filter

:   Filter for the LPT DAC audio device(s).

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../../analog-output-filters/#custom-filter-settings)
      for details.
