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

TODO(CL) insert links for the settings names

For Sound Blaster, the `sb_filter` parameter defaults to `modern`, which uses
the simple linear interpolation method of previous DOSBox versions. Set it to
`auto` to enable accurate analog output filter emulation that matches the
filters of the selected Sound Blaster model. OPL and CMS filters are also
accurately emulated.

TODO(CL) link to the sound device manual pages

Some devices, such as the Gravis UltraSound, Roland MT-32, FluidSynth,
Innovation SSI-2001, and the IBM Music Feature Card, don't have hardware
output filters to emulate, but you can apply custom filters to them if
desired.

The following per-device filter settings are available:

<div class="compact" markdown>

TODO(CL) insert links to these settings

- `sb_filter` --- Sound Blaster digital audio
- `opl_filter` --- OPL synthesiser
- `cms_filter` --- Creative Music System (Game Blaster)
- `pcspeaker_filter` --- PC Speaker
- `tandy_filter` --- Tandy synthesiser
- `tandy_dac_filter` --- Tandy DAC
- `ps1audio_filter` --- IBM PS/1 Audio synthesiser
- `ps1audio_dac_filter` --- IBM PS/1 Audio DAC
- `lpt_dac_filter` --- LPT DAC devices (Covox, Disney, Stereo-on-1)
- `gus_filter` --- Gravis UltraSound (custom only)
- `mt32_filter` --- Roland MT-32 (custom only)
- `fsynth_filter` --- FluidSynth (custom only)
- `innovation_filter` --- Innovation SSI-2001 (custom only)
- `imfc_filter` --- IBM Music Feature Card (custom only)

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


