# Creative Music System (CMS)

The **Creative Music System** (also referred to as **CMS** or **C/MS**) was
released in August 1987 by Creative Technology, the company's first sound
card. It used two Philips SAA1099 chips for square-wave synthesis --- a much
simpler sound than the [AdLib](adlib.md) card's FM synthesis, and it did not
sell well. The card was rebranded as **Game Blaster** a year later, without
making any changes to the hardware.

Despite the flop, Creative kept the CMS chips on the original [Sound Blaster
1.0](sound-blaster.md#sound-blaster-10) for backward compatibility (and
optionally on the [Sound Blaster 2.0](sound-blaster.md#sound-blaster-20)),
which is why CMS settings live in DOSBox Staging's shared `[sblaster]`
configuration section alongside the Sound Blaster's own.

DOS games typically split their audio into two streams handled by different
hardware: [synthesised music](../overview.md#synthesised-sound) and [digital
sound](../overview.md#digital-sound) (pre-recorded samples for speech and
sound effects). CMS handled only the music side, and only a small number of
games (mostly from Sierra) ever supported it --- this page is mostly relevant
only if you're configuring one of those titles. For most DOS games, see
[AdLib](adlib.md) and [Sound Blaster](sound-blaster.md) instead.

## Game Blaster emulation

As the CMS is also present on the [Sound Blaster
1.0](sound-blaster.md#sound-blaster-10), and optionally on the [Sound Blaster
2.0](sound-blaster.md#sound-blaster-20), CMS support in some early games might
only work with either the Game Blaster or one of the Sound Blaster models, but
not both. The reason for this is that the Game Blaster card had an extra
chip which some games used to auto-detect the card.

Use the following setting to emulate a Game Blaster card:

``` ini
[sblaster]
sbtype = gb
```

## Mixer channel

The CMS synthesizers outputs to the **CMS** [mixer
channel](../mixer.md#list-of-mixer-channels).



## Configuration settings

The CMS and Game Blaster settings are configured in the `[sblaster]` section.
The related Sound Blaster digital-audio settings are covered on the [Sound
Blaster](sound-blaster.md#configuration-settings) page.

##### cms

:   Enable CMS emulation.

    Possible values:

    - `auto` *default*{ .default } -- Auto-enable CMS emulation for [Sound
      Blaster 1](sound-blaster.md#sound-blaster-10) and Game Blaster.
    - `on` -- Enable CMS emulation on [Sound Blaster 1](sound-blaster.md#sound-blaster-10) and [2](sound-blaster.md#sound-blaster-20).
    - `off` -- Disable CMS emulation (except when the Game Blaster is
      selected).


##### cms_filter

:   Filter for the Creative Music System (CMS) / Game Blaster output:

    - `on` *default*{ .default } -- Filter the CMS output. This applies a 1st order low-pass filter at 6 kHz (`lpf 1 6000`).
    - `off` -- Don't filter the CMS output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.
