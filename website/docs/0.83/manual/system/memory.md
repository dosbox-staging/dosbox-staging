# Memory

This chapter covers all memory-related settings, regardless of which config
section they historically lived in: the emulated machine's total RAM, and
the DOS-level schemes (XMS, EMS, UMB) that expose memory above 640 KB to
DOS programs.

<!-- TODO: write a short "how much memory do I actually need" summary up
     front, aimed at someone who's never touched conventional/XMS/EMS/UMB
     before. -->

## Total machine memory

The default 16 MB of RAM set via [`memsize`](#memsize) is more than enough for
nearly all DOS software. A few late DOS-era games need 32 MB, but these are
rare exceptions.

## DOS memory management

The first IBM PCs only supported up to 640 KB of memory --- the so-called
"conventional memory" that DOS programs can access directly. As PCs gained more
RAM, a patchwork of standards emerged to make the extra memory available:

- **XMS** (Extended Memory) --- the main pool of memory above 1 MB. This is
  what the [`memsize`](#memsize) setting controls (16 MB by default).
  Nearly all games from the late DOS era use XMS.

- **EMS** (Expanded Memory) --- an older scheme that maps 64 KB pages of
  memory into the conventional memory area. Some early to mid-1990s games
  require EMS; a few older titles may actually malfunction if it's enabled.

- **UMB** (Upper Memory Blocks) --- small pockets of memory between 640 KB
  and 1 MB, used to free up conventional memory by loading drivers and TSRs
  into them.

All three are enabled by default and you can safely ignore them. On the rare
occasion a game misbehaves (usually an old title that chokes on EMS),
disabling the offending memory type is a quick fix.

<!-- TODO: PCjr has its own, much more constrained memory layout
     (pcjr_memory_config). That now lives in machine-types.md — consider
     a cross-reference here since it's still "memory" conceptually. -->

## Configuration settings

##### memsize

:   Amount of memory the emulated machine has in MB (`16` by default). Best
    leave at the default setting to avoid problems with some games, though a
    few games might require a higher value. There is generally no speed
    advantage when raising this value.


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
