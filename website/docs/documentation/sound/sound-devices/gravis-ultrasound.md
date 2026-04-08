# Gravis UltraSound

The Gravis UltraSound (GUS) was released in 1992 by an unlikely manufacturer:
Canadian joystick company Advanced Gravis. Its audio capabilities were far
ahead of anything else on the consumer market --- wavetable synthesis, stereo
sound, and up to 32 channels of simultaneous playback.

The catch? The GUS made no attempt at backwards compatibility with AdLib or
Sound Blaster cards. Programs had to be written specifically for it. Many DOS
gamers kept a [Sound Blaster](adlib-cms-sound-blaster.md) alongside their GUS
for titles that lacked native support — and in DOSBox Staging, you can do the
same by enabling both devices in your configuration.

Another quirk: unlike most sound cards, the GUS shipped with no built-in
instrument sounds. All voices had to be loaded from disk via "patch files" at
driver load time. Due to licensing restrictions, these patch files can't be
distributed with DOSBox Staging, so you'll need to obtain them separately.

Where the GUS truly shone was in the demoscene and tracker music community.
[Second Reality](https://www.pouet.net/prod.php?which=5) by Future Crew,
widely considered one of the greatest DOS demos ever made, was designed to
sound its best on a GUS. Games with native GUS support, like [Star Control
II](https://www.mobygames.com/game/179/star-control-ii/), also benefited
enormously from its superior audio capabilities.

### Mixer channel

The Gravis UltraSound outputs to the **GUS** mixer channel.


## Configuration settings

Gravis UltraSound settings are to be configured in the `[gus]` section.

!!! note

    The default settings of base address 240, IRQ 5, and DMA 3 have been
    chosen so the GUS can coexist with a Sound Blaster card. This works fine
    for the majority of programs, but some games and demos expect the GUS
    factory defaults of base address 220, IRQ 11, and DMA 1. The default
    IRQ 11 is also problematic with specific versions of the DOS4GW extender
    that cannot handle IRQs above 7.


##### gus

:   Enable Gravis UltraSound emulation. Many games and all demos upload their
    own sounds, but some rely on the instrument patch files included with the
    GUS for MIDI playback (see [ultradir](#ultradir) for details). Some games
    also require `ULTRAMID.EXE` to be loaded prior to starting the game.

    Possible values: `on`, `off` *default*{ .default }


##### gus_filter

:   Filter for the Gravis UltraSound audio output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output (1st order low-pass at 8 kHz).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### gusbase

:   The IO base address of the Gravis UltraSound.

    Possible values: `210`, `220`, `230`, `240` *default*{ .default }, `250`,
    `260`.


##### gusdma

:   The DMA channel of the Gravis UltraSound.

    Possible values: `1`, `3` *default*{ .default }, `5`, `6`, `7`.


##### gusirq

:   The IRQ number of the Gravis UltraSound.

    Possible values: `2`, `3`, `5` *default*{ .default }, `7`, `11`, `12`,
    `15`.


##### ultradir

:   Path to the UltraSound directory (`C:\ULTRASND` by default). This should
    have a `MIDI` subdirectory containing the patches (instrument files)
    required by some games for MIDI music playback. Not all games need these
    patches; many GUS-native games and all demos upload their own custom sounds
    instead.
