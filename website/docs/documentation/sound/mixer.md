# DOSBox mixer

The DOSBox mixer combines the output of all emulated sound devices into a
final stereo signal. Each device gets its own mixer channel, and you can
adjust volume, stereo panning, reverb, chorus, and crossfeed levels
independently per channel.

The mixer uses a 32-bit floating-point processing path internally, so
individual channels cannot be overloaded into clipping by high per-channel
volume settings. A fixed 20 Hz high-pass filter on the master output removes
DC offset and subsonic rumble. An auto-levelling [compressor](#compressor) on
the master channel prevents clipping of the final output.

The default master volume is 50% (-6 dB) to avoid audible distortion in games
with loud output. You can raise it per game as needed (e.g., `MIXER MASTER
100`), but it's better to boost only the channels that are too quiet (e.g.,
`MIXER SB 200`).

The mixer runs in its own dedicated thread, which greatly reduces and often
completely eliminates audio stuttering and glitches. This is especially
noticeable with the Roland MT-32, FluidSynth, OPL synth, and Red Book CD
Audio.


## The MIXER command

To manage all these mixer channels, DOSBox provides the handy `MIXER` command
that you can use from the DOS command line. If you run `MIXER` without any
arguments, it prints out the current state of the mixer. This is what we get
with the default configuration:

TODO insert image

As we can see, we have four channels enabled by default:

- `MASTER` -- This is the final summed stereo output of all other
    mixer channels. The `MASTER` channel is always enabled, always comes first,
    and is always stereo.

- `OPL` -- Stereo output of the Yamaha OPL3 synthesiser on the emulated
    Sound Blaster 16 card.
- `PCSPEAKER` -- Mono output of the ubiquitous small loudspeaker built into most old IBM PC compatibles.
- `SB` -- Stereo digital audio output of the emulated Sound Blaster 16 card.

There is an important thing to note here: some emulated sound devices contain
multiple sub-devices, each having their own mixer channel. The Sound Blaster
16 is such a device; its digital output is routed to the `SB` channel, while
the output of the onboard Yamaha OPL3 synthesiser chip has its dedicated `OPL`
channel.

This is actually similar to how the Sound Blaster software mixer works on later Sound
Blaster cards. The `MIXER` command extends this concept so you can adjust the
parameters of all emulated audio devices via a unified interface.

If you can't remember all this, don't worry --- just run `MIXER /?` to display
a handy little summary all available options.

## Changing channel settings

### Volume

Set the volume of a channel as a percentage:

    MIXER OPL 150

This sets the `OPL` channel to 150% volume. You can set the left and right
channels independently by separating the values with a colon:

    MIXER OPL 200:100

### Reverb and chorus

Set the reverb and chorus levels of a channel with `R` and `C` followed by
the level:

    MIXER OPL R50 C30

This sets the `OPL` channel's reverb level to 50 and chorus level to 30.
For details on the available presets and what reverb and chorus do, see
[Mixer effects](mixer-effects.md).

### Crossfeed

Set the crossfeed strength of a channel with `X` followed by the level:

    MIXER OPL X50

