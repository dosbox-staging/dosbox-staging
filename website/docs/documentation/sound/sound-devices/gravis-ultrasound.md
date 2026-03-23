# Gravis UltraSound

The Gravis UltraSound was an advanced synthesizer released by an unlikely
manufacturer: Canadian joystick company Advanced Gravis. Its audio was far
ahead of any other consumer device of the time, supporting wave-table
synthesis, stereo sound, 14-channel playback at 44.1 KHz or a whopping 32
channels of playback at 19.2 KHz.

However, the Ultrasound eschewed any attempt at backwards-compatibility with
AdLib or Soundblaster cards. Programs had to be written to specifically take
advantage of its capabilities. Many DOS users kept a Sound Blaster in their PC
in addition to an Ultrasound, in case they needed to run a program that did
not support the more advanced card. (And in DOSBox, this can be imitated by
turning on both devices in your configuration file, which is recommended.)

One quirk of the Ultrasound is that, unlike most synthesizers, it did not come
with any voices pre-installed on the card. All voices had to be installed from
disk either at driver load time or by the application. Because of this, a set
of drivers and "patch files" is needed in order to use the Ultrasound in
DOSBox. Due to incompatibilities between the license of the patch files and
DOSBox's GPL license, these files cannot be distributed with DOSBox, so you
will need to download them from another website:

The Gravis Ultrasound is configured under the gus category. It has several
options, which are explained in the comments in the configuration file. Of
particular note is the ultradir option, which must be set to the path to the
patch files inside DOSBox. (Which is likely not the same as the path on your
real hard drive.)



On the plus side, when games did offer support for the Gravis, it provided a far greater audio capability than anything else on the home consumer market. It allowed for 14 channels at 44kHz playback or 32 channels at 19.2kHz.

Several games require the use of Gravis drivers, and as copyrighted software from a commercial company, these drivers are not included with the open-sourced DOSBox installation. You need to install the drivers within DOSBox session to a subdirectory and afterwards change ultradir= to point to this directory path in Windows, macOS or Linux.





https://retronn.de/imports/gus_config_guide.html


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

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../../analog-output-filters/#custom-filter-settings)
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
