# Output filter emulation

Most DOS-era sound cards feature an output low-pass filter (and sometimes a
high-pass filter too) that shapes the warmth and character of their output.
DOSBox Staging accurately emulates these analog output stage filters for all
supported sound devices, resulting in a more period-accurate and pleasant
listening experience.

Filters are enabled by default for small-speaker audio systems ([PC
Speaker](sound-devices/pc-speaker.md), [IBM PS/1 Audio
Card](sound-devices/ibm-ps1audio.md), [Tandy](sound-devices/tandy.md), and the
various [Covox options](sound-devices/covox-variants.md)). For hardware
devices, the filters emulate the analog output stage of the original hardware.
For small-speaker systems, they model the frequency response of a band-limited
speaker in an acoustic environment --- capturing the muffled, boxy quality
that was very much part of how these devices actually sounded.

For [Sound Blaster](sound-devices/adlib-cms-sound-blaster.md), the
[`sb_filter`](sound-devices/adlib-cms-sound-blaster.md#sb_filter) parameter
defaults to `modern`, which uses the simple linear interpolation method of
previous DOSBox versions (kept as the default for broad compatibility across
the widest range of games). Set it to `auto` to enable accurate analog output
filter emulation that matches the filters of the selected Sound Blaster model.
OPL and CMS filters are also accurately emulated.

Some devices, such as the [Gravis UltraSound](sound-devices/gravis-ultrasound.md),
[Roland MT-32](sound-devices/roland-mt-32.md),
[FluidSynth](sound-devices/general-midi.md#fluidsynth),
[Innovation SSI-2001](sound-devices/innovation.md), and the
[IBM Music Feature Card](sound-devices/imfc.md), don't have hardware output
filters to emulate. These devices typically have clean, full-range output by
design, but you can apply custom filters if you want to tailor the sound to
your preference.

The following per-device filter settings are available:

<div class="compact" markdown>

- [`cms_filter`](sound-devices/adlib-cms-sound-blaster.md#cms_filter) --- Creative Music System (Game Blaster)
- [`fsynth_filter`](sound-devices/general-midi.md#fsynth_filter) --- FluidSynth (custom only)
- [`gus_filter`](sound-devices/gravis-ultrasound.md#gus_filter) --- Gravis UltraSound (custom only)
- [`imfc_filter`](sound-devices/imfc.md#imfc_filter) --- IBM Music Feature Card (custom only)
- [`innovation_filter`](sound-devices/innovation.md#innovation_filter) --- Innovation SSI-2001 (custom only)
- [`lpt_dac_filter`](sound-devices/covox-variants.md#lpt_dac_filter) --- LPT DAC devices (Covox, Disney, Stereo-on-1)
- [`mt32_filter`](sound-devices/roland-mt-32.md#mt32_filter) --- Roland MT-32 (custom only)
- [`opl_filter`](sound-devices/adlib-cms-sound-blaster.md#opl_filter) --- OPL synthesiser
- [`pcspeaker_filter`](sound-devices/pc-speaker.md#pcspeaker_filter) --- PC Speaker
- [`ps1audio_dac_filter`](sound-devices/ibm-ps1audio.md#ps1audio_dac_filter) --- IBM PS/1 Audio DAC
- [`ps1audio_filter`](sound-devices/ibm-ps1audio.md#ps1audio_filter) --- IBM PS/1 Audio synthesiser
- [`sb_filter`](sound-devices/adlib-cms-sound-blaster.md#sb_filter) --- Sound Blaster digital audio
- [`tandy_dac_filter`](sound-devices/tandy.md#tandy_dac_filter) --- Tandy DAC
- [`tandy_filter`](sound-devices/tandy.md#tandy_filter) --- Tandy synthesiser

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
    hpf 3 120 lpf 1 6500
    ```