For details on what crossfeed does and the available presets, see
[Mixer effects --- Crossfeed](mixer-effects.md#crossfeed).

### Advanced usage

You may change the settings of more than one channel in a single command.
For example, the command `MIXER x30 opl 150 r50 c30 sb x10 reverse` accomplishes the
following:

- Set the global crossfeed to 30 (`x30`)
- Set the volume of the `OPL` channel to 150%, the reverb level to 50, and the
    chorus level to 30 (`fm 150 r50 c30`)
- Set the crossfeed of the `SB` channel to 10 and use reverse stereo (`sb x10
    reverse`)

## List of mixer channels

<div class="compact" markdown>

The below table lists all possible mixer channels with links to the respective
sections of the manual where they're described in detail.

| Channel name    | Device                                       | Stereo? |
| --------------- | -------------------------------------------- | --------|
| **CDAUDIO**     | [CD-DA digital audio](sound-devices/cd-da.md)              | Yes
| **CMS**         | [Creative Music System (C/MS) synthesiser](sound-devices/adlib-cms-sound-blaster.md#cms) | Yes
| **COVOX**       | [Covox Speech Thing digital audio](sound-devices/covox-variants.md#covox-speech-thing) | No
| **DISNEY**      | [Disney Sound Source (DSS) digital audio](sound-devices/covox-variants.md#disney-sound-source) | No
| **DISKNOISE**   | [Disk noise emulation](disk-noise.md) | No
| **FSYNTH**      | [FluidSynth MIDI synthesiser](sound-devices/general-midi.md#fluidsynth) | Yes
| **GUS**         | [Gravis UltraSound digital audio](sound-devices/gravis-ultrasound.md) | Yes
| **IMFC**        | [IBM Music Feature Card](sound-devices/imfc.md) | Yes
| **MASTER**      | Master output channel                        | Yes
| **INNOVATION**  | [Innovation SSI-2001 synthesiser](sound-devices/innovation.md) | No
| **MT32**        | [Roland MT-32 MIDI synthesiser](sound-devices/roland-mt-32.md) | Yes
| **OPL**         | [Yamaha OPL synthesiser](sound-devices/adlib-cms-sound-blaster.md) | Yes[^stereo]
| **PCSPEAKER**   | [PC speaker](sound-devices/pc-speaker.md)    | No
| **PS1**         | [IBM PS/1 Audio synthesiser](sound-devices/ibm-ps1audio.md) | No
| **PS1DAC**      | [IBM PS/1 Audio digital audio](sound-devices/ibm-ps1audio.md) | No
| **REELMAGIC**   | [ReelMagic MPEG audio](../graphics/reelmagic.md) | Yes
| **SB**          | [Sound Blaster digital audio](sound-devices/adlib-cms-sound-blaster.md) | Yes[^stereo]
| **SOUNDCANVAS** | [Roland Sound Canvas synthesiser](sound-devices/general-midi.md#sound-canvas) | Yes
| **STON1**       | [Stereo-on-1 digital audio](sound-devices/covox-variants.md#stereo-on-1-dac) | Yes
| **TANDY**       | [Tandy 1000 synthesiser](sound-devices/tandy.md) | No
| **TANDYDAC**    | [Tandy 1000 digital audio](sound-devices/tandy.md) | No

</div>

[^stereo]: The output can either mono or stereo, depending on the selected model.


## Minimising audio glitches

Even after setting the optimal [cpu_cycles](../system/cpu.md#cpu_cycles)
value, you may hear occasional clicks or pops. This depends on your
particular hardware, audio driver, and operating system combination.

Increasing the audio buffer sizes usually helps. The trade-off is higher
audio latency (a longer delay between an action and its sound). The
default `blocksize` is 1024 on Windows and 512 on macOS and Linux
(in sample frames). The default `prebuffer` is 25 ms on Windows and 20 ms
on the other platforms.

Try doubling both values as a starting point:

``` ini
[mixer]
blocksize = 2048
prebuffer = 50
```

The `blocksize` should be a power of two: 256, 512, 1024, 2048, 4096, or
8192. The `prebuffer` can be any value in milliseconds. Increase
further if glitches persist, but keep in mind that larger buffers mean more
noticeable input-to-sound delay.

Also make sure your [cpu_cycles](../system/cpu.md#cpu_cycles) isn't set
higher than the game actually needs --- overdoing it wastes host CPU
resources that could otherwise go towards glitch-free audio emulation.


## Configuration settings

Mixer settings are to be configured in the `[mixer]` section. For effects
settings (crossfeed, reverb, chorus), see [Mixer effects](mixer-effects.md#configuration-settings).


##### blocksize

:   Block size of the host audio device in sample frames (`1024` on Windows,
    `512` on other platforms by default). Valid range is 64 to 8192. Should
    be set to power-of-two values (e.g., 256, 512, 1024). Larger values
    might help with sound stuttering but will introduce more latency. Also
    see [negotiate](#negotiate).


##### compressor

:   Enable the auto-leveling compressor on the master channel to prevent
    clipping of the audio output.

    Possible values: `on` *default*{ .default }, `off`


##### denoiser

:   Remove low-level residual noise from the output of the OPL synth and the
    Roland Sound Canvas. The emulation of these devices is accurate to the
    original hardware, which includes the emulation of very low-level
    semi-random noise. Although this is authentic, most people would find it
    slightly annoying.

    Possible values:

    - `on` *default*{ .default } -- Enable the denoiser on the OPL and
      SOUNDCANVAS channels. The denoiser does not introduce any sound quality
      degradation; it only removes the barely audible residual noise in quiet
      passages.
    - `off` -- Disable the denoiser.


##### negotiate

:   Negotiate a possibly better [blocksize](#blocksize) setting (`off` on
    Windows, `on` on other platforms by default). Enable if you're not
    getting audio or the sound is stuttering with your `blocksize` setting.
    Disable to force the manually set `blocksize` value.

    Possible values: `on`, `off`


##### nosound

:   Enable silent mode (`off` by default). Sound is still emulated in silent
    mode, but DOSBox outputs no sound to the host. Capturing the emulated
    audio output to a WAV file works in silent mode.

    Possible values: `on`, `off` *default*{ .default }


##### prebuffer

:   How many milliseconds of sound to render in advance on top of
    [blocksize](#blocksize) (`25` on Windows, `20` on other platforms by
    default). Larger values might help with sound stuttering but will
    introduce more latency.


##### rate

:   Sample rate of DOSBox's internal audio mixer in Hz (`48000` by default).
    Valid range is 8000 to 96000 Hz. The vast majority of consumer-grade audio
    hardware uses a native rate of 48000 Hz.

    !!! note

        The OS will most likely resample non-standard sample rates to 48000 Hz
        anyway. Recommend leaving this as-is unless you have good reason to
        change it.
