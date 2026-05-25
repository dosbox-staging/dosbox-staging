# Troubleshooting

A problem/solution reference for common issues. Each entry includes examples drawn from
real games. For game-specific fixes not covered here, see the
[Game Issues wiki](https://github.com/dosbox-staging/dosbox-staging/wiki/Game-issues).

!!! tip
    Before assuming an emulation problem, check whether the issue exists on real hardware.
    Many quirks people attribute to DOSBox Staging are original game bugs.
    [VOGONS](https://www.vogons.org) and [PCGamingWiki](https://www.pcgamingwiki.com) are
    good references for this.

---

## Game crashes immediately on launch

Start with the most common culprits before digging into specifics:

- Try [`core = normal`](../system/cpu.md#core) in `[cpu]` as a baseline. Use `normal` before
  trying `dynamic` or `auto`.
- Check whether the game needs a specific DOS version. Try `ver set 3.1` in `[autoexec]`.
- Try [`loadfix GAME.EXE`](cpu-and-memory.md#loadfix) — some games crash when too much
  conventional memory is free.
- A **divide-by-zero** error at startup usually means the game did a CPU speed measurement and
  got an impossible result. Lower [`cpu_cycles`](cpu-and-memory.md#cpu-speed) or use
  `loadfix`.
- A crash specifically at the **sound setup screen** may be the CauseWay DOS extender bug
  (version 3.14). Try launching the sound setup executable separately, or use
  `DPMILD32.EXE SETUP.EXE` from the HX extender package as a wrapper. This affects Extreme
  Assault, [Mortal Kombat Trilogy (1996)](https://www.mobygames.com/game/2036/mortal-kombat-trilogy/),
  and Network Q RAC Rally Championship.

## Game runs too fast or too slow

- Adjust [`cpu_cycles`](cpu-and-memory.md#cpu-speed). Real-mode games: 1,000–8,000.
  Protected-mode: 30,000–200,000.
- Use `cpu_cycles` and `cpu_cycles_protected` separately if the game switches modes between
  menus and gameplay.
- If the game behaves differently each run, it's probably locking its timing to a CPU speed
  measurement taken at startup. Keep cycles stable during the first few seconds.

*Examples:* [Duke Nukem II (1993)](https://www.mobygames.com/game/1009/duke-nukem-ii/) runs
too fast at default cycles and needs `cpu_cycles = 4500`.
[Wing Commander (1990)](https://www.mobygames.com/game/3/wing-commander/) needs
`cpu_cycles = 3500`.
[Flashback (1992)](https://www.mobygames.com/game/555/flashback-the-quest-for-identity/)
must be *started* at `cpu_cycles = 6500` — raising cycles while the game is already running
has no effect on its timing.

## No sound or music

- Run the game's **sound setup utility** and configure it to port `220`, IRQ `7`, DMA `1`.
  This is the [most common reason for silence](sound.md#run-the-games-sound-setup-utility) and
  the first thing to check.
- Check whether the game requires [EMS](sound.md#games-that-require-ems) — many games silently
  skip audio without it rather than showing an error.
- If **music plays but effects don't** (or vice versa), the game usually has separate setup
  entries for music and effects. Configure both.
- Try a different [`sbtype`](sound.md#sound-blaster-model) — older games often work better
  with `sb1` or `sb2` than `sb16`.

*Examples:* [Doom (1993)](https://www.mobygames.com/game/TODO/doom/) and Doom II produce no
audio until you run `SETUP.EXE` and select Sound Blaster with the correct port, IRQ, and DMA
values. 1942: The Pacific Air War requires the third-party JEMM memory manager loaded before
the game to get sound working — DOSBox Staging's built-in EMS support is not enough for this
one.

## Sound or music sounds wrong

- Check that DOSBox Staging's MIDI output type matches what the game is configured for. General
  MIDI and MT-32 are completely different and are not interchangeable. See
  [MIDI music](sound.md#midi-music).
- If using MT-32 emulation, check the [ROM version](sound.md#mt-32-rom-versions). Late-80s
  and early-90s Sierra titles almost always need old ROMs and will crash or sound wrong with
  new ROMs.
- If using FluidSynth, try a more comprehensive GM soundfont to avoid
  [Capital Tone Fallback](sound.md#fluidsynth-and-capital-tone-fallback) producing substitute
  instruments.

*Examples:* [Heroes of Might & Magic (1995)](https://www.mobygames.com/game/TODO/heroes-of-might-and-magic/)
plays music at 8-bit depth by default, producing a noticeable hiss. A hex edit to `HEROES.EXE`
enables 16-bit audio.
[Might and Magic: World of Xeen](https://www.mobygames.com/game/TODO/might-and-magic-world-of-xeen/)'s
MT-32 driver crashes the game after a few minutes, so General MIDI must be used despite the
music having been composed for MT-32.

## Game freezes with General MIDI selected

This is almost always a [game bug in the MPU-401 MIDI driver](sound.md#game-freezes-on-startup-with-general-midi-selected),
not an emulation issue. Switch to Sound Blaster for music.

*Examples:* [Hi-Octane (1995)](https://www.mobygames.com/game/2208/hi-octane/) freezes right
after the introduction.
[Privateer 2: The Darkening (1996)](https://www.mobygames.com/game/363/privateer-2-the-darkening/)
freezes during the main intro FMV. Both have the same root cause.

## Audio is choppy or stutters

- Adjust [`cpu_cycles`](cpu-and-memory.md#cpu-speed) — both too-low and too-high values can
  cause audio problems.
- If choppy audio only happens during cutscenes or FMVs, raise cycles — the video decoder
  needs more headroom.
- If using DOS/32A, try reverting to DOS/4GW. See
  [DOS extenders](cpu-and-memory.md#dos-extenders).

*Examples:*
[Beneath a Steel Sky (1994)](https://www.mobygames.com/game/386/beneath-a-steel-sky/)'s intro
stutters at the wrong cycle count — it needs `cpu_cycles_protected = 4000`.
[Chasm: The Rift (1997)](https://www.mobygames.com/game/TODO/chasm-the-rift/) has in-game
sound stuttering that's fixed by changing the video resolution to 640×480 in the game's own
options menu.
[Arctic Baron / Transarctica (1993)](https://www.mobygames.com/game/3932/arctic-baron/) uses
the Sound Blaster DAC mode; if cycles are too low, sound effects come out choppy. About 5,000
cycles is the recommended starting point.

## Screen flickering or graphical corruption

- Try `machine = vesa_nolfb` for late-90s and Build engine games. See
  [Graphics — Screen flickering](graphics.md#screen-flickering).
- Try `vmem_delay = on` with a low `cpu_cycles` for older CGA/EGA/VGA titles.
- If the **HUD or status bar isn't updating** when picking up items, `machine = vesa_nolfb`
  fixes this too.
- For black screen on launch, try
  [`vga_render_per_scanline = false`](graphics.md#vga-rendering).

*Examples:* Blood's status bar stops updating in VESA resolutions above 340×200 — fixed with
`machine = vesa_nolfb`.
[Dragon's Lair (1993)](https://www.mobygames.com/game/1503/dragons-lair/) goes to a black
screen — fixed with `vga_render_per_scanline = false`.

## Game doesn't detect the CD-ROM

- Mount the disc image **before** launching the game executable.
- Mount the `.cue` file, not just the `.iso`. See
  [CD audio](sound.md#cd-audio) on the Sound page.
- Some games write the expected CD drive letter to a config file during installation. If
  installation assumed drive D but you mount on E, the game won't find it. Check the game's
  `.ini`, `.cfg`, or batch file for a hardcoded drive letter.

*Examples:* [Star Wars: Dark Forces (1995)](https://www.mobygames.com/game/TODO/star-wars-dark-forces/)
scans for the CD on startup and won't proceed without it — the game folder must be mounted as a
CD-ROM drive before launch.
[Lands of Lore: The Throne of Chaos (1993)](https://www.mobygames.com/game/TODO/lands-of-lore-the-throne-of-chaos/)
(GOG release) stores the disc data as a file called `game.dat`, which must be mounted as a
CD-ROM image despite the non-obvious filename.

## Installer crashes or hangs

- Set `cpu_cycles = max` before running the installer. See
  [Slow installers](cpu-and-memory.md#slow-installers).
- Add [`-freesize`](cpu-and-memory.md#disk-space-reported-to-the-game) to your mount command
  if the installer complains about disk space.
- Some installers crash on a **CD transfer speed test** when an ISO image returns impossibly
  fast read results. Look for a `/c` flag or equivalent to skip the test. Retribution (1994)
  supports `install /c` for this reason. Down in the Dumps has a divide-by-zero on install due
  to the same cause and requires a workaround of copying the required game files manually.
- A crash at the sound setup screen may be the CauseWay extender bug — see
  "Game crashes immediately" above.

## Mouse feels wrong or game won't respond to it

- Too fast or slow: adjust [`mouse_sensitivity`](mouse.md#mouse-sensitivity).
- Erratic movement: try `mouse_raw_input = off`. See
  [Erratic or inconsistent movement](mouse.md#erratic-or-inconsistent-movement).
- Sticky movement: try `builtin_dos_mouse_driver_move_threshold = 2`. See
  [Stuck movement at very low speeds](mouse.md#stuck-movement-at-very-low-speeds).
- Game exits at startup: it may need a [3-button mouse](mouse.md#three-button).
- No response at all: try `builtin_dos_mouse_driver = off`. See
  [Game requires its own DOS mouse driver](mouse.md#game-requires-its-own-dos-mouse-driver).

## Copy protection is triggering

- Always mount the disc image before launching. Don't use Daemon Tools or similar tools
  alongside DOSBox Staging.
- For **LaserLock** protection, replace DOS/4GW with DOS/32A. See
  [DOS extenders](cpu-and-memory.md#dos-extenders).
- Some games write a copy-protection **counter** to a save file that increments each run and
  eventually triggers a lockout.
  [Lemmings (1991)](https://www.mobygames.com/game/TODO/lemmings/) stores this counter in
  `RUSSELL.DAT`. Making the file read-only after a clean install prevents the counter from
  incrementing.

## File access or saves misbehave

Toggle [`file_locking`](sound.md#file-locking) in `[dos]`. See the Sound page for context.

*Examples:* Indiana Jones and His Desktop Adventures (Win 3.1) throws a "Sharing violation"
error — fixed with `file_locking = off`.

## General checklist

If you're stuck and don't know where to start:

1. Use a [per-game local config](overview.md#use-per-game-configs) so your changes are
   isolated to this game.
2. Try `core = normal` in `[cpu]`.
3. Reset [memory settings](cpu-and-memory.md#memory) to defaults, then restrict them one at a
   time.
4. Run the game's [sound setup utility](sound.md#run-the-games-sound-setup-utility) if you
   haven't already.
5. Check the [Game Issues wiki](https://github.com/dosbox-staging/dosbox-staging/wiki/Game-issues)
   for a documented fix.
6. Search [VOGONS](https://www.vogons.org) to check whether the problem exists on real hardware.
