# Sound overview

## Audio support in DOS games

Audio support in DOS games is a complex topic that is both fascinating and
bewildering at the same time. Many competing audio solutions from different
manufacturers were available on the market throughout most of the DOS-era,
especially from 1987 onwards. Most of these audio devices were totally
incompatible with each other and often had vastly different capabilities,
necessitating game developers to add direct support for every popular device
at the time their game was released. Moreover, as many of these devices were
initially rather expensive, their adoption was slow and gradual. Beside
supporting top of the range audio devices, developers had to add several
fallback options to cater for the least common denominator cards out there, so
people could get at least *some* sort of sound out of their games.

Although in the early days there were no standardisation efforts when it came
to game audio, de facto standards did emerge over time, so the situation is
not as bad as it looks at first glance. While the number of distinct audio
devices DOSBox can emulate is nearing 20, many of them are niche options, and
the practical gamer only needs to be familiar with the 4-5 most common ones.

But before that, we need to gain some understanding on how audio in DOS games
generally works.

### Synthesised sound

Initially, music and sound effects in computer games were not stored as
digitised waveforms of audio recordings (as are WAV, FLAC and MP3 files, for
instance). The biggest reason for this were the memory and storage limitations
of these early computers. An IBM PC with 640 *kilobytes* of on-board memory
could store only a little more than a minute's worth of 8-bit 10kHz digitised
audio---and that would leave no space for the operating system and the program
itself! Most home users could not afford a hard drive throughout the 1980s,
and even when they started to become common in the early 90s, games were still
coming on multiple floppy disks that could typically only hold 720kB or 1.44MB
worth of data.

Yet we know that many of these games had quite elaborate, sometimes hour-long
soundtracks! So how did they do it?

The solution was to store music as a series of notes instead of digitised
audio. These notes would be played back by a synthesiser chip in hardware, and
the chip could be programmed to assign different sounding instruments to
certain notes. In essence, before the advent of the CD-ROM that could hold
650MB to 700MB of data (a vast amount for the time), or even stereo audio
tracks at 16-bit 44.1kHz, all games were using hardware synthesisers for music
playback.

These most well-known of these was the [AdLib Music Synthesizer
Card](../sound-devices/adlib-cms-sound-blaster/#adlib-music-synthesizer-card),
or just **AdLib** in short, which was almost universally supported by all
games from about 1990 onwards. Before that, the [Tandy 3
Voice](../sound-devices/tandy/) was the only step-up from the built-in PC
speaker, but only the PCjr or Tandy versions of some games. Of course, these
simple synthesisers could only approximate the sound of realistic
instruments, and the result always unmistakably sounded like early electronic
music.

[Roland MT-32](../sound-devices/roland-mt-32/) and [General
MIDI](../sound-devices/general-midi/) music of later DOS era games followed
the same basic principle, but utilising much more expensive external sound
modules instead. These were the top-of-the-line sound devices of the day,
capable of convincingly emulating the sound of real instruments, and producing
CD-quality soundtracks in the right hands. The communication between the
computer and these external modules was also standardised.


### Digital sound

Some audio cards had ADPCM decompression capabilities
implemented in hardware that could cut down storage requirements to a tenth,
but support was not universal across sound cards, and developers had to go
with the least common denominator format, which was uncompressed audio.



## Configuring games for audio


### Selecting the best audio option

It is a bit less confusing to look at the best audio options separately for
the three major DOS gaming eras. Naturally, these are only general guidelines
and there are always exceptions, but most people would agree with this order
of preference (most desirable options come first).

**Early DOS era (1982--1987)**

<div class="compact" markdown>

- [Tandy 3 Voice](../sound-devices/tandy/)
- [PC speaker](../sound-devices/pc-speaker/)

</div>

**Late 80s / early 90s era (1987--1992)**

<div class="compact" markdown>

- [Roland MT-32](../sound-devices/roland-mt-32/)
- [Sound Blaster](../sound-devices/adlib-cms-sound-blaster/#sound-blaster-10)
- [AdLib Music Synthesizer Card](../sound-devices/adlib-cms-sound-blaster/#adlib-music-synthesizer-card)
- [Tandy 3 Voice](../sound-devices/tandy/)
- [Game Blaster / Creative Music System (C/MS)](../sound-devices/adlib-cms-sound-blaster#creative-music-system-cms-game-blaster)
- [PC speaker](../sound-devices/pc-speaker/)

</div>

**Mid-90s / late DOS era (1992--1997)**

<div class="compact" markdown>

- [General MIDI](../sound-devices/general-midi/)
- [Roland MT-32](../sound-devices/roland-mt-32/)
- [Gravis UltraSound](../sound-devices/gravis-ultrasound/)
- [Sound Blaster 16](../sound-devices/adlib-cms-sound-blaster/#sound-blaster-16)
- [Sound Blaster Pro](../sound-devices/adlib-cms-sound-blaster/#sound-blaster-pro) / [Sound Blaster Pro 2.0](../sound-devices/adlib-cms-sound-blaster/#sound-blaster-pro-20)
- [Sound Blaster](../sound-devices/adlib-cms-sound-blaster/#sound-blaster-10)

</div>

