# AdLib, CMS & Sound Blaster

For many who owned a gaming PC in the 1990s, the name **Sound Blaster** is a
synonym for "sound card" --- and rightly so, as the Sound Blaster family of
cards by Creative Technology was the de facto standard for DOS gaming. The
first Sound Blaster was released in 1989, and it quickly became the
best-selling expansion card for the PC. Virtually all games from 1990 onwards
had Sound Blaster support in some shape or form, and this remained unchanged
for the rest of the DOS-era until the late 90s.

DOSBox can emulate all major revisions of the Sound Blaster family. One of the
most common models was the [Sound Blaster 16](#sound-blaster-16) which has
good (but not perfect) backward-compatibility with earlier cards. This is the
sound card DOSBox emulates by default to ensure the widest possible
compatibility with games,

Most games that support the AdLib or Sound Blaster work fine with the DOSBox
defaults, and don't require you to load any DOS drivers. A small percentage of
games, however --- especially from the early Sound Blaster days --- only work
with a specific model, or require a driver to be loaded. Please refer to the
[List of games that require a sound driver](#){ external } on the wiki for
further details.


## Sound Blaster overview

### On-board synthesisers

In addition to their digital audio playback capabilities (also referred to as
**PCM sound**), all Sound Blaster models feature at least one on-board
synthesiser for the generation of music and sound effects. The most important
of these is the [AdLib](#adlib-music-synthesizer-card) that is present on all
Sound Blaster cards. The AdLib is also referred to as **OPL sound** (after the
Yamaha OPL range of chips responsible for the "AdLib sound"), or **FM sound**
(after the OPL chip's FM sound synthesis capabilities). DOSBox uses the highly
accurate NukedOPL library to achieve a nearly bit-perfect emulation of the OPL
chips.

The other less common synthesiser is the [Creative Music
System](#creative-music-system-cms-game-blaster) (also referred to as **C/MS**, or
**CMS**) which is only present on the earliest Sound Blaster models.

Because the history and even the emulation of these synthesisers are so
closely entwined with those of the Sound Blaster, we are going to discuss them
together.


### Emulated Sound Blaster models

The below table lists all Sound Blaster models emulated by DOSBox, along with
their main capabilities:

| Model                                                         | `sbtype` value | Digital audio                          | Synthesiser                 |
| ------------------------------------------------------------- | -------------- | -------------------------------------- | --------------------------- |
| [Game Blaster](#creative-music-system-cms-game-blaster)       | `gb`           | N/A                                    | CMS (stereo)                |
| [Sound Blaster 1.0](#sound-blaster-10)                        | `sb1`          | 8-bit mono 23kHz                       | OPL2 (mono)<br>CMS (stereo) |
| [Sound Blaster 2.0](#sound-blaster-20)                        | `sb2`          | 8-bit mono 23kHz                       | OPL2 (mono)<br>CMS (stereo) |
| [Sound Blaster Pro](#sound-blaster-pro)                       | `sbpro1`       | 8-bit mono 44kHz<br>8-bit stereo 23kHz | Dual OPL2 (stereo)          |
| [Sound&nbsp;Blaster&nbsp;Pro&nbsp;2.0](#sound-blaster-pro-20) | `sbpro2`       | 8-bit mono 44kHz<br>8-bit stereo 23kHz | OPL3 (stereo)               |
| [Sound Blaster 16](#sound-blaster-16)                         | `sb16`         | 16-bit stereo 44kHz                    | OPL3 (stereo)               |


### Mixer channels

The digital audio of the Sound Blaster is output to the **SB** [mixer
channel](../mixer.md#list-of-mixer-channels), while the AdLib (OPL)
synthesiser has its own dedicated **OPL** channel. Both channels can be either
mono or stereo, depending on the particular Sound Blaster model being
emulated.

The Creative Music System synthesiser has its own dedicated **CMS** channel,
which is always stereo.

The upshot of this is that the digital audio and synthesiser volumes can be
adjusted independently via the DOSBox mixer (e.g., in games that use
digital audio for speech and sound effects, and the synthesiser for music).

!!! note

    Starting with the Sound Blaster Pro, programs can adjust the volume of the
    digital audio, OPL, and CD Audio channels via the Sound Blaster software
    mixer. By default, DOSBox forwards these adjustments to the DOSBox mixer,
    allowing programs to change the volumes of these channels. This can
    potentially result in your manually set volumes being overridden. To
    prevent this from happening, set the [`sbmixer`](#sbmixer) configuration
    setting to `off`.


### Default settings

The default Sound Blaster settings shown below have been selected for the
widest possible compatibility with games. Most games can auto-detect the
presence of a Sound Blaster card and these settings, but in case they need a
little help, here are the values to use:

<div class="compact" markdown>

| Setting                                | Config setting    | Default value |
| -------------------------------------- | ----------------- | ------------- |
| Base address (or I/O address, or port) | [`sbbase`](#sbbase) | 220           |
| IRQ (or interrupt)                     | [`irq`](#irq)       | 7             |
| DMA (or low DMA)                       | [`dma`](#dma)       | 1             |
| High DMA (or HDMA)                     | [`hdma`](#hdma)     | 5             |

</div>

If you have configured a game for Sound Blaster speech and music but can't
hear both or maybe there is no sound at all, or if the game hangs or crashes,
first double check that the above settings have been set in the game's
configuration. Many games don't let you to set all of them manually and just
assume some common fixed values instead. Please refer to the detailed
description of the config settings for troubleshooting tips.


### Selecting the best Sound Blaster model for a game

DOSBox emulates the [Sound Blaster 16](#sound-blaster-16) by default to ensure
the widest overall compatibility with DOS games. This lowest common
denominator setting will serve you well if you just want to play some games
with minimum hassle. If that's the case, you might as well stop reading this
chapter now, and only come back to it if you've encountered issues with
certain titles, or if you want to further your understanding of the subtler
differences between the various Sound Blaster models.

For the audio enthusiasts who are still with us, here's the thing: if you want
to coax the best sound out of a particular game, often you'll need to select a
specific Sound Blaster model the game was primarily developed for.  Although
the Sound Blaster 16 is compatible with earlier models, this compatibility is
imperfect and is plagued by a range of issues:

Stereo bug
:   Due to a design flaw, games with support for stereo digital audio on
    Sound Blaster Pro output mono sound only on the Sound Blaster 16.

Different analog low-pass output filters
:   Sound Blaster cards before the Sound Blaster 16 have a fixed frequency
    low-pass filter on the output to reduce the effects of aliasing which sounds
    like metallic overtones when playing digital audio at low sample rates.

    This fixed filter was changed to a variable-frequency design on the Sound
    Blaster 16, and the character of the filter was also altered. The old
    filter let a good amount of the metallic overtones through, resulting in a
    characteristic gritty, crunchy sound, while the new variable brick-wall
    filter was much more effective at reducing these overtones.

    The result is that low sample rate digital audio typical to earlier DOS games
    sounds overly muffled on the Sound Blaster 16 compared to earlier models.

Dual OPL2 support
:   The Sound Blaster Pro 1.0 is capable of stereo "dual OPL2" operation
    thanks to its two on-board OPL2 chips. This is the only Sound Blaster
    model with a dual OPL2 configuration. Previous models feature a single
    OPL2 chip that can output mono sound only, and later cards have the
    improved OPL3 chip that can also output stereo. Stereo sound on dual OPL2
    and OPL3 needs to be programmed differently, and some games were targeting
    only one of these two incompatible stereo OPL modes. Therefore, games that
    were written for dual OPL2 will only produce stereo synthesised sound on
    the Sound Blaster Pro 1.0 and no other model.

To provide a good out-of-the-box experience that works well enough with most
games without tinkering, DOSBox defaults to the imaginary `modern` [filter
setting](#sb_filter) on all Sound Blaster models. This implements simple
linear-interpolation instead of accurately emulating the analog output filter
--- this makes most games sound decent regardless of the Sound Blaster model
in use, but it's not authentic.

For the best authentic results, set the most appropriate Sound Blaster model
for each game via the [`sb_type`](#sbtype) configuration setting. Additionally,
set the [`sb_filter`](#sb_filter) setting to `auto` to let DOSBox select the
output filter appropriate for the emulated Sound Blaster model.

For example, use the following settings to enable authentic Sound Blaster Pro
1.0 emulation with dual OPL2 support:

``` ini
[sblaster]
sbtype = sbpro1
sb_filter = auto
```

Determining the best Sound Blaster model for a game can be tricky as the
different cards were released very close to each other; simply looking at the
game's release date often doesn't help much. Ultimately, you'll need to do a
little bit of research, experiment, and trust your ears.


---

## Creative Music System (CMS) / Game Blaster

The **Creative Music System** (also referred to as **CMS** or **C/MS**) was
released in August 1987 by Creative Technology. This was the first sound card
of the company, it sounded like a much simpler variant of the AdLib card, and
it did not sell that well.

The card was rebranded as **Game Blaster** a year later, without making any
changes to the hardware.

Use the following setting to enable Game Blaster emulation:

``` ini
[sblaster]
sbtype = gb
```

!!! note

    As the CMS is also present on the Sound Blaster 1.0, and optionally on the
    Sound Blaster 2.0, CMS support in some early games might only work with
    either the Game Blaster or one of the Sound Blaster models, but not both.


## FM synthesisers

### AdLib Music Synthesizer Card

The **AdLib Music Synthesizer Card** (typically simply referred to as
**AdLib**) was released by Ad Lib in 1987. It was the first popular sound card
for IBM PC compatibles, capable of producing multichannel music and sound
effects via its Yamaha OPL2 synthesizer chip.

Being such a huge step up from the PC speaker, the AdLib took the market by
storm. As it quickly became a de facto standard, many competing sound card
manufacturers started including full AdLib compatibility on their cards. The
result of this is that virtually all DOS games from 1990 onwards support the
AdLib, at least as a fallback option.

Digital audio is not supported on the card, but some games were able to
approximate it with various tricks (e.g., **F-15 Strike Eagle II** by
MicroProse).

Use the following settings to enable AdLib (OPL2) emulation:

``` ini
[sblaster]
sbtype = none
oplmode = opl2
```

!!! note

    When [`oplmode`](#oplmode) is set to `opl2`, CMS emulation is always
    enabled as well. This is legacy DOSBox behaviour, and will be addressed in
    a future version.


### AdLib Gold 1000

The **AdLib Gold 1000** was released in 1992. It has a Yamaha OPL3 chip and a
stereo audio processor on-board, and it also supports 12-bit digital audio. An
optional surround module is available to add further spatial effects to the
OPL output (e.g., chorus and reverb).

DOSBox fully emulates the card, including the surround module, except for the
digital audio capabilities.

Very few games have direct support for the AdLib Gold 1000, and no known game
makes use of its digital audio features. The single game in existence that
really takes advantage of the AdLib Gold and its surround module is the
adventure game **Dune** from 1992.

Use the following settings to enable AdLib Gold 1000 emulation. Typically, you
would use the card together with a Sound Blaster for digital audio. The setup
utility of [Dune](https://en.wikipedia.org/wiki/Dune_(video_game)) should
auto-detect AdLib Gold and the surround module correctly with these settings.

``` ini
[sblaster]
sbtype = sb16
oplmode = opl3gold
```


### ESS Enhanced FM Audio (ESFM)

**ESFM** is the OPL3-compatible FM synthesiser found on later ESS AudioDrive
cards. In "legacy mode", ESFM is fully compatible with the Yamaha OPL3 and
yields almost identical output on most material. What sets it apart is its
"native mode", which offers advanced synthesis features surpassing the OPL3's
capabilities --- it bridges the gap between synthetic-sounding OPL music and
sample-based MIDI music.

Since ESFM was released in 1995, only a handful of games support native mode,
but in the few that do, the results sound quite spectacular.

- To run ESFM in **legacy mode**, use `oplmode = esfm` with any Sound Blaster
  model and configure the game for Sound Blaster and AdLib/OPL as usual.

- To use **native mode**, set `sbtype = ess` and configure the **ESS Technology
  ES1688, ES1788, ES1888 Enhanced FM Audio** MIDI music driver in the game's
  setup utility (most games that support ESFM natively use the Miles Sound
  System). For the digital audio driver, select the Sound Blaster Pro option
  (ESS AudioDrive cards are Sound Blaster Pro compatible).

``` ini
[sblaster]
sbtype = ess
```

??? note "Games with ESFM support"

    <div class="compact" markdown>

    - [Advanced Civilization (1995)](https://www.mobygames.com/game/5297/advanced-civilization/)
    - [Callahan's Crosstime Saloon (1997)](https://www.mobygames.com/game/2150/callahans-crosstime-saloon/)
    - [Heaven's Dawn (1995)](https://www.mobygames.com/game/45120/heavens-dawn/)
    - [Heroes of Might and Magic II (1996)](https://www.mobygames.com/game/1513/heroes-of-might-and-magic-ii-the-succession-wars/)
    - [Magic Carpet 2 (1995)](https://www.mobygames.com/game/790/magic-carpet-2-the-netherworlds/)
    - [Shannara (1995)](https://www.mobygames.com/game/3208/shannara/)
    - [The 11th Hour (1995)](https://www.mobygames.com/game/567/the-11th-hour/)
    - [The Gene Machine (1996)](https://www.mobygames.com/game/1121/the-gene-machine/)
    - [The Settlers II (1996)](https://www.mobygames.com/game/598/the-settlers-ii-veni-vidi-vici/)
    - [Theme Hospital (1997)](https://www.mobygames.com/game/674/theme-hospital/)
    - [WarCraft II (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/)
    - [Z (1996)](https://www.mobygames.com/game/346/z/)

    </div>


!!! tip

    You can also try to "retrofit" the `ESFM.MID` driver from Miles Sound
    System games that have it into earlier ones that don't. For example,
    **Discworld** sounds great with the ESFM driver from **Heaven's Dawn**.


## Sound Blaster models

### Sound Blaster 1.0

The original **Sound Blaster 1.0** was released by Creative Technology in 1989
as the successor of their Game Blaster card. It was the first sound card on
the market to have digital audio playback capabilities; it could output 8-bit
digital audio at up to 23kHz sample rates on a single mono channel. Full
compatibility with the CMS and the AdLib (Yamaha OPL2) played a critical role
in the card's enormous success. In less than a year, the Sound Blaster became
the best-selling expansion card for the PC.

This had transformed DOS gaming, as games could use realistic digital audio
effects while simultaneously playing music via the OPL2 or CMS synthesiser
chips.

Use the following setting to enable Sound Blaster 1.0 emulation:

``` ini
[sblaster]
sbtype = sb1
```

!!! note

    Certain games from the early Sound Blaster days might only work with the
    Sound Blaster 1.0 and no other models.


### Sound Blaster 2.0

The **Sound Blaster 2.0** was released by Creative Technology in October 1991.
Similarly to its predecessor, the Sound Blaster 1.0, it can play back 8-bit
digital audio on a single mono channel, but the maximum sample rate has been
increased to 44kHz.

The card retains the perfect AdLib (OPL2) compatibility, but CMS support was
only available as an optional add-on. Some early games specifically written
for Sound Blaster 1.0 might have problems with this or later cards.

Use the following setting to enable Sound Blaster 2.0 emulation:

``` ini
[sblaster]
sbtype = sb2
```


### Sound Blaster Pro

The **Sound Blaster Pro** (retroactively also referred to as **Sound Blaster
Pro 1.0**) was released by Creative Technology in May 1991. This was the first
model that introduced stereo sound into the Sound Blaster lineup. It is
backward-compatible with earlier models, but in addition to 8-bit mono
playback of digital audio at up to to 44kHz, it's also capable of stereo 8-bit
output at a maximum of 23kHz. A distinctive feature of the card is a *pair* of
on-board Yamaha OPL2 chips to support stereo synthesised sound.

This is the only Sound Blaster model with such a dual OPL2 configuration; many
games take advantage of it and output stereo OPL music when configured for the
Sound Blaster Pro.

Use the following setting to enable Sound Blaster Pro emulation:

``` ini
[sblaster]
sbtype = sbpro1
```

!!! important

    Games with stereo OPL sound are usually targeting either the dual OPL2
    equipped Sound Blaster Pro 1.0, or the OPL3 chip of later models (OPL3 is
    capable of stereo sound in a single chip). It is important to configure
    these games for the appropriate Sound Blaster model via the
    [`sbtype`](#sbtype) setting, and select the same model in the game's sound
    setup. Failing to do so will produce mono OPL sound at best, or no sound
    at all at worst.


!!! note

    This is the only card supported by the known OEM releases of Microsoft Windows
    3.0a with Multimedia Extensions.

??? note "Games with dual OPL2 stereo music"

    The following games are known to support stereo dual OPL2 music when
    configured for the Sound Blaster Pro (`sbtype = sbpro1`). Select
    "Sound Blaster Pro" (or just "Sound Blaster" in some cases) in the game's
    sound setup to hear stereo OPL.

    <div class="compact" markdown>

    - [11th Hour, The (1995)](https://www.mobygames.com/game/567/the-11th-hour/)
    - [Amberstar (1992)](https://www.mobygames.com/game/3160/amberstar/)
    - [Azrael's Tear (1996)](https://www.mobygames.com/game/1852/azraels-tear/)
    - [B-17 Flying Fortress (1992)](https://www.mobygames.com/game/1756/b-17-flying-fortress/)
    - [Battle Isle 2200 (1994)](https://www.mobygames.com/game/3420/battle-isle-2200/)
    - [BattleTech: The Crescent Hawks' Revenge (1990)](https://www.mobygames.com/game/233/battletech-the-crescent-hawks-revenge/)
    - [Blackthorne (1994)](https://www.mobygames.com/game/1445/blackthorne/)
    - [Body Blows (1993)](https://www.mobygames.com/game/1970/body-blows/)
    - [Castle of Dr. Brain (1991)](https://www.mobygames.com/game/1523/castle-of-dr-brain/)
    - [Cobra Mission (1992)](https://www.mobygames.com/game/3404/cobra-mission/)
    - [Command Adventures: Starship (1993)](https://www.mobygames.com/game/23005/command-adventures-starship/)
    - [Conquests of the Longbow (1991)](https://www.mobygames.com/game/1967/conquests-of-the-longbow-the-legend-of-robin-hood/)
    - [Darklands (1992)](https://www.mobygames.com/game/258/darklands/)
    - [David Leadbetter's Greens (1991)](https://www.mobygames.com/game/11959/david-leadbetters-greens/)
    - [Death Gate (1994)](https://www.mobygames.com/game/175/death-gate/)
    - [Der Clou! (The Clue!) (1994)](https://www.mobygames.com/game/2568/the-clue/)
    - [Discworld (1995)](https://www.mobygames.com/game/184/discworld/)
    - [Dragonsphere (1994)](https://www.mobygames.com/game/2257/dragonsphere/)
    - [Dune II (1992)](https://www.mobygames.com/game/321/dune-ii-the-building-of-a-dynasty/)
    - [Dungeon Master II (1995)](https://www.mobygames.com/game/914/dungeon-master-ii-skullkeep/)
    - [EcoQuest: The Search for Cetus (1991)](https://www.mobygames.com/game/584/ecoquest-the-search-for-cetus/)
    - [Elvira: Mistress of the Dark (1990)](https://www.mobygames.com/game/4050/elvira/)
    - [F-117A Nighthawk Stealth Fighter 2.0 (1991)](https://www.mobygames.com/game/655/f-117a-nighthawk-stealth-fighter-20/)
    - [F-15 Strike Eagle III (1992)](https://www.mobygames.com/game/445/f-15-strike-eagle-iii/)
    - [Fleet Defender (1994)](https://www.mobygames.com/game/4540/fleet-defender/)
    - [Formula One Grand Prix (1992)](https://www.mobygames.com/game/2071/world-circuit/)
    - [Gobliins 2 (1992)](https://www.mobygames.com/game/2625/gobliins-2-the-prince-buffoon/)
    - [Gods (1992)](https://www.mobygames.com/game/1511/gods/)
    - [Golden Axe (1990)](https://www.mobygames.com/game/199/golden-axe/)
    - [Gunship 2000 (1991)](https://www.mobygames.com/game/891/gunship-2000/)
    - [Harrier Jump Jet (1992)](https://www.mobygames.com/game/886/jump-jet/)
    - [Heroes of Might and Magic II (1996)](https://www.mobygames.com/game/1513/heroes-of-might-and-magic-ii-the-succession-wars/)
    - [Hi-Octane (1995)](https://www.mobygames.com/game/2208/hi-octane/)
    - [Hocus Pocus (1994)](https://www.mobygames.com/game/1237/hocus-pocus/)
    - [Hoyle's Official Book of Games Volume 3 (1991)](https://www.mobygames.com/game/2124/hoyle-official-book-of-games-volume-3/)
    - [Indiana Jones and the Fate of Atlantis (1992)](https://www.mobygames.com/game/1358/indiana-jones-and-the-fate-of-atlantis/)
    - [Inherit The Earth: Quest For The Orb (1994)](https://www.mobygames.com/game/4008/inherit-the-earth-quest-for-the-orb/)
    - [Jill of the Jungle (1992)](https://www.mobygames.com/game/774/jill-of-the-jungle/)
    - [Jones in the Fast Lane (1990)](https://www.mobygames.com/game/370/jones-in-the-fast-lane/)
    - [King's Quest V (1990)](https://www.mobygames.com/game/130/kings-quest-v-absence-makes-the-heart-go-yonder/)
    - [Knights of Xentar (1994)](https://www.mobygames.com/game/1665/knights-of-xentar/)
    - [Legend of Kyrandia (1992)](https://www.mobygames.com/game/394/fables-fiends-the-legend-of-kyrandia-book-one/)
    - [Leisure Suit Larry 1 VGA (1991)](https://www.mobygames.com/game/413/leisure-suit-larry-1-in-the-land-of-the-lounge-lizards/)
    - [Leisure Suit Larry 5 (1991)](https://www.mobygames.com/game/408/leisure-suit-larry-5-passionate-patti-does-a-little-undercover-w/)
    - [Leisure Suit Larry 6 (1993)](https://www.mobygames.com/game/407/leisure-suit-larry-6-shape-up-or-slip-out/)
    - [Lord of the Rings Volume 1 (1990)](https://www.mobygames.com/game/3870/jrr-tolkiens-the-lord-of-the-rings-vol-i/)
    - [Lord of the Rings Volume 2 (1993)](https://www.mobygames.com/game/1480/jrr-tolkiens-the-lord-of-the-rings-vol-ii-the-two-towers/)
    - [Master of Magic (1994)](https://www.mobygames.com/game/200/master-of-magic/)
    - [Master of Orion (1993)](https://www.mobygames.com/game/466/master-of-orion/)
    - [Mixed-Up Mother Goose (1987)](https://www.mobygames.com/game/22295/mixed-up-mother-goose/)
    - [Mixed-Up Fairy Tales (1991)](https://www.mobygames.com/game/6880/mixed-up-fairy-tales/)
    - [Pirates! Gold (1993)](https://www.mobygames.com/game/166/pirates-gold/)
    - [Police Quest III (1991)](https://www.mobygames.com/game/148/police-quest-3-the-kindred/)
    - [Prince of Persia 2 (1993)](https://www.mobygames.com/game/78/prince-of-persia-2-the-shadow-the-flame/)
    - [Quest for Glory II (1990)](https://www.mobygames.com/game/169/quest-for-glory-ii-trial-by-fire/)
    - [Quest for Glory IV (1994)](https://www.mobygames.com/game/119/quest-for-glory-shadows-of-darkness/)
    - [Return of the Phantom (1993)](https://www.mobygames.com/game/2011/return-of-the-phantom/)
    - [Settlers II, The (1996)](https://www.mobygames.com/game/598/the-settlers-ii-veni-vidi-vici/)
    - [Sid Meier's Civilization (1991)](https://www.mobygames.com/game/585/sid-meiers-civilization/)
    - [Sid Meier's Colonization (1994)](https://www.mobygames.com/game/366/sid-meiers-colonization/)
    - [Sid Meier's Railroad Tycoon Deluxe (1993)](https://www.mobygames.com/game/4037/sid-meiers-railroad-tycoon-deluxe/)
    - [Silpheed (1988)](https://www.mobygames.com/game/167/silpheed/)
    - [Space Quest I VGA (1991)](https://www.mobygames.com/game/187/space-quest-i-roger-wilco-in-the-sarien-encounter/)
    - [Space Quest III (1989)](https://www.mobygames.com/game/142/space-quest-iii-the-pirates-of-pestulon/)
    - [Space Quest IV (1991)](https://www.mobygames.com/game/143/space-quest-iv-roger-wilco-and-the-time-rippers/)
    - [Special Forces (1992)](https://www.mobygames.com/game/5816/special-forces/)
    - [Star Wars: Dark Forces (1995)](https://www.mobygames.com/game/379/star-wars-dark-forces/)
    - [Stone Age (1992)](https://www.mobygames.com/game/23765/stone-age/)
    - [Street Fighter II (1992)](https://www.mobygames.com/game/6239/street-fighter-ii/)
    - [Strike Commander (1993)](https://www.mobygames.com/game/1365/strike-commander/)
    - [Stronghold (1993)](https://www.mobygames.com/game/482/stronghold/)
    - [Summer Challenge (1992)](https://www.mobygames.com/game/2109/summer-challenge/)
    - [Supaplex (1991)](https://www.mobygames.com/game/2106/supaplex/)
    - [System Shock (1994)](https://www.mobygames.com/game/545/system-shock/)
    - [Theme Hospital (1997)](https://www.mobygames.com/game/674/theme-hospital/)
    - [Ultima Underworld (1992)](https://www.mobygames.com/game/690/ultima-underworld-the-stygian-abyss/)
    - [Ultima Underworld II (1993)](https://www.mobygames.com/game/691/ultima-underworld-ii-labyrinth-of-worlds/)
    - [William Shatner's TekWar (1995)](https://www.mobygames.com/game/2420/william-shatners-tekwar/)
    - [Wing Commander (1990)](https://www.mobygames.com/game/3/wing-commander/)
    - [Winter Challenge (1991)](https://www.mobygames.com/game/2110/the-games-winter-challenge/)
    - [Witchhaven (1995)](https://www.mobygames.com/game/3318/witchaven/)
    - [Witchhaven II (1996)](https://www.mobygames.com/game/3514/witchaven-ii-blood-vengeance/)
    - [Wolf (1994)](https://www.mobygames.com/game/754/wolf/)
    - [X-Com: UFO Defense (1994)](https://www.mobygames.com/game/1358/x-com-ufo-defense/)
    - [Xatax (1994)](https://www.mobygames.com/game/1263/xatax/)
    - [Z (1996)](https://www.mobygames.com/game/346/z/)

    </div>


### Sound Blaster Pro 2.0

The **Sound Blaster Pro 2.0** was released by Creative Technology in 1991,
quickly replacing its predecessor, the Sound Blaster Pro.

It is functionally the same as the earlier Sound Blaster Pro with one key
difference: instead of the dual OPL2 chips, it sports a single Yamaha OPL3
chip also capable of producing stereo sound. The OPL3 has perfect backward
compatibility with the single-OPL2 configuration of earlier cards, but not
with dual OPL2.

Use the following setting to enable Sound Blaster Pro 2.0 emulation:

``` ini
[sblaster]
sbtype = sbpro2
```

!!! important

    Games with stereo OPL sound are usually targeting either the dual OPL2
    equipped Sound Blaster Pro 1.0, or the OPL3 chip of later models, such as
    the Sound Blaster Pro 2.0 (OPL3 is capable of stereo sound in a single
    chip). It is important to configure these games for the appropriate Sound
    Blaster model via the [`sbtype`](#sbtype) setting, and select the same model
    in the game's sound setup. Failing to do so will produce mono OPL sound at
    best, or no sound at all at worst.

??? note "Games with OPL3 stereo music"

    The following games are known to support OPL3 stereo music. Configure the
    game for the Sound Blaster Pro 2.0 (`sbtype = sbpro2`) or Sound Blaster 16
    (`sbtype = sb16`) to hear stereo OPL3 output.

    <div class="compact" markdown>

    - [Cybersphere (1993)](https://www.mobygames.com/game/79550/cybersphere/)
    - [Dark Forces (1995)](https://www.mobygames.com/game/379/star-wars-dark-forces/)
    - [Darklands (1992)](https://www.mobygames.com/game/258/darklands/)
    - [Descent (1995)](https://www.mobygames.com/game/454/descent/)
    - [Descent 2 (1996)](https://www.mobygames.com/game/694/descent-ii/) (if CD audio is disabled)
    - [Doom (1993)](https://www.mobygames.com/game/1068/doom/) (requires `SET DMXOPTION=-phase -opl3` in the autoexec)
    - [Doom 2 (1994)](https://www.mobygames.com/game/299/doom-ii/) (requires `SET DMXOPTION=-phase -opl3` in the autoexec)
    - [Dune (1992)](https://www.mobygames.com/game/380/dune/) (on AdLib Gold)
    - [Dungeon Master 2 (1995)](https://www.mobygames.com/game/914/dungeon-master-ii-skullkeep/)
    - [Fleet Defender (1994)](https://www.mobygames.com/game/4540/fleet-defender/)
    - [FlixMix (1995)](https://www.mobygames.com/game/13819/flixmix/)
    - [Gabriel Knight (1993)](https://www.mobygames.com/game/665/gabriel-knight-sins-of-the-fathers/)
    - [Hexen (1995)](https://www.mobygames.com/game/383/hexen-beyond-heretic/) (requires `SET DMXOPTION=-phase -opl3` in the autoexec)
    - [King's Quest VI (1992)](https://www.mobygames.com/game/131/kings-quest-vi-heir-today-gone-tomorrow/)
    - [Lost Vikings, The (1993)](https://www.mobygames.com/game/1547/the-lost-vikings/)
    - [Magic Carpet 2 (1995)](https://www.mobygames.com/game/790/magic-carpet-2-the-netherworlds/)
    - [Sam & Max Hit the Road (1993)](https://www.mobygames.com/game/672/sam-max-hit-the-road/)
    - [Settlers II, The (1996)](https://www.mobygames.com/game/598/the-settlers-ii-veni-vidi-vici/)
    - [Sid Meier's Civilization (1991)](https://www.mobygames.com/game/585/sid-meiers-civilization/) (requires updated sound drivers)
    - [SimCity 2000 (1994)](https://www.mobygames.com/game/657/simcity-2000/)
    - [SlipSpeed (2020)](https://www.mobygames.com/game/144988/slipspeed/)
    - [Stonekeep (1995)](https://www.mobygames.com/game/1876/stonekeep/)
    - [System Shock (1994)](https://www.mobygames.com/game/545/system-shock/)
    - [Tetris Classic (1992)](https://www.mobygames.com/game/27780/tetris-classic/)
    - [Theme Hospital (1997)](https://www.mobygames.com/game/674/theme-hospital/)
    - [TIE Fighter (1994)](https://www.mobygames.com/game/240/star-wars-tie-fighter/)
    - [WarCraft II (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/)
    - [Xatax (1994)](https://www.mobygames.com/game/1263/xatax/)

    </div>


### Sound Blaster 16

The **Sound Blaster 16** was released by Creative Technology in June 1992.
This is the first Sound Blaster card to support CD-quality sound (stereo
16-bit digital audio at 44kHz sample rate). The card features the Yahama OPL3
chip that has perfect backward-compatibility with OPL2 (but not dual OPL2),
and supports stereo operation in a single chip.

DOSBox emulates the Sound Blaster 16 by default. Most games either support
this card directly, or will work with selecting an earlier Sound Blaster model
in the game's setup, making use of the card's backward-compatibility feature.
This is a good overall default, but if you're an audio enthusiast or would
just like to get the best possible experience out of a given game, reading the
[Selecting the best Sound Blaster model for a
game](#selecting-the-best-sound-blaster-model-for-a-game) section is highly
recommended.

Use the following setting to enable Sound Blaster 16 emulation:

``` ini
[sblaster]
sbtype = sb16
```

??? note "Games with true 16-bit digital audio"

    While most games advertised Sound Blaster 16 support, the majority only
    played back 8-bit samples due to floppy disk storage constraints. Starting
    around 1995, CD-ROM titles began incorporating genuine 16-bit recordings,
    typically at 22 kHz. The jump from 8-bit to 16-bit is noticeable primarily
    as reduced background noise; the higher sampling rate often matters more to
    the perceived audio quality than the bit depth alone.

    The following games are known to use true 16-bit digital audio with the
    Sound Blaster 16 (`sbtype = sb16`):

    <div class="compact" markdown>

    - AH-64D Longbow
    - Blood
    - Command & Conquer
    - Command & Conquer: Red Alert
    - Crusader: No Remorse
    - Crusader: No Regret
    - Descent II
    - Duke Nukem 3D
    - Dungeon Keeper
    - EcoQuest 2
    - Eradicator
    - Fade to Black
    - Freddy Pharkas
    - FX Fighter
    - Gabriel Knight
    - Gabriel Knight 2
    - Hi-Octane
    - King's Quest VII
    - Leisure Suit Larry 6
    - M.A.X.
    - Mission Critical
    - Pepper's Adventures in Time
    - Police Quest IV
    - Quake
    - Quest for Glory IV
    - Rise of the Triad
    - Shadow Warrior
    - Shattered Steel
    - Slater and Charlie Go Camping
    - Space Quest 6
    - Star Trek: The Next Generation
    - Torin's Passage
    - Under a Killing Moon
    - The Pandora Directive
    - Wing Commander III
    - Wing Commander IV

    </div>


## Audio artifact mitigation

Real Sound Blaster and OPL hardware exhibited certain audio quirks --- startup
pops, hanging notes, DC bias --- that were simply part of the experience at the
time. DOSBox Staging faithfully emulates these behaviours, but also provides
settings to mitigate them when strict authenticity is less important than a
clean listening experience. These settings are best applied on a per-game basis.

When a Sound Blaster card powers on, there is a brief transient pop before
the DMA audio stabilises. The [`sbwarmup`](#sbwarmup) setting silences the
initial DMA output for a configurable number of milliseconds (100 by default)
to suppress these startup pops. Reduce it if initial audio is getting cut off.

Some games don't properly send note-off commands to the OPL FM synthesiser,
resulting in notes that play forever. The [`opl_fadeout`](#opl_fadeout) setting
monitors OPL port activity and fades out any remaining sound after the game
stops writing to the chip. Known affected games include [The Bard's Tale](https://www.mobygames.com/game/819/tales-of-the-unknown-volume-i-the-bards-tale/). The
`fade` preset waits 500 ms then fades over 500 ms; custom timing can be
specified as two values in milliseconds.

A small number of games play PCM (sampled) audio through the OPL synthesiser
rather than using it for FM synthesis. This can cause clicking and popping due
to DC bias in the OPL output. The [`opl_remove_dc_bias`](#opl_remove_dc_bias)
setting removes this bias. Only enable it for known affected games: Golden
[Golden Eagle](https://www.mobygames.com/game/44953/golden-eagle/),
[Wizardry 6](https://www.mobygames.com/game/709/wizardry-bane-of-the-cosmic-forge/), and
[Wizardry 7](https://www.mobygames.com/game/1561/wizardry-crusaders-of-the-dark-savant/).


## Configuration settings

Due to their tightly coupled nature, all AdLib, Creative Music System (CMS),
Game Blaster and Sound Blaster settings are to be configured in the
`[sblaster]` section.

### Sound Blaster

Many DOS programs use the `BLASTER` DOS environment variable to auto-detect
and auto-configure the Sound Blaster card. On a real machine, this variable is
set up in `AUTOEXEC.BAT` during the Sound Blaster driver installation process.

DOSBox injects the `BLASTER` environment variable at startup based on the
card's configuration in the `[sblaster]` section. You should never set this
variable manually.

To view the value of the variable, execute the `SET BLASTER` DOS
command. For example, this is the output you'll get with the default settings:

```
BLASTER=A220 I7 D1 H5 T6
```

The meaning of the space-separated values are as follows:

<div class="compact" markdown>

- `Axyz` --- base address ([`sbbase`](#sbbase))
- `Ix` --- IRQ number ([`irq`](#irq))
- `Dx` --- DMA channel ([`dma`](#dma))
- `Hx` --- High DMA channel ([`hdma`](#hdma))

</div>

The number after the `T` parameter describes the type of the card:

<div class="compact" markdown>

- `1` -- Sound Blaster 1.x
- `2` --  Sound Blaster Pro
- `3` --  Sound Blaster 2.x
- `4` --  Sound Blaster Pro 2
- `5` --  Sound Blaster Pro MCA *--- not used by DOSBox*
- `6` --  Sound Blaster 16, 32, AWE32/64, or ViBRA
- `10` -- Sound Blaster MCA *--- not used by DOSBox*

</div>


##### dma

:   The DMA (also referred to as low DMA) channel used for 8-bit
    digital audio on the Sound Blaster.

    Possible values: `0`, `1` *default*{ .default }, `3`, `5`, `6`, `7`.

    You can typically only choose between `0`, `1` and `3` on real cards.

    !!! important

        Many earlier games assume DMA 1 and do not let the user to change
        it, making it the most compatible setting.


##### hdma

:   The high DMA channel used for 16-bit digital audio on the Sound Blaster
    16.

    Possible values: `0`, `1`, `3`, `5` *default*{ .default }, `6`, `7`.

    You can typically only choose between `5`, `6` and `7` on real cards.


##### irq

:   The IRQ (interrupt) number of the Game Blaster / Sound Blaster.

    Possible values: `3`, `5`, `7` *default*{ .default }, `9`, `10`, `11`, `12`.

    You can typically only choose between `3`, `5`, `7` and `10` on real cards,

    Many games do not recognize IRQs higher than 7.

    !!! important

        Cards before the Sound Blaster 16 defaulted to IRQ 7. From the Sound
        Blaster 16 onwards, the default had been changed to IRQ 5. Because of
        this, many earlier games assume IRQ 7 and do not let the user to
        change it. Games that assume IRQ 5 are rarer, making `7` the overall
        most compatible setting.


##### sb_filter

:   Filter settings for the Sound Blaster digital audio output:

    - `auto` -- Use the appropriate filter determined by the [`sbtype`](#sbtype)
      setting.
    - `sb1`, `sb2`, `sbpro1`, `sbpro2`, `sb16` -- Use the digital audio filter
      of this Sound Blaster model.
    - `modern` *default*{ .default } -- Use linear interpolation upsampling
      regardless of the `sbtype` setting. This is the legacy DOSBox
      behaviour; it acts as a sort of a low-pass filter. Use `auto` instead
      to enable model-authentic filter emulation.
    - `off` -- Don't filter the digital audio output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.

    These are the model specific low-pass filter settings for the digital
    audio output:

    <div class="compact" markdown>

    | `sbtype`             | Filter type           | Cutoff frequency |
    | -------------------- | --------------------- | ---------------- |
    | `sb1`                | 2nd order (12dB/oct)  | 3.8kHz
    | `sb2`                | 2nd order (12dB/oct)  | 4.8kHz
    | `sbpro1`<br>`sbpro2` | 2nd order (12dB/oct)  | 3.2kHz
    | `sb16`               | brickwall             | variable;<br>half of the playback rate

    </div>

    <!-- TODO audio samples -->


##### sb_filter_always_on

:   Force the Sound Blaster Pro 2 filter to be always on. Other Sound
    Blaster models don't allow toggling the filter in software.

    Possible values: `on`, `off` *default*{ .default }


##### sbbase

:   The base address (also referred to as I/O address, or port) of the Game
    Blaster / Sound Blaster.

    Possible values: `220` *default*{ .default }, `240`, `260`, `280`, `2a0`, `2c0`, `2e0`, `300`.

    You can typically only choose between `220`, `240` and `260` on real cards.

    The AdLib Music Synthesizer Card has a fixed base address of `380` that
    cannot be changed. Games can access the Yamaha OPL chip on Sound Blaster
    cards either at base address `380` (this is the AdLib compatibility mode),
    or at the Sound Blaster's base address.

    !!! important

        Virtually all games with Game Blaster / Creative Music System (CMS)
        support only work with the base address set to `220`. A significant
        number of early 90s games with Sound Blaster support also assume a
        base address of `220` and do not let the user the change it, making
        this the most compatible setting.


##### sbmixer

:   Allow the Sound Blaster mixer to modify the DOSBox mixer.

    Possible values: `on` *default*{ .default }, `off`

    Starting with the Sound Blaster Pro, programs can adjust the volume of the
    digital audio, OPL, and CD Audio channels via the Sound Blaster software
    mixer. By default, DOSBox forwards these adjustments to the DOSBox mixer
    which can potentially result in your manually set volumes being
    overridden. To prevent this from happening, set the `sbmixer`
    configuration setting to `off` to retain full manual control over the
    channel volumes. Note that this might break some games that use the mixer
    to achieve certain effects (e.g., dynamically lowering the music volume
    when voice-overs are playing).


##### sbtype

:   Sound Blaster model to emulate.

    Possible values:

    <div class="compact" markdown>

    - `none`/`off` -- Disable Sound Blaster emulation.
    - `gb` -- [Creative Music System (CMS) / Game Blaster](#creative-music-system-cms-game-blaster)
    - `sb1` -- [Sound Blaster 1.0](#sound-blaster-10)
    - `sb2` -- [Sound Blaster 2.0](#sound-blaster-20)
    - `sbpro1` -- [Sound Blaster Pro](#sound-blaster-pro)
    - `sbpro2` -- [Sound Blaster Pro 2.0](#sound-blaster-pro-20)
    - `sb16` *default*{ .default } -- [Sound Blaster 16](#sound-blaster-16)
    - `ess` -- ESS ES1688 AudioDrive. The ESS DAC is not emulated but the
      card is Sound Blaster Pro compatible; just configure the game for Sound
      Blaster digital sound. Use this for getting ESS Enhanced FM music via
      the card's ESFM synthesiser in games that support it.

    </div>

    Please refer to the [Emulated Sound Blaster
    models](#emulated-sound-blaster-models) section for an overview of the
    capabilities of the different models.


##### sbwarmup

:   Silence initial digital audio after card power-on for this many
    milliseconds.

    Default value: `100`

    This mitigates pops heard when starting many Sound Blaster based games.
    Reduce this if you notice intial playback is missing digital audio.


### OPL

!!! note

    Some games trigger low-level residual noise from the OPL synth in quiet
    passages. The [Denoiser](../mixer.md#denoiser) removes this without
    degrading sound quality.

##### opl_fadeout

:   Fade out hanging notes on the OPL synth.

    Possible values:

    - `off` *default*{ .default } -- Don't fade out hanging notes.
    - `fade` -- Fade out hanging notes. You should only enable this in games
      that sometimes play hanging notes that never stop (e.g., Bard's Tale).
    - `<custom>` -- A custom fade-out definition in the following format:
      `WAIT FADE`, where `WAIT` is how long after the last I/O port write
      fading begins (between 100 and 5000 milliseconds), and `FADE` is the
      fade-out period (between 10 and 3000 milliseconds). Examples:
        - `300 200` (wait 300 ms before fading out over a 200 ms period)
        - `1000 3000` (wait 1 second before fading out over 3 seconds)



##### opl_filter

:   Filter settings for the OPL output:

    - `auto` *default*{ .default } -- Use the appropriate filter determined by the [`sbtype`](#sbtype) setting.
    - `sb1`, `sb2`, `sbpro1`, `sbpro2`, `sb16` -- Use the OPL filter of this Sound Blaster model.
    - `off` -- Don't filter the OPL output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.

    These are the model specific low-pass filter settings for OPL output: 

    <div class="compact" markdown>

    | `sbtype`             | Filter type           | Cutoff frequency |
    | -------------------- | --------------------- | ---------------- |
    | `sb1`<br>`sb2`       | 1st order (6dB/oct)   | 12kHz 
    | `sbpro1`<br>`sbpro2` | 1st order (6dB/oct)   | 8kHz
    | `sb16`               | no filter             | N/A

    </div>

    <!-- TODO audio samples -->


##### opl_remove_dc_bias

:   Remove DC bias from the OPL output.

    Possible values: `on`, `off` *default*{ .default }

    Some games play digitised music and sound effects using the OPL (AdLib)
    channels by rapidly changing the volume in very crude steps, similar to
    how the Disney Sound Source and Covox LPT DAC operate. This can produce
    annoying pops. Enable this setting to eliminate them.

    Known affected games: Golden Eagle (1991), Wizardry VI: Bane of the
    Cosmic Forge (1990), Wizardry VII (1992).

    Recommended settings for **Wizardry VI** when configured for AdLib sound:

    ``` ini
    [sblaster]
    sbtype = none
    oplmode = opl2
    opl_remove_dc_bias = true
    opl_filter = lpf 2 5500

    [speaker]
    pcspeaker = off

    [autoexec]
    mixer opl 500
    ```


##### oplmode

:   OPL mode to emulate.

    DOSBox uses the highly accurate NukedOPL library to achieve a nearly
    bit-perfect emulation of the Yamaha OPL chips.

    Possible values:

      - `auto` *default*{ .default } -- Use the appropriate model determined
        by [`sbtype`](#sbtype).

      - `opl2` -- Yamaha OPL2 (YM3812, mono).

      - `dualopl2` -- Dual OPL2 (two OPL2 chips in stereo configuration).

      - `opl3` -- Yamaha OPL3 (YMF262, stereo).

      - `opl3gold` -- OPL3 and the optional AdLib Gold Surround module. Use
        with `sbtype = sb16` to emulate the AdLib Gold 1000.

      - `esfm` -- ESS ESFM (enhanced Yamaha OPL3 compatible FM synth).

      - `none`/`off` -- Disable OPL emulation.

    !!! note

        - `sbtype = none` and `oplmode = opl2` emulates the original AdLib
          card.

        - Only `oplmode = esfm` is not enough to get ESS Enhanced FM music
          in games; you'll also need to set `sbtype = ess`. `oplmode = esfm`
          is useful to get ESFM-flavoured OPL with original Sound Blaster
          models.


### CMS

##### cms

:   Enable CMS emulation.

    Possible values:

    - `auto` *default*{ .default } -- Auto-enable CMS emulation for Sound
      Blaster 1 and Game Blaster.
    - `on` -- Enable CMS emulation on Sound Blaster 1 and 2.
    - `off` -- Disable CMS emulation (except when the Game Blaster is
      selected).


##### cms_filter

:   Filter for the Creative Music System (CMS) / Game Blaster output:

    - `on` *default*{ .default } -- Filter the CMS output. This applies a 1st order low-pass filter at 6 kHz (`lpf 1 6000`).
    - `off` -- Don't filter the CMS output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.

