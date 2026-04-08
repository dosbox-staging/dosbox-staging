---
toc_depth: 1
---

# The DOS eras

## Overview

The DOS era spanned roughly 15 years---from the original IBM PC in 1981 to the
final wave of DOS games in the late 1990s. Hardware evolved dramatically during
this period, and games were designed for the capabilities of their time.
Matching the emulated hardware to a game's era gives the most authentic
experience.

The key settings to adjust are [`machine`](system/general.md#machine) (graphics
standard), [`cputype`](system/cpu.md#cputype),
[`cpu_cycles`](system/cpu.md#cpu_cycles) (CPU speed), and the [sound device](sound/overview.md). See
[A short DOS primer](dos-primer.md) for an explanation of these concepts.


## The early days (1981--1984)

The IBM PC launched in 1981 as a business machine, but games appeared almost
immediately. The hardware was modest: an Intel 8088 running at 4.77 MHz,
monochrome Hercules graphics or CGA graphics offering four colours at
320&times;200, and the PC speaker as the only sound source. Software arrived
on 360 KB floppy disks, and many early games were "booter" titles that ran
directly from the floppy without an operating system.

### Notable games

<div class="compact" markdown>

- [Wizardry (1981)](https://www.mobygames.com/game/1209/wizardry-proving-grounds-of-the-mad-overlord/) --- the granddaddy of dungeon crawler RPGs in forced ironman mode
- [Castle Wolfenstein (1981)](https://www.mobygames.com/game/3115/castle-wolfenstein/) --- stealth action pioneer
- [Microsoft Flight Simulator (1982)](https://www.mobygames.com/game/4003/microsoft-flight-simulator-v10/) --- one of the first major PC games
- [Zork (1982)](https://www.mobygames.com/game/50/zork-the-great-underground-empire/) --- iconic text adventure
- [Ultima III: Exodus (1983)](https://www.mobygames.com/game/878/exodus-ultima-iii/) --- foundational RPG
- [Lode Runner (1983)](https://www.mobygames.com/game/243/lode-runner/) --- influential puzzle-platformer
- [The Hitchhiker's Guide to the Galaxy (1984)](https://www.mobygames.com/game/88/the-hitchhikers-guide-to-the-galaxy/) --- legendary Infocom text adventure
- [Alley Cat (1984)](https://www.mobygames.com/game/190/alley-cat/) --- beloved CGA platformer
- [King's Quest (1984)](https://www.mobygames.com/game/122/kings-quest/) --- first animated adventure game of the iconic Sierra On-Line
- [Rogue (1984)](https://www.mobygames.com/game/1743/rogue/) --- the original textmode roguelike

</div>


### Typical hardware

<div class="compact" markdown>

| Component   | Typical spec         |
| ----------- | --------------       |
| CPU         | Intel 8088, 4.77 MHz |
| Graphics    | CGA (or Hercules)    |
| Sound       | PC speaker           |
| RAM         | 256--640 KB          |

</div>


### DOSBox Staging config

``` ini
[dosbox]
machine = cga

[cpu]
cpu_cycles = 300

[sblaster]
sbtype = none
```

Alternatively, use `machine = hercules` to emulate the Hercules graphics
adapter.


## EGA and Tandy (1984--1987)

The IBM PC/AT introduced the 286 CPU in 1984 alongside EGA graphics, which
offered 16 colours graphics --- a big step up from the fixed 4-colour palettes
of CGA. Around the same time, Tandy cloned the failed IBM PCjr's enhanced
graphics and sound hardware into the much more successful Tandy 1000 line,
giving budget-conscious gamers 16-colour graphics and three-voice sound. The
PC speaker remained the dominant sound source on non-Tandy machines, though
the AdLib card appeared in 1987 to change that.

### Notable games

<div class="compact" markdown>

- [Ultima IV: Quest of the Avatar (1985)](https://www.mobygames.com/game/884/ultima-iv-quest-of-the-avatar/) --- pioneering RPG with a moral system
- [The Bard's Tale (1985)](https://www.mobygames.com/game/819/tales-of-the-unknown-volume-i-the-bards-tale/) --- classic party-based RPG
- [Where in the World Is Carmen Sandiego? (1985)](https://www.mobygames.com/game/163/where-in-the-world-is-carmen-sandiego/) --- educational classic
- [Starflight (1986)](https://www.mobygames.com/game/115/starflight/) --- open-world space exploration
- [Might and Magic: Book One (1986)](https://www.mobygames.com/game/1619/might-and-magic-book-one-secret-of-the-inner-sanctum/) --- sprawling first-person RPG
- [Space Quest (1986)](https://www.mobygames.com/game/114/space-quest-chapter-i-the-sarien-encounter/) --- humorous sci-fi adventure
- [Sid Meier's Pirates! (1987)](https://www.mobygames.com/game/214/sid-meiers-pirates/) --- genre-defining open-world adventure
- [Leisure Suit Larry (1987)](https://www.mobygames.com/game/379/leisure-suit-larry-in-the-land-of-the-lounge-lizards/) --- adult comedy adventure
- [Maniac Mansion (1987)](https://www.mobygames.com/game/714/maniac-mansion/) --- SCUMM engine debut
- [Dungeon Master (1987)](https://www.mobygames.com/game/834/dungeon-master/) --- real-time dungeon crawling pioneer

</div>

### Typical hardware

<div class="compact" markdown>

| Component   | Typical spec                                |
| ----------- | --------------                              |
| CPU         | Intel 286, 8--12 MHz                        |
| Graphics    | EGA (or Tandy)                              |
| Sound       | PC speaker (Tandy 3-voice, AdLib from 1987) |
| RAM         | 640 KB                                      |

</div>


### DOSBox Staging config

``` ini
[dosbox]
machine = ega

[cpu]
cpu_cycles = 1500
```

!!! tip

    Set `machine = tandy` for games that support Tandy graphics and sound.
    Many mid-1980s Sierra and other titles look and sound significantly better
    in Tandy mode.


## The VGA and Sound Blaster revolution (1987--1990)

Two hardware breakthroughs transformed PC gaming in the late 1980s. VGA
graphics arrived in 1987 with 256 colours at 320&times;200 resolution,
enabling visuals that could rival the Amiga. On the audio side, the AdLib card
brought FM synthesis to the PC in 1987, followed by the Sound Blaster in 1989
which added digital audio playback. The 386 CPU provided the processing power
to drive increasingly ambitious games. This period marks the moment PC gaming
became a serious force.

For those with deeper pockets, the **Roland MT-32** sound module (released in
1987) offered dramatically superior music through its LA synthesis engine.
Sierra On-Line was an early champion, and many late-80s games sound
remarkably better on the MT-32 compared to AdLib or Sound Blaster FM. It
remained expensive and never became mainstream, but it's widely regarded as
the best way to experience game music from this period.

### Notable games

<div class="compact" markdown>

- [Wasteland (1988)](https://www.mobygames.com/game/287/wasteland/) --- post-apocalyptic RPG that inspired Fallout
- [Pool of Radiance (1988)](https://www.mobygames.com/game/502/pool-of-radiance/) --- first Gold Box D&D RPG
- [Ultima V: Warriors of Destiny (1988)](https://www.mobygames.com/game/808/ultima-v-warriors-of-destiny/) --- deep narrative RPG
- [SimCity (1989)](https://www.mobygames.com/game/848/simcity/) --- city-building pioneer
- [Prince of Persia (1989)](https://www.mobygames.com/game/196/prince-of-persia/) --- rotoscoped animation breakthrough
- [Quest for Glory: So You Want to Be a Hero (1989)](https://www.mobygames.com/game/168/heros-quest-so-you-want-to-be-a-hero/) --- adventure-RPG hybrid
- [Populous (1989)](https://www.mobygames.com/game/613/populous/) --- the original god simulator
- [The Secret of Monkey Island (1990)](https://www.mobygames.com/game/616/the-secret-of-monkey-island/) --- peak LucasArts adventure blockbuster
- [Wing Commander (1990)](https://www.mobygames.com/game/3/wing-commander/) --- cinematic space combat
- [Railroad Tycoon (1990)](https://www.mobygames.com/game/70/sid-meiers-railroad-tycoon/) --- business simulation classic

</div>


### Typical hardware

<div class="compact" markdown>

| Component   | Typical spec                             |
| ----------- | --------------                           |
| CPU         | Intel 386DX, 25--33 MHz                  |
| Graphics    | VGA                                      |
| Sound       | Sound Blaster (or AdLib)<br>Roland MT-32 |
| RAM         | 1--4 MB                                  |

</div>


### DOSBox Staging config

``` ini
[dosbox]
machine = svga_s3

[cpu]
cpu_cycles = 6000

[sblaster]
sbtype = sb1

[midi]
mididevice = mt32
model = mt32_old
```


## The 486 era (1990--1993)

The 486 brought a huge performance leap in the early 1990s. The AMD 386DX-40
offered near-486 performance at a fraction of the cost, extending the 386's
life for budget users. Sound hardware matured rapidly --- the Sound Blaster
Pro and 16 added stereo output, and the Gravis UltraSound offered advanced
wavetable synthesis. Hard drives became affordable for home users, and games
grew to fill them.

The **Roland Sound Canvas SC-55**, released in 1991, became the reference
device for **General MIDI** game music, offering a standardised set of 128
realistic instrument sounds. Many games from this era supported both General
MIDI and the MT-32 --- the Sound Canvas for its broad instrument palette, and
the MT-32 for its distinctive character and the many titles already composed
for it. The **Roland CM-32L**, a cost-reduced MT-32 variant with extra sound
effects, was also popular.

### Notable games

<div class="compact" markdown>

- [Civilization (1991)](https://www.mobygames.com/game/585/sid-meiers-civilization/) --- the strategy game by which all others are measured
- [Eye of the Beholder (1991)](https://www.mobygames.com/game/835/eye-of-the-beholder/) --- stone-cold dungeon crawler classic
- [Lemmings (1991)](https://www.mobygames.com/game/683/lemmings/) --- genre-defying addictive puzzle game
- [Wolfenstein 3D (1992)](https://www.mobygames.com/game/306/wolfenstein-3d/) --- the first-person shooter prototype
- [Dune II (1992)](https://www.mobygames.com/game/241/dune-ii-the-building-of-a-dynasty/) --- a forerunner of the real-time strategy genre
- [Ultima Underworld (1992)](https://www.mobygames.com/game/690/ultima-underworld-the-stygian-abyss/) --- the first true 3D RPG
- [Indiana Jones and the Fate of Atlantis (1992)](https://www.mobygames.com/game/316/indiana-jones-and-the-fate-of-atlantis/) --- LucasArts adventure masterpiece
- [Ultima VII: The Black Gate (1992)](https://www.mobygames.com/game/608/ultima-vii-the-black-gate/) --- one of the greatest RPGs ever made
- [Star Control II (1992)](https://www.mobygames.com/game/179/star-control-ii/) --- epic space adventure
- [Syndicate (1993)](https://www.mobygames.com/game/281/syndicate/) --- cyberpunk real-time tactics
- [Day of the Tentacle (1993)](https://www.mobygames.com/game/719/maniac-mansion-day-of-the-tentacle/) --- pinnacle of point-and-click adventures

</div>


<div class="compact" markdown>

### Typical hardware

| Component   | Typical spec                                                   |
| ----------- | --------------                                                 |
| CPU         | Intel 486DX2, 33--66 MHz                                       |
| Graphics    | VGA                                                            |
| Sound       | Sound Blaster Pro 2 (or GUS)<br>Roland MT-32 (or General MIDI) |
| RAM         | 4--16 MB                                                       |

</div>


### DOSBox Staging config

``` ini
[dosbox]
machine = svga_s3

[cpu]
cpu_cycles = 30000

[sblaster]
sbtype = sbpro2

[gus]
# Only enable it for games that use the Gravis Ultrasound
gus = on

[midi]
mididevice = mt32
model = cm32l
```

Alternatively, use `sbtype = sb16` to emulate the Sound Blaster 16, and
`mididevice = soundcanvas` to emulate the Roland Sound Canvas SC-55.


## The Pentium era (1993--1996)

Intel's Pentium processor arrived in 1993 and rapidly became the standard for
serious gaming. The Sound Blaster 16 was ubiquitous, SVGA cards pushed
higher resolutions and colour depths, and CD-ROM drives became standard,
ushering in the era of full-motion video and CD audio soundtracks. This was
the golden age of DOS gaming --- the period that produced many of the
platform's most celebrated titles that served as blueprints for the modern
gaming industry.

By this point, the **Roland Sound Canvas** (and General MIDI in general) had
become the dominant standard for game music, displacing the MT-32. Most
Pentium-era games with MIDI support target General MIDI as their primary
option, and the SC-55 remains the best way to hear these soundtracks as
intended.

### Notable games

<div class="compact" markdown>

- [Master of Orion (1993)](https://www.mobygames.com/game/212/master-of-orion/) --- definitive 4X space strategy
- [DOOM (1993)](https://www.mobygames.com/game/1068/doom/) --- defined the FPS genre
- [X-COM: UFO Defense (1994)](https://www.mobygames.com/game/521/x-com-ufo-defense/) --- tactical strategy masterpiece
- [Panzer General (1994)](https://www.mobygames.com/game/1021/panzer-general/) --- accessible wargaming classic
- [Star Wars: TIE Fighter (1994)](https://www.mobygames.com/game/240/star-wars-tie-fighter/) --- best Star Wars space sim
- [System Shock (1994)](https://www.mobygames.com/game/681/system-shock/) --- cyberpunk immersive sim cult classic
- [Tex Murphy: Under a Killing Moon (1994)](https://www.mobygames.com/game/850/under-a-killing-moon/) --- FMV detective adventure
- [Command & Conquer (1995)](https://www.mobygames.com/game/338/command-conquer/) --- popularised real-time strategy
- [Warcraft II: Tides of Darkness (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/) --- landmark RTS
- [Jagged Alliance (1995)](https://www.mobygames.com/game/1038/jagged-alliance/) --- tactical mercenary RPG
- [Heroes of Might and Magic (1995)](https://www.mobygames.com/game/668/heroes-of-might-and-magic/) --- turn-based strategy gem
- [Discworld (1995)](https://www.mobygames.com/game/184/discworld/) --- Terry Pratchett point-and-click adventure
- [The Elder Scrolls: Daggerfall (1996)](https://www.mobygames.com/game/778/the-elder-scrolls-chapter-ii-daggerfall/) --- massive open-world RPG

</div>


### Typical hardware

<div class="compact" markdown>

| Component   | Typical spec                              |
| ----------- | --------------                            |
| CPU         | Intel Pentium, 90--133 MHz                |
| Graphics    | SVGA                                      |
| Sound       | Sound Blaster 16 (or GUS)<br>General MIDI |
| RAM         | 16 MB                                     |

</div>


### DOSBox Staging config

``` ini
[dosbox]
machine = svga_s3

[cpu]
cpu_cycles = 80000

[sblaster]
sbtype = sb16

[gus]
# Only enable it for games that use the Gravis Ultrasound
gus = on

[midi]
mididevice = soundcanvas
```


## The twilight of DOS (1996--1998)

By the mid-1990s, Windows 95 and DirectX were rapidly taking over PC gaming.
The final wave of DOS games pushed hardware hard --- fast Pentiums and Pentium
MMX processors, AWE32/64 wavetable sound cards, and SVGA graphics at
640&times;480 or higher resultion with early Voodoo 3D acceleration. These
late DOS titles often demanded top-of-the-line hardware and represent the
platform's technical peak, even as the ecosystem was transitioning to Windows.

### Notable games

<div class="compact" markdown>

- [Quake (1996)](https://www.mobygames.com/game/374/quake/) --- true 3D engine revolution
- [Duke Nukem 3D (1996)](https://www.mobygames.com/game/365/duke-nukem-3d/) --- interactive environments and humour
- [Heroes of Might and Magic II (1996)](https://www.mobygames.com/game/1513/heroes-of-might-and-magic-ii-the-succession-wars/) --- refined turn-based strategy
- [Tomb Raider (1996)](https://www.mobygames.com/game/348/tomb-raider/) --- 3D action-adventure landmark
- [Broken Sword (1996)](https://www.mobygames.com/game/499/circle-of-blood/) --- acclaimed point-and-click adventure
- [Descent II (1996)](https://www.mobygames.com/game/694/descent-ii/) --- six-degrees-of-freedom shooter
- [Dungeon Keeper (1997)](https://www.mobygames.com/game/156/dungeon-keeper/) --- play the villain strategy classic
- [Fallout (1997)](https://www.mobygames.com/game/223/fallout/) --- post-apocalyptic RPG masterpiece
- [Theme Hospital (1997)](https://www.mobygames.com/game/674/theme-hospital/) --- Bullfrog management sim

</div>


### Typical hardware

<div class="compact" markdown>

| Component   | Typical spec                                      |
| ----------- | --------------                                    |
| CPU         | Intel Pentium MMX, 166--233 MHz                   |
| Graphics    | SVGA                                              |
| Sound       | Sound Blaster 16 / AWE32 (or GUS)<br>General MIDI |
| RAM         | 32 MB                                             |

</div>


### DOSBox Staging config

``` ini
[dosbox]
machine = svga_s3

[dosbox]
memsize = 32

[cpu]
cputype = pentium_mmx
# 640x480 3D might require up to 400000 to run smoothly
cpu_cycles = 150000

[sblaster]
sbtype = sb16

[gus]
# Only enable it for games that use the Gravis Ultrasound
gus = on

[midi]
mididevice = soundcanvas
```


## Windows 3.x games

DOSBox Staging fully supports Windows 3.x, which was not a standalone
operating system but an operating environment running on top of DOS. A number
of games were released exclusively for Windows 3.1 with no DOS version ---
particularly full-motion video (FMV) adventures and multimedia titles that
leveraged Windows' built-in video and audio support. Many of these are cult
classics well worth rediscovering.

### Notable games

<div class="compact" markdown>

- [Myst (1993)](https://www.mobygames.com/game/1223/myst/) --- landmark first-person adventure
- [The Journeyman Project (1993)](https://www.mobygames.com/game/22007/the-journeyman-project/) --- time-travel sci-fi puzzle adventure
- [The Dark Eye (1995)](https://www.mobygames.com/game/1782/the-dark-eye/) --- surreal Edgar Allan Poe horror adventure
- [Shivers (1995)](https://www.mobygames.com/game/663/shivers/) ---  horror-themed first-person adventure
- [Dust: A Tale of the Wired West (1995)](https://www.mobygames.com/game/3990/dust-a-tale-of-the-wired-west/) --- FMV western adventure
- [Burn:Cycle (1995)](https://www.mobygames.com/game/3962/burncycle/) --- cyberpunk FMV adventure
- [Civilization II (1996)](https://www.mobygames.com/game/15/sid-meiers-civilization-ii/) --- definitive Windows 3.x strategy game
- [9: The Last Resort (1996)](https://www.mobygames.com/game/2176/9-the-last-resort/) --- surreal FMV puzzle adventure
- [Titanic: Adventure Out of Time (1996)](https://www.mobygames.com/game/2892/titanic-adventure-out-of-time/) --- historical FMV adventure
- [Blue Heat (1997)](https://www.mobygames.com/game/4954/blue-heat/) --- mature-themed video adventure

</div>


<div class="compact" markdown>

### Typical hardware

| Component   | Typical spec                     |
| ----------- | --------------                   |
| CPU         | Intel 486DX2, 33--66 MHz         |
| Graphics    | VGA                              |
| Sound       | Sound Blaster 16<br>General MIDI |
| RAM         | 4--16 MB                         |

</div>


### DOSBox Staging config

``` ini
[dosbox]
machine = svga_s3

[cpu]
cpu_cycles = 30000

[sblaster]
sbtype = sb16

[midi]
mididevice = soundcanvas
```

