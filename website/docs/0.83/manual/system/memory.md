# Memory

Memory management has a bad reputation in DOS, and for good reason. The
original IBM PC could only give programs 640 KB to work with — the rest of
its addressable memory was reserved for video cards and system firmware.
As PCs gained more RAM over the following decade, several competing,
overlapping standards emerged to make that extra memory usable, and DOS
software from different years picked different ones. A DOS PC from the
early 90s often had to support all of them at once just to run everything
in a typical game library.

The good news: DOSBox Staging enables all of these by default, at sizes
that suit almost every game, so in practice you rarely need to think about
any of this. The rest of this chapter explains what each setting does and
covers the exceptions.


## Total machine memory

The default 16 MB of RAM, set via [`memsize`](#memsize), covers nearly all DOS
software. A handful of 1996--97 games ---
[Quake](https://www.mobygames.com/game/374/quake/), [Duke Nukem
3D](https://www.mobygames.com/game/365/duke-nukem-3d/), [Tomb
Raider](https://www.mobygames.com/game/348/tomb-raider/), [Descent
II](https://www.mobygames.com/game/694/descent-ii/) --- were built for
machines with more RAM and run better with `memsize = 32`. There's no benefit
to raising it beyond what a specific game needs — it doesn't make the emulator
faster, and some older games actually break if they see more memory than they
expect.


## DOS memory management

DOSBox Staging emulates the three schemes DOS software used to reach past the
640 KB conventional memory limit, and enables all of them by default:

- **XMS** (Extended Memory) — the main pool of memory above 1 MB, sized by
  [`memsize`](#memsize). Nearly all games from the late DOS era use XMS.

- **EMS** (Expanded Memory) — an older scheme that maps 64 KB pages of
  memory into the conventional memory area. Some early to mid-1990s games
  require EMS; a few older titles may actually malfunction if it's enabled.

- **UMB** (Upper Memory Blocks) — small pockets of memory between 640 KB
  and 1 MB, used to free up conventional memory by loading drivers and TSRs
  (resident programs) into them.

You can safely leave all three on. On the rare occasion a game misbehaves
(usually an old title that chokes on EMS), disabling the offending memory
type via [`ems`](#ems), [`xms`](#xms), or [`umb`](#umb) is a quick fix.

!!! note "PCjr machines"

    The IBM PCjr uses its own, much more constrained memory layout, and has
    a dedicated [`pcjr_memory_config`](machine-types.md#pcjr_memory_config)
    setting (`expanded` by default, or `standard` for the legacy behaviour
    needed by a handful of titles). See [Machine types](machine-types.md)
    for details.


## Configuration settings

### Total machine memory

This setting is configured in the `[dosbox]` section.

##### memsize
:   Amount of memory the emulated machine has in MB (`16` by default). Best
    leave at the default setting to avoid problems with some games, though a
    few games might require a higher value. There is generally no speed
    advantage when raising this value.


### DOS memory management

These settings are configured in the `[dos]` section.

##### ems
:   Enable EMS support. Enabled provides the best compatibility but certain
    applications may run better with other choices, or require EMS support to
    be disabled to work at all.
    Possible values: `on` *default*{ .default }, `off`

##### xms
:   Enable XMS memory support.
    Possible values: `on` *default*{ .default }, `off`

##### umb
:   Enable UMB memory support.
    Possible values: `on` *default*{ .default }, `off`
