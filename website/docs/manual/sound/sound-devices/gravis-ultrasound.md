# Gravis UltraSound

The **Gravis UltraSound (or just GUS)** was released in 1992 by an unlikely
manufacturer: Canadian joystick company Advanced Gravis. Its audio
capabilities were far ahead of anything else on the consumer market ---
wavetable synthesis, stereo sound, and up to 32 channels of simultaneous
playback.

The catch? The GUS made no attempt at backwards compatibility with AdLib or
Sound Blaster cards. Programs had to be written specifically for it. Many DOS
gamers kept a [Sound Blaster](adlib-cms-sound-blaster.md) alongside their GUS
for titles that lacked native support --- and in DOSBox Staging, you can do the
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


## How games use the GUS

Software used the GUS in several distinct ways. Understanding the difference
is important because each requires a different DOSBox configuration.


### Native GUS

Approximately 40 games have native GUS support. These titles use the card's
hardware mixing to play tracker-style music, uploading their own samples
directly to the GUS's on-board RAM --- no patch files or drivers needed. This
is the GUS at its best: crystal-clear multi-channel audio mixed in hardware
with a very low noise floor.

??? note "Notable native GUS games"

    - [Star Control II](https://www.mobygames.com/game/179/star-control-ii/)
    - [Doom](https://www.mobygames.com/game/1068/doom/)
    - [Epic Pinball](https://www.mobygames.com/game/263/epic-pinball/)
    - [One Must Fall: 2097](https://www.mobygames.com/game/2862/one-must-fall-2097/)
    - [Jazz Jackrabbit](https://www.mobygames.com/game/1487/jazz-jackrabbit/)
    - [Tyrian](https://www.mobygames.com/game/2064/tyrian/)

All demoscene productions with GUS support also fall into this category ---
they upload their own instrument samples and use the hardware mixer directly.

Native GUS games typically need only `gus = on` in the config. Select "Gravis
UltraSound" (or similar) in the game's setup utility. No patch files or TSRs
are required.

``` ini
[gus]
gus = on
```


### Gravis UltraMID

Around 100 games support the GUS through **UltraMID**, a MIDI translation
layer. These games send standard MIDI commands (just like they would to a
[Roland MT-32](roland-mt-32.md) or [General MIDI](general-midi.md) device), and the UltraMID driver translates them
into GUS hardware calls, loading the appropriate instrument samples from disk.

The result can sound very good --- somewhere between FM synthesis and true
General MIDI --- but it requires the GUS patch files to be installed. The
patches must be placed in the directory pointed to by
[`ultradir`](#ultradir) (default `C:\ULTRASND`), with the MIDI instrument
bank in a `MIDI` subdirectory.

Some UltraMID titles also require `ULTRAMID.EXE` to be loaded as a TSR before
starting the game. When this is the case, add it to the `[autoexec]` section:

``` ini
[gus]
gus = on

[autoexec]
C:\ULTRASND\ULTRAMID.EXE
```

!!! tip

    If the game's setup utility offers both "Gravis UltraSound" and
    "GUS MIDI" (or similar), try the native GUS option first --- it usually
    sounds better. Fall back to the MIDI/UltraMID option if the native one
    produces no music.

??? note "Notable UltraMID games"

    - [Hocus Pocus](https://www.mobygames.com/game/1237/hocus-pocus/)
    - [Rise of the Triad](https://www.mobygames.com/game/768/rise-of-the-triad-dark-war/)
    - [Raptor: Call of the Shadows](https://www.mobygames.com/game/1289/raptor-call-of-the-shadows/)
    - [Blake Stone: Aliens of Gold](https://www.mobygames.com/game/1371/blake-stone-aliens-of-gold/)
    - [Turrican II](https://www.mobygames.com/game/1652/turrican-ii-the-final-fight/)


For a comprehensive list of games with GUS support (both native and
UltraMID), consult the official **GLIST.TXT** compatibility list maintained
by Gravis:

- [gravisultrasound.com](http://www.gravisultrasound.com/files/documentation/GLIST.TXT)
- [retronn.de mirror](https://retronn.de/ftp/driver/Gravis/UltraSound/GAMES/GLIST.TXT)
- [dk.toastednet.org mirror](http://dk.toastednet.org/GUS/TEXT/GLIST.TXT)


## Setting up the GUS environment

For games that need the patch files (UltraMID titles), you'll need to set up
the GUS directory structure. The expected layout is:

```
C:\ULTRASND\
├── MIDI\
│   ├── ACBASS.PAT
│   ├── ACPIANO.PAT
│   ├── ... (instrument patch files)
│   └── DEFAULT.CFG
└── ULTRAMID.EXE
```

The patch files cannot be distributed with DOSBox Staging due to licensing
restrictions. Search online for "GUS patch files" or "GUS MIDI patches" to
find them. Several freely available patch sets exist.

Once the files are in place, ensure `ultradir` points to the correct location
in your DOSBox config:

``` ini
[gus]
gus = on
ultradir = C:\ULTRASND
```

!!! note

    Native GUS games and all demos do *not* need the patch files --- they
    upload their own instrument samples. You only need the `ULTRASND`
    directory setup for UltraMID games.


## GUS-only configuration

If you want to emulate a system with *only* a GUS and no Sound Blaster (as
some purists had in the 1990s), disable the other sound devices:

``` ini
[sblaster]
sbtype = none
oplmode = none

[gus]
gus = on
```

This can be useful for games where the GUS and Sound Blaster interfere with
each other, or when you want to ensure a game uses its GUS code path
exclusively.


## Mixer channel

The Gravis UltraSound outputs to the **GUS** [mixer](../mixer.md) channel.


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

    - `on` *default*{ .default } -- Filter the output. This applies a 1st order low-pass filter at 8 kHz (`lpf 1 8000`).
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
