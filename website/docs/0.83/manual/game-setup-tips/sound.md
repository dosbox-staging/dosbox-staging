# Sound

## Run the game's sound setup utility

DOSBox Staging does not configure game audio on your behalf. Most DOS games
ship with a dedicated setup utility — usually `SETUP.EXE`, `SETSOUND.EXE`, or
`INSTALL.EXE` — that must be run before the game produces any sound. Running
the game executable directly without doing this first is probably the most
common reason for silence.

When a game's setup utility asks for hardware settings, use these defaults,
which match DOSBox Staging's emulated [Sound
Blaster](../sound/sound-blaster.md):

| Setting           | Value |
| ---               | ---   |
| Port / IO Address | `220` |
| IRQ               | `7`   |
| DMA               | `1`   |

Always use the setup utility's **test sound** option before saving. Many games
have **separate settings for music and sound effects** — configure and test
both.

## Hardcoded sound hardware settings

Some games ignore what you enter in their setup utility and use hardcoded
hardware values instead. A number of classic titles hardcode the Sound Blaster
DMA channel to `1` regardless of what you configure. [Prince of Persia
(1990)](https://www.mobygames.com/game/TODO/prince-of-persia/) is a well-known
case. If a game produces no sound despite an apparently correct setup, check
whether it hardcodes its own values, and make sure DOSBox Staging's emulated
sound card matches what the game expects.

The **Gravis UltraSound** is emulated at non-factory-default I/O settings so
it can coexist with the Sound Blaster without conflicts. A small number of
games and demos expect the GUS at its factory-default I/O address (`240`) and
will misbehave at any other address. Check the game's documentation if you
have GUS-specific problems.


## Sound Blaster model

The [`sbtype`](../sound/sound-blaster.md#sbtype) setting controls which Sound
Blaster model is emulated. The default is `sb16`, but older games sometimes
work better with an older model:

```ini
[sblaster]
sbtype = sb2    ; or sb1, sbpro1, sbpro2, sb16
```

[Spelljammer: Pirates of Realmspace
(1992)](https://www.mobygames.com/game/dos/spelljammer-pirates-of-realmspace/)
crashes when certain enemies attack unless `sbtype = sb16`. [Worms
(1995)](https://www.mobygames.com/game/340/worms/) has a persistent DMA bug
with `sb16` and needs `sbtype = sb1`.


## MIDI music

For games that support General MIDI or Roland MT-32, the MIDI configuration
matters a lot. See the [MIDI reference](../sound/midi.md) for setup
instructions.

### General MIDI and MT-32 are not interchangeable

These are completely different synthesiser standards with different
instruments and timbres. If DOSBox Staging is set to emulate MT-32 but the
game is configured for General MIDI output (or vice versa), the music will
play but sound wrong — wrong instruments, garbled melodies, or a completely
different character than intended. Always match what the game's setup selects
to what DOSBox Staging is actually providing.

[Might and Magic: World of
Xeen](https://www.mobygames.com/game/TODO/might-and-magic-world-of-xeen/) is a
cautionary example in both directions: its MT-32 driver crashes the game after
a few minutes of play, so General MIDI must be used instead. But then the
music was composed for MT-32, so it sounds different. There's no ideal
solution — just pick the one that doesn't crash.

### MT-32 ROM versions

The Roland MT-32 hardware came in two distinct generations:

- **Old ROMs** — the original MT-32, released 1987
- **New ROMs** — the CM-32L and later variants

These are not compatible. Many older Sierra adventure games and other
late-1980s titles were composed specifically for the original MT-32's old
ROMs, and will crash or produce garbled audio with new ROMs. If MT-32 music is
crashing a late-80s or early-90s title at startup or producing obviously wrong
output, try switching to the old ROM set. See the [MT-32 section of the MIDI
manual](../sound/midi.md#mt-32) for how to switch.

Some early Legend Entertainment games — including [Eric the
Unready](https://www.mobygames.com/game/TODO/eric-the-unready/) and Gateway —
have a different MT-32 problem: their MPU-401 driver does unusual things to
the MIDI interface, causing tempo fluctuations under DOSBox Staging. The fix
is to run the `LEGMPU.COM` utility before starting the game. See the [Game
Issues
wiki](https://github.com/dosbox-staging/dosbox-staging/wiki/Game-issues) for
the download link.

### Game freezes on startup with General MIDI selected

If a game freezes right after the title screen or intro when General MIDI is
selected as the music device, this is almost always a known bug in the game's
own MPU-401 MIDI driver. It affects [Hi-Octane
(1995)](https://www.mobygames.com/game/2208/hi-octane/) and [Privateer 2: The
Darkening
(1996)](https://www.mobygames.com/game/363/privateer-2-the-darkening/), among
others. Switch to Sound Blaster for music as a workaround.

### FluidSynth and Capital Tone Fallback

If you're using [FluidSynth](../sound/midi.md#fluidsynth) for General MIDI,
the soundfont you choose matters. A soundfont missing any of the 128 standard
GM instruments triggers **Capital Tone Fallback (CTF)** — the synth
substitutes a similar-but-wrong instrument. The result can be noticeably off,
especially in games with complex soundtracks. Use a comprehensive GM soundfont
to avoid this.


## CD audio

Many games use Red Book CD audio for their soundtrack — the music plays
directly from the disc, completely separately from MIDI or Sound Blaster
audio. To get CD audio working, mount the disc image using the `mount` command
with a `.cue` file. See [Mounting disc
images](../using-dosbox-staging/storage.md#mounting-disc-images) for the full
syntax.

A `.cue` file is the index that describes the disc's track layout. Mounting a
disc image without the `.cue` file (e.g., mounting only the `.iso`) will give
you the game's data but CD audio won't play. Always mount the `.cue` file when
one is available.

DOSBox Staging also supports auto-mounting disc images. See the [Storage
reference](../using-dosbox-staging/storage.md#auto-mounting) for details on
how to set this up so you don't have to manually mount the disc every time you
launch a game.

Don't use Daemon Tools or similar virtual drive software alongside DOSBox
Staging — it conflicts with CD handling.


## File locking

The [`file_locking`](../system/dos.md#file_locking) setting in `[dos]`
controls whether DOS file locking is emulated. Some Windows 3.1 games and a
handful of DOS games crash or misbehave during file operations — including
saves — depending on whether this is enabled or disabled.

[Indiana Jones and His Desktop Adventures (Win
3.1)](https://www.mobygames.com/game/TODO/indiana-jones-and-his-desktop-adventures/)
throws a "Sharing violation" error unless `file_locking = off`. If a game
fails unexpectedly during file access, try toggling this setting.

## Games that require EMS for audio

A significant number of DOS games require [EMS (Expanded
Memory)](../system/dos.md#ems) for sound effects or music to work, even when
the game itself runs. The failure mode varies: some play music but not sound
effects, others play nothing at all. To enable EMS and disable XMS, which can
conflict:

```ini
[dos]
xms = false
ems = true
```

See also the [Memory section](cpu-and-memory.md#memory) for broader context on
memory configuration. The following games are known to require EMS for full
audio or will not run at all without it:

- [Aces of the Pacific (1992)](https://www.mobygames.com/game/1419/aces-of-the-pacific/) — Does not work. Requires 311,296 bytes of EMS.

- [Aladdin (1993)](https://www.mobygames.com/game/1476/aladdin/) — Does not work. EMS allocation error.

- [Blackthorne (1994)](https://www.mobygames.com/game/1445/blackthorne/) — Does not work. Requires at least 2,048K of EMS.

- [Darklands (1992)](https://www.mobygames.com/game/258/darklands/) — Does not work. Unable to find EMS memory.

- [Delta V (1994)](https://www.mobygames.com/game/3970/delta-v/) — Does not work. Requires EMS memory.

- [Frontier: Elite II (1993)](https://www.mobygames.com/game/802/frontier-elite-ii/) — Does not work. Requires 768K of expanded memory.

- [Lemmings 2: The Tribes (1993)](https://www.mobygames.com/game/1603/lemmings-2-the-tribes/) — Digital sound effects do not play (music works). No Expanded Memory Manager found.

- [Master of Magic (1994)](https://www.mobygames.com/game/1600/master-of-magic/) — Does not work. Requires at least 2,700K of expanded memory.

- [Monster Bash (1993)](https://www.mobygames.com/game/597/monster-bash/) — Digital sound effects do not play (music works). Sound Blaster sounds require 100K bytes of EMS.

- [NHL Hockey (1993)](https://www.mobygames.com/game/1529/nhl-hockey/) — Does not work. Insufficient EMS available.

- [Sid & Al's Incredible Toons (1993)](https://www.mobygames.com/game/2050/sid-als-incredible-toons/) — Does not work. Requires 737,280 free bytes of EMS.

- [Star Trek: 25th Anniversary — CD-ROM (1993)](https://www.mobygames.com/game/564/star-trek-25th-anniversary/) — Does not work. Requires 560K conventional and 1,024K EMS.

- [Star Wars: X-Wing — Floppy (1993)](https://www.mobygames.com/game/658/star-wars-x-wing/) — Advanced sound effects and music do not play.

- [Star Wars: X-Wing — CD (1994)](https://www.mobygames.com/game/658/star-wars-x-wing/) — Does not work. Not enough memory.

- [Star Wars: TIE Fighter (1994)](https://www.mobygames.com/game/782/star-wars-tie-fighter/) — Does not work. Requires 565K conventional and 896K EMS.

- [The Elder Scrolls: Arena (1994)](https://www.mobygames.com/game/1083/the-elder-scrolls-arena/) — Does not work. Not enough EMS.

- [The Incredible Machine 2 (1994)](https://www.mobygames.com/game/1607/the-incredible-machine-2/) — Does not work. Requires 262,144 free bytes of EMS.

- [Ultima Underworld: The Stygian Abyss (1992)](https://www.mobygames.com/game/640/ultima-underworld-the-stygian-abyss/) — Does not work. Out of EMS memory. Error code C001.

- [Ultima Underworld II: Labyrinth of Worlds (1993)](https://www.mobygames.com/game/1065/ultima-underworld-ii-labyrinth-of-worlds/) — Does not work. Out of EMS memory. Error code C001.

- [Wing Commander II: Vengeance of the Kilrathi (1991)](https://www.mobygames.com/game/496/wing-commander-ii-vengeance-of-the-kilrathi/) — Some sound effects and music do not work. No Expanded Memory detected.


!!! note

    This list is sourced from the [VOGONS Wiki list of programs that require
    EMS](https://www.vogonswiki.com/index.php/List_of_programs_that_require_EMS).
    MobyGames links for Aces of the Pacific, Blackthorne, Darklands, Delta V,
    Frontier: Elite II, Lemmings 2, Master of Magic, Monster Bash, NHL Hockey,
    Sid & Al's, Star Trek, X-Wing, TIE Fighter, Arena, Incredible Machine 2,
    both Ultima Underworlds, and Wing Commander II are confirmed. The Aladdin
    link is inferred and should be verified before publishing.
