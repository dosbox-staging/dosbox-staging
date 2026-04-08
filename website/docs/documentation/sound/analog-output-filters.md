# Analog output filter emulation

## Overview

Most DOS-era sound cards feature an output low-pass filter (and sometimes a
high-pass filter too) that plays an important role in giving their sound its
character. DOSBox Staging accurately emulates these analog output stage filters
for all supported sound devices, resulting in a more period-accurate and
pleasant listening experience.

Filters are enabled by default for small-speaker audio systems (PC Speaker,
PS/1 Audio, Tandy, and the various LPT DAC options). Depending on the device,
these filters either emulate the analog output filters of the original
hardware, or the sound coming out of a small band-limited speaker in an
acoustic environment.

For [Sound Blaster](sound-devices/adlib-cms-sound-blaster.md), the [`sb_filter`](sound-devices/adlib-cms-sound-blaster.md#sb_filter) parameter defaults to `modern`, which uses
the simple linear interpolation method of previous DOSBox versions. Set it to
`auto` to enable accurate analog output filter emulation that matches the
filters of the selected Sound Blaster model. OPL and CMS filters are also
accurately emulated.

Some devices, such as the [Gravis UltraSound](sound-devices/gravis-ultrasound.md), [Roland MT-32](sound-devices/roland-mt-32.md), [FluidSynth](sound-devices/general-midi.md#fluidsynth),
[Innovation SSI-2001](sound-devices/innovation.md), and the [IBM Music Feature Card](sound-devices/imfc.md), don't have hardware
output filters to emulate, but you can apply custom filters to them if
desired.

The following per-device filter settings are available:

<div class="compact" markdown>

- [`sb_filter`](sound-devices/adlib-cms-sound-blaster.md#sb_filter) --- Sound Blaster digital audio
- [`opl_filter`](sound-devices/adlib-cms-sound-blaster.md#opl_filter) --- OPL synthesiser
- [`cms_filter`](sound-devices/adlib-cms-sound-blaster.md#cms_filter) --- Creative Music System (Game Blaster)
- [`pcspeaker_filter`](sound-devices/pc-speaker.md#pcspeaker_filter) --- PC Speaker
- [`tandy_filter`](sound-devices/tandy.md#tandy_filter) --- Tandy synthesiser
- [`tandy_dac_filter`](sound-devices/tandy.md#tandy_dac_filter) --- Tandy DAC
- [`ps1audio_filter`](sound-devices/ibm-ps1audio.md#ps1audio_filter) --- IBM PS/1 Audio synthesiser
- [`ps1audio_dac_filter`](sound-devices/ibm-ps1audio.md#ps1audio_dac_filter) --- IBM PS/1 Audio DAC
- [`lpt_dac_filter`](sound-devices/covox-variants.md#lpt_dac_filter) --- LPT DAC devices (Covox, Disney, Stereo-on-1)
- [`gus_filter`](sound-devices/gravis-ultrasound.md#gus_filter) --- Gravis UltraSound (custom only)
- [`mt32_filter`](sound-devices/roland-mt-32.md#mt32_filter) --- Roland MT-32 (custom only)
- [`fsynth_filter`](sound-devices/general-midi.md#fsynth_filter) --- FluidSynth (custom only)
- [`innovation_filter`](sound-devices/innovation.md#innovation_filter) --- Innovation SSI-2001 (custom only)
- [`imfc_filter`](sound-devices/imfc.md#imfc_filter) --- IBM Music Feature Card (custom only)

</div>


## Custom filter settings

One or two custom filters in the following format:

```
TYPE ORDER FREQ
```

Where **TYPE** can be `hpf` (high-pass) or `lpf` (low-pass), **ORDER** is the
order of the filter from 1 to 16 (1st order = 6dB/octave slope, 2nd order =
12dB/octave, etc.), and **FREQ** is the cutoff frequency in Hz.

Examples:

- 2nd order (12dB/octave) low-pass filter at 12kHz:

    ```
    lpf 2 12000
    ```

- 3rd order (18dB/octave) high-pass filter at 120Hz, and 1st order
  (6dB/octave) low-pass filter at 6.5kHz:

    ```
    hpf 3 120 lfp 1 6500
    ```


