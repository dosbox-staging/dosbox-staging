# Covox variants

Several inexpensive **Covox variants** appeared in the late 1980s as a cheap
way to get digital audio output from a PC. These small digital-to-analog
converter (DAC) devices were far simpler than a [Sound
Blaster](adlib-cms-sound-blaster.md) — just a dongle that plugged into the
parallel port, also known as the LPT (Line Print Terminal) port — but they
could play back digitised sound and speech at decent quality for the era. For
this reason, they are also sometimes called LPT DAC devices.


## Covox Speech Thing

Released in 1986, the **Covox Speech Thing** (or just **Covox**) was a simple,
cheap DAC that plugged into the parallel port --- popular enough that it was
often built as a DIY project. It provided 8-bit mono audio output and was
supported by around 60 games, including several Sierra titles.

## Disney Sound Source

Introduced in 1990, the **Disney Sound Source** was a parallel port audio
device backwards-compatible with the [Covox Speech Thing](#covox-speech-thing),
but with lower CPU overhead during playback. It was supported by around 150
games, making it the most popular parallel port audio device. DOSBox Staging
emulates the Disney Sound Source, which is also compatible with Covox Speech
Thing titles and Intersound MDO software.

## Stereo-on-1 DAC

The **Stereo-on-1** was a stereo variant of the parallel port DAC concept,
splitting the signal into left and right channels at the cost of halving the
bit depth. Less common than the mono [Covox](#covox-speech-thing), but
supported by some tracker music players and a handful of games.


## Mixer channels

The Covox Speech Thing outputs to the **COVOX** [mixer
channel](../mixer.md#list-of-mixer-channels), the Disney Sound Source to the
**DISNEY** channel, and the Stereo-on-1 to the **STON1** channel.


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

    - `on` *default*{ .default } -- Filter the output. The default filter
      varies by device:

        | Device | Default filter | Equivalent setting |
        | --- | --- | --- |
        | Disney Sound Source | 2nd order high-pass at 100 Hz, and 2nd order low-pass at 2 kHz | `hpf 2 100 lpf 2 2000` |
        | Covox Speech Thing | 2nd order low-pass at 9 kHz | `lpf 2 9000` |
        | Stereo-on-1 | 2nd order low-pass at 9 kHz | `lpf 2 9000` |

    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.
