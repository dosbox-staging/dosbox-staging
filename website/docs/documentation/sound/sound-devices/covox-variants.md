# Covox variants

## Overview 

Several inexpensive parallel port DAC devices appeared in the late 1980s as
a cheap way to get digital audio output from a PC. They were far simpler
than a Sound Blaster — just a small dongle that plugged into your printer
port — but they could play back digitised sound and speech at decent quality
for the era.

### Covox Speech Thing

The Covox Speech Thing (1986) was a simple R-2R resistor ladder DAC that
plugged into the parallel port. It was cheap to build (even as a DIY
project) and provided 8-bit mono audio output. A number of games supported
it, including several Sierra titles and [*Leisure Suit Larry 1 VGA*](https://www.mobygames.com/game/413/leisure-suit-larry-1-in-the-land-of-the-lounge-lizards/).


### Disney Sound Source

The Disney Sound Source (1990) was a parallel port audio device
backwards-compatible with the Covox Speech Thing, but it added a small FIFO
buffer and control logic to reduce CPU overhead during playback. DOSBox
Staging emulates the Disney Sound Source, which is also compatible with
Covox Speech Thing titles and Intersound MDO software.


### Stereo-on-1 DAC

The Stereo-on-1 was a stereo variant of the parallel port DAC concept. It
split the parallel port's 8 data lines into two 4-bit channels — one for
left, one for right — giving you stereo output at the cost of halving the
bit depth. Less common than the mono Covox, but supported by some tracker
players and a handful of games.

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
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.
