# Roland Sound Canvas

!!! tip "New to MIDI in DOS games?"

    If you're not familiar with how MIDI works in DOS games, read the overview
    in the [MIDI](../midi.md) section first --- it explains how DOS games use
    MIDI and the difference between the [Roland MT-32](roland-mt-32.md) family
    of devices and **Roland Sound Canvas / General MIDI**.


The **Roland Sound Canvas SC-55**, released in 1991, was the first General
MIDI sound module and quickly became the de facto standard for DOS game music.
Most composers wrote and tested their music on Sound Canvas hardware, so the
same hardware was needed to hear the soundtrack as intended.

For background on the General MIDI and GS standards, and help deciding which
MIDI device to use for a particular game, see the [Which MIDI device should I
use?](../midi.md#which-midi-device-should-i-use)

Don't confuse the Sound Canvas with the [Roland MT-32](roland-mt-32.md) ---
General MIDI and MT-32 use incompatible instrument sets, moreover the MT-32 is
a fully programmable synthesizer, so music written for one usually sounds
completely wrong on the other. See [MT-32 vs General
MIDI](../midi.md#mt-32-vs-general-midi) for details.

DOSBox Staging emulates the SC-55 via the [Nuked SC55
CLAP](https://github.com/johnnovak/Nuked-SC55-CLAP) audio plugin (bundled with
the release packages), providing sample-accurate playback of the actual SC-55
sound engine --- as close to the real hardware as you can get without owning
one.

??? note "Notable games with Sound Canvas / General MIDI support"

    <div class="compact" markdown>

    - [Azrael's Tear (1996](https://www.mobygames.com/game/1852/azraels-tear/)
    - [Betrayal at Krondor (1993)](https://www.mobygames.com/game/285/betrayal-at-krondor/)
    - [Day of the Tentacle (1993)](https://www.mobygames.com/game/659/maniac-mansion-day-of-the-tentacle/)
    - [Descent (1995)](https://www.mobygames.com/game/454/descent/)
    - [Dig, The (1995)](https://www.mobygames.com/game/499/the-dig/)
    - [Discworld (1995)](https://www.mobygames.com/game/184/discworld/)
    - [Doom (1993)](https://www.mobygames.com/game/1068/doom/)
    - [Duke Nukem 3D (1996)](https://www.mobygames.com/game/365/duke-nukem-3d/)
    - [Elder Scrolls: Arena, The (1994)](https://www.mobygames.com/game/803/the-elder-scrolls-arena/)
    - [Elder Scrolls: Daggerfall, The (1996)](https://www.mobygames.com/game/778/the-elder-scrolls-chapter-ii-daggerfall/)
    - [Full Throttle (1995)](https://www.mobygames.com/game/414/full-throttle/)
    - [Gabriel Knight (1993)](https://www.mobygames.com/game/665/gabriel-knight-sins-of-the-fathers/)
    - [King's Quest VI: Heir Today, Gone Tomorrow (1992)](https://www.mobygames.com/game/131/kings-quest-vi-heir-today-gone-tomorrow/)
    - [Legend of Kyrandia II: Hand of Fate, The](https://www.mobygames.com/game/871/fables-fiends-hand-of-fate/)
    - [Leisure Suit Larry 6: Shape Up or Slip Out! (1993)](https://www.mobygames.com/game/407/leisure-suit-larry-6-shape-up-or-slip-out/)
    - [Lords of the Realm (1994)](https://www.mobygames.com/game/4303/lords-of-the-realm/)
    - [Master of Magic (1985)](https://www.mobygames.com/game/16961/master-of-magic/)
    - [Master of Orion (1993)](https://www.mobygames.com/game/212/master-of-orion/)
    - [Phantasmagoria (1995)](https://www.mobygames.com/game/1164/roberta-williams-phantasmagoria/)
    - [Quest for Glory III: Wages of War (1992)](https://www.mobygames.com/game/173/quest-for-glory-iii-wages-of-war/)
    - [Realms of Arkania: Star Trail (1994)](https://www.mobygames.com/game/3438/realms-of-arkania-star-trail/)
    - [Sam & Max Hit the Road (1993)](https://www.mobygames.com/game/672/sam-max-hit-the-road/)
    - [Shadow Warrior (1997)](https://www.mobygames.com/game/387/shadow-warrior/)
    - [Space Quest 6: Roger Wilco in the Spinal Frontier (1995)](https://www.mobygames.com/game/145/space-quest-6-roger-wilco-in-the-spinal-frontier/)
    - [Space Quest V: The Next Mutation 1993)](https://www.mobygames.com/game/144/space-quest-v-the-next-mutation/)
    - [Star Wars: Dark Forces (1995)](https://www.mobygames.com/game/379/star-wars-dark-forces/)
    - [Star Wars: TIE Fighter (1994)](https://www.mobygames.com/game/204/star-wars-tie-fighter/)
    - [Stonekeep (1995)](https://www.mobygames.com/game/1876/stonekeep/)
    - [System Shock (1994)](https://www.mobygames.com/game/545/system-shock/)
    - [Ultima VII (1992)](https://www.mobygames.com/game/608/ultima-vii-the-black-gate/)
    - [Under a Killing Moon (1994)](https://www.mobygames.com/game/850/under-a-killing-moon/)
    - [WarCraft II (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/)

    </div>

## Setting up the Sound Canvas ROM images

!!! warning

    Emulating the Roland Sound Canvas requires the original ROM data from the
    hardware itself --- the Sound Canvas MIDI device **cannot** function
    without these files. We can't bundle these ROM files with our release
    packages due to copyright restrictions, so you'll need to obtain and
    install them yourself.


Download the SC-55 ROM files from
[here](https://archive.org/details/nuked-sc-55-clap-rom-files), unpack the ZIP
archive, then move the contents of the `Nuked-SC55-Resources/ROMs/` folder
into the `soundcanvas-roms` folder inside your DOSBox Staging configuration
folder:

<div class="compact" markdown>

| Platform | ROM folder
|----------|---------------------------------------------------------
| Windows  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\soundcanvas-roms\`
| macOS    | `/Users/<USERNAME>/Library/Preferences/DOSBox/soundcanvas-roms/`
| Linux    | `$HOME/.config/dosbox/soundcanvas-roms/`

</div>

By doing so, you will make these ROMs globally available for all games.

This is what the contents of `soundcanvas-roms` should look like:

```
soundcanvas-roms
├── SC-55-v1.10
│   ├── sc55_rom1.bin
│   ├── sc55_rom2.bin
│   ├── sc55_waverom1.bin
│   ├── sc55_waverom2.bin
│   └── sc55_waverom3.bin
├── SC-55-v1.20
│   ...
├── SC-55-v1.21
...
```

You can also set a custom ROM folder via the
[`soundcanvas_rom_dir`](#soundcanvas_rom_dir) setting.


Now you can enable the Sound Canvas MIDI device in your config:

``` ini
[midi]
mididevice = soundcanvas
```

!!! note

    Sound Canvas emulation is CPU-intensive. You'll need a mid-range or better
    desktop-class CPU from the last 5--7 years for glitch-free playback. If
    your system struggles, [FluidSynth](fluidsynth.md) with a good SoundFont is
    a lighter alternative.

    Emulating SC-55mk2 is much more CPU-intensive than emulating the
    original SC-55 revision. The good news is that SC-55 v1.21 is the overall
    best version to use for most games --- you'll rarely need to use the
    SC-55mk2.

!!! tip

    You can switch between Sound Canvas models on the fly by changing the
    [`soundcanvas_model`](#soundcanvas_model) setting --- no restart needed.

For a hands-on walkthrough of setting up Sound Canvas emulation with a specific
game, see the [Star Wars: Dark
Forces](../../../getting-started/star-wars-dark-forces.md#sound-canvas-emulation)
chapter of the Getting Started guide.

## Listing the available models

Run the `MIXER /LISTMIDI` command to see the list of available Sound Canvas
models:

TODO

## Choosing a Sound Canvas model

The SC-55 went through several firmware revisions. The most important
difference is **Capital Tone Fallback (CTF)** --- a Roland GS feature that
ensures correct instrument playback when a game requests a GS "variation
tone" that isn't available on the module. Without CTF, the module may play
silence or the wrong instrument. Many DOS games rely on CTF for correct
audio, particularly for percussion and instrument variations.

Due to a patent dispute with Yamaha, Roland was forced to remove CTF from
later firmware revisions. This makes the choice of firmware version
significant for DOS gaming.

DOSBox Staging emulates the following Sound Canvas firmware versions:

<div class="compact" markdown>

| Model | Version | `soundcanvas_model` | CTF | Notes |
|-------|---------|---------------------|-----|-------|
| SC-55 | v1.00 | `sc55_100` | Yes | First release; some instrument mapping bugs |
| SC-55 | v1.10 | `sc55_110` | Yes | Bug fixes |
| SC-55 | v1.20 | `sc55_120` | Yes | Corrected instrument 122; GS reset |
| SC-55 | v1.21 | `sc55_121` | Yes | Fixed NRPN processing bugs; still has CTF support --- **overall best version** |
| SC-55 | v2.00 | `sc55_200` | No | CTF removed (Yamaha patent dispute) |
| SC-55mk2 | v1.00 | `sc55mk2_100` | No | 28 voices, 354 sounds; no CTF |
| SC-55mk2 | v1.01 | `sc55mk2_101` | No | Minor revisions |

</div>

The **SC-55 v1.21** is the best overall choice for DOS gaming. It has
**Capital Tone Fallback (CTF)** and addresses most issues present in earlier
revisions. DOSBox Staging's `soundcanvas_model = auto` prefers v1.21 when its
ROM is available. Avoid v2.00 and the mk2 models for games that rely on
CTF.

The **SC-55mk2** has higher polyphony, but its voice-allocation rules are
different from those of the original module. As a result, some games composed
for the original SC-55 can cut notes off earlier on the mk2 (for example,
[Settlers
II](https://www.mobygames.com/game/598/the-settlers-ii-veni-vidi-vici/)). Very
few games make meaningful use of the additional polyphony, and the differences
are usually subtle unless you know what to listen for. The mk2 is also more
CPU-intensive to emulate, so the SC-55 v1.21 is generally the better choice.


??? note "Games that require Capital Tone Fallback (CTF) for correct audio"

    These games use GS variation tones that depend on CTF. On SC-55 v2.00
    or Mk II firmware (which lack CTF), certain instruments or percussion
    will sound incorrect or be silent.

    <div class="compact" markdown>

    - [Blood (1997)](https://www.mobygames.com/game/793/blood/)
    - [Duke Nukem 3D (1996)](https://www.mobygames.com/game/365/duke-nukem-3d/)
    - [Dune II (1992)](https://www.mobygames.com/game/327/dune-ii-the-building-of-a-dynasty/)
    - [Elder Scrolls: Arena, The (1994)](https://www.mobygames.com/game/1704/the-elder-scrolls-arena/)
    - [Extreme Assault (1997)](https://www.mobygames.com/game/3165/extreme-assault/)
    - [Hexen (1995)](https://www.mobygames.com/game/383/hexen-beyond-heretic/)
    - [Might and Magic IV (1992)](https://www.mobygames.com/game/451/might-and-magic-clouds-of-xeen/)
    - [Might and Magic V (1993)](https://www.mobygames.com/game/452/might-and-magic-darkside-of-xeen/)
    - [Shadow Warrior (1997)](https://www.mobygames.com/game/1779/shadow-warrior/)
    - [WarCraft II (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/)

    </div>


## Mixer channel

Sound Canvas outputs to the **SOUNDCANVAS** [mixer
channel](../mixer.md#list-of-mixer-channels).


## Legal notes

!!! warning "Important"

    By providing an easy-to-use Roland Sound Canvas emulation feature, we aim
    to preserve an important part of DOS gaming history for all to freely
    enjoy for posterity. The feature is **only intended** for personal use
    (e.g., retro gaming, or writing music as a hobby) and research purposes.

    The Roland Sound Canvas ROM images are copyrighted by Roland Corporation,
    therefore they cannot be bundled with DOSBox.

    As per the original [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55)
    license, the use of the feature is **strictly prohibited** for creating
    commercial Roland Sound Canvas emulation hardware boxes or for use in
    commercial music production.

    Moreover, you are **not** allowed to include the bundled `Nuked-SC55.clap`
    plugin in any commercial software package without both Roland's and
    nukeykt's express permissions. If you delete the plugin from your
    commercial package, you're fine --- DOSBox Staging will continue to work,
    just without Sound Canvas support.


!!! note "License notes"

    Please see [this detailed
    description](https://github.com/dosbox-staging/dosbox-staging/pull/4090)
    on how we bridged the license incompatibility between DOSBox Staging and
    Nuked-SC55 while fully complying with both the GPL v2 and the MAME license
    and respecting nukeykt's wishes.


## Configuration settings

Sound Canvas settings are to be configured in the `[soundcanvas]` section.


##### soundcanvas_model

:   Roland Sound Canvas model to use. One or more CLAP audio plugins that
    implement the supported Sound Canvas models must be present in the
    `plugins` folder in your DOSBox installation or configuration folder.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Pick the best available model.
    - `sc55` -- Pick the best available original SC-55 model.
    - `sc55mk2` -- Pick the best available SC-55mk2 model.
    - `<version>` -- Use the exact specified model version (e.g., `sc55_121`).

    </div>


##### soundcanvas_rom_dir

:   The folder containing the Roland Sound Canvas ROMs (unset by default).
    The folder can be absolute or relative, or leave it unset to use the
    `soundcanvas-roms` folder in your DOSBox configuration folder. Other
    common system locations will be checked as well.


##### soundcanvas_filter

:   Filter for the Roland Sound Canvas audio output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output. This applies a 1st order low-pass filter at 11 kHz (`lpf 1 11000`).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.
