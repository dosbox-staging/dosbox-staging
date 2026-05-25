# Graphics

Most DOS games work fine with DOSBox Staging's default graphics configuration, but a handful
of common issues come up often enough to document here. See the
[Graphics section of the manual](../graphics/adapters.md) for the full configuration reference.

---

## Screen flickering

### Late-90s games and Build engine titles

If the whole screen or the HUD flickers in a late-90s game — particularly Build engine games
like [Blood (1997)](https://www.mobygames.com/game/980/blood/),
[Duke Nukem 3D](https://www.mobygames.com/game/TODO/duke-nukem-3d/), or Shadow Warrior — try:

```ini
[dosbox]
machine = vesa_nolfb
```

This disables VESA linear framebuffer mode, which resolves flickering caused by games writing
directly to video memory in ways that conflict with the default SVGA emulation. It also fixes
the status bar or HUD not updating when picking up items, which is a related symptom.

In Blood v1.10 and later specifically, `machine = svga_paradise` may be needed instead.

### Older CGA, EGA, and early VGA games

If flickering appears in older titles, enable video memory access delay alongside a
period-accurate CPU cycle count:

```ini
[dosbox]
vmem_delay = on

[cpu]
cpu_cycles = 3000   ; adjust to match the game's era
```

`vmem_delay` simulates the delay that real hardware had when the CPU and video card both tried
to access video memory at the same time. Games that wrote graphics in tight timing loops could
produce visual tearing or flicker on faster machines — and DOSBox Staging, running at an
emulated speed faster than the game expected, can trigger the same effect.

Games known to benefit from `vmem_delay` include
[Future Wars (1989)](https://www.mobygames.com/game/TODO/future-wars/),
[Operation Stealth (1990)](https://www.mobygames.com/game/TODO/operation-stealth/),
[Quest for Glory II: Trial by Fire (1990)](https://www.mobygames.com/game/TODO/quest-for-glory-ii-trial-by-fire/),
[Hostages (1988)](https://www.mobygames.com/game/TODO/hostages/),
[Corncob 3-D (1992)](https://www.mobygames.com/game/TODO/corncob-3-d/),
[The Gold of the Aztecs (1990)](https://www.mobygames.com/game/TODO/the-gold-of-the-aztecs/),
and [Crazy Brix (1996)](https://www.mobygames.com/game/TODO/crazy-brix/).

---

## Choppy video in full-screen mode

If full-screen video is choppy or tearing, try changing the OpenGL shader in the
[`[render]` section](../graphics/rendering.md):

```ini
[render]
glshader = none
```

`glshader = sharp` is another option worth trying.

---

## VGA rendering quirk (`vga_render_per_scanline`)

A small number of games have graphical glitches — including black screens on launch — caused
by per-scanline VGA rendering behaviour that differs from original hardware timing.

[Dragon's Lair (1993)](https://www.mobygames.com/game/1503/dragons-lair/) is a documented
example: the game goes to a black screen without this fix. If you run into this kind of
rendering problem, try:

```ini
[dosbox]
vga_render_per_scanline = false
```

!!! note "TODO"
    TODO: Move/adapt the `vga_render_per_scanline` description from the 0.81.1 release notes
    into a dedicated section in the [VGA / SVGA adapters](../graphics/adapters.md) page, then
    update the link above to point there.

---

## Games that ship with a bundled UniVBE driver

UniVBE (Universal VESA BIOS Extensions) is a software driver from the 1990s that some games
shipped with to work around limitations in contemporary video card VESA implementations.
DOSBox Staging's VESA implementation is complete, so you don't need UniVBE — and the bundled
driver versions that some games include can actually cause problems: choppy framerates, crashes,
or other graphical issues.

If a game ships with a bundled `UNIVBE.DRV` file and you're seeing these symptoms, the
recommended fix is to load **NoUniVBE** before the game starts. NoUniVBE intercepts the driver
load and prevents UniVBE from initialising, passing VESA mode requests directly to DOSBox
Staging instead. See the
[UniVBE wiki page](https://github.com/dosbox-staging/dosbox-staging/wiki/UniVBE) for the
download link and setup instructions.

[Mortal Kombat Trilogy (1996)](https://www.mobygames.com/game/2036/mortal-kombat-trilogy/) is
a documented case: the game's bundled UniVBE driver cuts the framerate in half, making the
game very choppy. NoUniVBE resolves this.

[Constructor (1997)](https://www.mobygames.com/game/TODO/constructor/) has a related issue:
deleting `UNIVBE.DRV` and `UVCONFIG.DAT` from its `SETTINGS` subdirectory forces a fresh
video card detection pass and resolves a startup freeze.

---

## SVGA and VESA modes

Most games that use SVGA or VESA resolutions work correctly with the default
[`machine = svga_s3`](../graphics/adapters.md). A few edge cases worth knowing:

- **[Screamer 2 (1996) — Italian version](https://www.mobygames.com/game/TODO/screamer-2/)** —
  predates the English release and uses VESA 1.2 high-colour modes. Needs
  `machine = vesa_oldvbe` for correct high-colour display.
- **[Atlantis: The Lost Tales (1997)](https://www.mobygames.com/game/TODO/atlantis-the-lost-tales/)** —
  nearly unresponsive to mouse movement regardless of sensitivity settings; this is a
  game-specific quirk with no general graphics fix.
