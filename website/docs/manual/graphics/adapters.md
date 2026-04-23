# Graphics adapters

The [`machine`](../system/general.md#machine) setting selects which video
adapter DOSBox Staging emulates. This determines the graphics modes available
to DOS programs and, in some cases, enables additional hardware like sound
chips.

The default **`svga_s3`** ([S3 Trio64](#s3-trio64)) covers the widest range of
games, from standard VGA through high-resolution SVGA. You only need to
change it for titles that specifically require an older standard, typically
early to mid-1980s games written for [CGA](#cga), [Tandy](#tandy-1000), or
[Hercules](#hercules-graphics-card) hardware, or demoscene productions
targeting specific SVGA chipsets.

For an overview of which graphics standard was typical in each era, see
[The DOS eras](../dos-eras.md). The
[Getting Started guide](../../getting-started/enhancing-prince-of-persia.md#graphics-options)
walks through switching between adapters for a specific game.


## Hercules Graphics Card

`machine = hercules`

The **Hercules Graphics Card** (also referred to as **HGC** or just
**Hercules**), released in 1982 by Hercules Computer Technology, was the first
widely available card to combine MDA-compatible text with bitmapped graphics
on IBM PC compatibles. Its 720&times;348 monochrome graphics mode offered the
highest resolution available to standard PC monitors until VGA arrived five
years later.

Hercules was hugely popular for business software and CAD programs. Game
support was more limited --- most action games required colour --- but titles
with explicit Hercules support include Lotus 1-2-3, various text adventures,
and a number of Sierra and other titles that shipped with Hercules drivers.

DOSBox Staging emulates the Hercules with selectable monochrome phosphor
palettes (amber, green, white, paperwhite) that can be cycled with ++f11++ or
set via the
[`monochrome_palette`](rendering.md#monochrome_palette) setting.

{{ figure(
    "https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-amber.png",
    "Prince of Persia in Hercules mode using the default `amber` palette",
    small=False
) }}

<div class="image-grid" markdown>
{{ figure(
    "https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-green.png",
    "Hercules mode using the `green` palette",
    small=False
) }}
{{ figure(
    "https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-white.png",
    "Hercules mode using the `white` palette",
    small=False
) }}
</div>

!!! note

    Some software (CheckIt, QBasic, MS-DOS Editor) may detect Hercules as MDA
    and refuse to enter graphics mode. Look for a companion TSR like
    `QBHERC.COM` or `MSHERC.COM` to work around this.


## CGA and its descendants

### CGA

`machine = cga`

Introduced in 1981, IBM's **Color Graphics Adapter** or just **CGA** was the
first colour graphics option for the PC. It offered 320&times;200 at 4 colours
from fixed palettes, 640&times;200 at 2 colours, and both 40- and 80-column
colour text modes.

CGA's most distinctive feature is its **composite video output**. On NTSC
composite monitors or TVs, interference patterns in the signal produce
"artifact colours" --- up to 16 colours from what is nominally a 4-colour
mode. Sierra On-Line and other developers deliberately exploited this to
produce far more colourful graphics than the digital RGBI output could
display. See [Composite video](composite-video.md) for full details.

CGA was the baseline graphics standard that virtually all DOS games from 1981
to 1987 supported, and it remained a common fallback option well into the
early 1990s.

{{ figure(
    "https://www.dosbox-staging.org/static/images/getting-started/pop-cga.jpg",
    "Prince of Persia in CGA mode"
) }}


### Monochrome CGA

`machine = cga_mono`

Emulates a [CGA](#cga) adapter connected to a monochrome monitor. This
produces CGA video modes rendered in monochrome, with selectable phosphor
palettes just like Hercules mode. A few programs were designed for this
specific configuration.


### IBM PCjr

`machine = pcjr`

Released in 1983, the **IBM PCjr** was IBM's ambitious but ill-fated attempt
at a home computer. It extended CGA with additional video modes — most notably
16-colour 320&times;200, a significant step up from CGA's 4-colour limit — and
added a Texas Instruments SN76496 [3-voice sound
chip](../sound/sound-devices/tandy.md), the first built-in sound hardware
beyond the PC speaker on any IBM PC platform.

It was discontinued after just one year. The chiclet keyboard was universally
mocked, the price was too high, and compatibility with standard IBM PC
software was shakier than anyone wanted. Tandy Corporation saw the
opportunity, cloned the PCjr's enhanced graphics and sound into the [Tandy
1000](#tandy-1000), fixed most of what IBM got wrong, and sold it through
their Radio Shack stores nationwide in the USA. The rest is history --- the
PCjr is a footnote, the Tandy 1000 is beloved.

Setting `machine = pcjr` enables PCjr graphics, [PCjr
sound](../sound/sound-devices/tandy.md) and [composite
video](composite-video.md) emulation.


### Tandy 1000

`machine = tandy`

The Tandy 1000 (1984) by Tandy Corporation successfully cloned the PCjr's
enhanced graphics and sound into a longer-lived, more popular product line.
All CGA and PCjr video modes are supported, including 16-colour 320&times;200.
Later Tandy models with "Video II" hardware added 16-colour 640&times;200. The
built-in TI SN76496 3-voice sound chip gave Tandy machines a major audio
advantage over standard PCs.

Combined with enhanced graphics and sound, the Tandy 1000 was the best
consumer-grade DOS gaming platform of the mid-1980s. Many Sierra titles
(King's Quest, Space Quest, Leisure Suit Larry), Defender of the Crown, and
Sid Meier's Pirates! look and sound noticeably better in Tandy mode compared
to standard CGA.

The **Tandy 1000** released in 1984 by Tandy Corporation successfully cloned
the [IBM PCjr's](#ibm-pcjr) enhanced graphics and sound into a longer-lived,
more popular product line. All CGA and PCjr video modes are supported,
including 16-colour 320&times;200 --- and while Tandy graphics are technically
an extension of [CGA](#cga) rather than [EGA](#ega), the 16-colour
320&times;200 mode means most games that supported both Tandy and EGA looked
virtually identical on either platform. Later Tandy models added 16-colour
640×200. The built-in TI SN76496 [3-voice
sound](../sound/sound-devices/tandy.md) chip gave Tandy machines a major audio
advantage over standard PCs.

Combined with enhanced graphics and sound, the Tandy 1000 was the best
consumer-grade DOS gaming platform of the mid-1980s. Many Sierra titles
([King's Quest](https://www.mobygames.com/game/122/kings-quest/), [Space
Quest](https://www.mobygames.com/game/114/space-quest-chapter-i-the-sarien-encounter/),
[Leisure Suit
Larry](https://www.mobygames.com/game/379/leisure-suit-larry-in-the-land-of-the-lounge-lizards/)),
[Defender of the
Crown](https://www.mobygames.com/game/181/defender-of-the-crown/), and [Sid
Meier's Pirates!](https://www.mobygames.com/game/214/sid-meiers-pirates/) look
and sound noticeably better in Tandy mode compared to standard CGA.

Setting `machine = tandy` also enables
[Tandy sound](../sound/sound-devices/tandy.md) and
[composite video](composite-video.md) emulation.


## Composite video

The [CGA](#cga), [PCjr](#ibm-pcjr), and [Tandy](#tandy-1000) adapters all
support [composite video emulation](composite-video.md). On real hardware,
connecting these cards to an NTSC composite monitor or TV produced "artifact
colours" --- interference patterns in the analogue signal that create far more
colours than the digital output. Game developers at Sierra On-Line and
elsewhere deliberately exploited this effect.

DOSBox Staging faithfully emulates composite output for all three adapters,
including adjustable hue, saturation, contrast, and brightness. This is
essential for playing early Sierra AGI games ([King's
Quest](https://www.mobygames.com/game/122/kings-quest/), [Space
Quest](https://www.mobygames.com/game/114/space-quest-chapter-i-the-sarien-encounter/))
as the artists intended. See [Composite video](composite-video.md) for
configuration details and hotkeys.


## EGA

`machine = ega`

IBM's **Enhanced Graphics Adapter** or **EGA** released in 1984 was a major
step forward: 16 simultaneous colours from a 64-colour palette at up to
640&times;350 resolution. DOSBox emulates EGA with a fixed 256 KB of video
memory.

EGA was the dominant PC graphics standard from roughly 1985 to 1990. Most
celebrated games of the period, including
[Loom](https://www.mobygames.com/game/176/loom/), [The Secret of Monkey
Island](https://www.mobygames.com/game/616/the-secret-of-monkey-island/), and
the early Sierra adventures, ran in 16-colour 320&times;200, a resolution
shared equally by [EGA](#ega) and [Tandy](#tandy-1000). EGA's higher 640×350
resolution was used mainly by strategy titles, simulations, and illustrated
text adventures that benefited from the extra detail. Legend Entertainment's
[Spellcasting
101](https://www.mobygames.com/game/1027/spellcasting-101-sorcerers-get-all-the-girls/)
and [Timequest]() are good examples of games built specifically around it.
Most games used [dithering](rendering.md#dedithering) creatively to simulate
more colours than the 16-colour palette could display directly.

!!! note

    EGA is *not* backward compatible with Hercules/MDA text modes.
    Games expecting CGA monitor behaviour on an EGA card may also
    show differences, as DOSBox emulates the EGA with an EGA-class monitor.

{{ figure(
    "https://www.dosbox-staging.org/static/images/getting-started/pop-ega.jpg",
    "Prince of Persia in EGA mode"
) }}


## VGA and SVGA

IBM's **Video Graphics Array** or **VGA** from 1987 was the last IBM graphics
standard that PC clone manufacturers widely adopted. VGA
introduced the iconic Mode 13h --- 256 colours from a palette of 262,144 at
320&times;200 --- which became *the* standard DOS gaming mode from the late
1980s through the mid-1990s. VGA also introduced analog video output,
replacing the digital RGBI output of earlier standards.

There is no dedicated VGA-only machine type in DOSBox Staging. All SVGA
adapters below are fully backward-compatible with VGA (and by extension CGA
and EGA), so any of them will work for VGA games. The default `svga_s3` is
the best general-purpose choice.

!!! note

    A small number of games and demos rely on per-scanline VGA timing tricks
    that change graphics settings mid-frame (e.g.,
    [Lemmings](https://www.mobygames.com/game/683/lemmings/), [Pinball
    Fantasies](https://www.mobygames.com/game/571/pinball-fantasies/), [Alien
    Carnage](https://www.mobygames.com/game/522/alien-carnage/)). These may
    need the `svga_paradise` machine type, which is the closest to IBM's
    original VGA implementation. See also
    [`vga_render_per_scanline`](../system/general.md#vga_render_per_scanline).

**SVGA** (Super VGA) is a loose term for any VGA superset offering higher
resolutions and colour depths. Because each manufacturer implemented their own
extensions, the VESA BIOS Extensions (VBE) standard was created to provide a
uniform programming interface. DOSBox Staging emulates four SVGA chipsets,
ranging from late 1980s designs to the mid-1990s S3 Trio64.

{{ figure(
    "https://www.dosbox-staging.org/static/images/getting-started/pop-vga.jpg",
    "Prince of Persia in VGA mode --- VGA adapters line-double 200-line modes, resulting in \"chunky\" looking pixels"
) }}


### Paradise PVGA1A

`machine = svga_paradise`

The **Paradise PVGA1A** (1988) was an early SVGA chipset from Western Digital,
and the closest thing DOSBox Staging offers to a plain IBM VGA adapter. It has
no VESA VBE support and defaults to 512 KB of video memory (configurable to
256 KB or 1 MB via [`vmemsize`](../system/general.md#vmemsize)).

Use this for games or demos that expect strict VGA behaviour with no SVGA
extensions --- particularly titles that break or behave unexpectedly on more
capable adapters. It's also the recommended choice for the handful of games
that rely on per-scanline VGA register tricks, as it most faithfully
replicates the original IBM VGA implementation.


### Tseng Labs ET3000

`machine = svga_et3000`

The **Tseng Labs ET3000** (1987) was one of the earliest SVGA chipsets,
predating even the [Paradise PVGA1A](#paradise-pvga1a). Its extended modes are
limited by today's standards --- 800×600 at 16 colours and 640×480 at 256
colours --- and it has no VESA VBE support and a fixed 512 KB of video memory.
Despite its limitations, the ET3000 was fast for its time and found a
following in the demoscene.

This is a niche option, useful mainly for demoscene productions or the
occasional title written specifically for this chipset.


### Tseng Labs ET4000

`machine = svga_et4000`

The **Tseng Labs ET4000** (1989) was the successor to the ET3000 and one of the
most popular SVGA chipsets of the early 1990s — widely adopted,
well-supported, and significantly more capable than its predecessor. It offers
a wider range of extended modes and defaults to 1 MB of video memory
(configurable to 256 KB or 512 KB via
[`vmemsize`](../system/general.md#vmemsize)). No VESA VBE is supported on this
card.

The ET4000's popularity made it a common target for demoscene productions of
the era, and a handful of games were written with it specifically in mind. If
a game or demo explicitly lists ET4000 support, this is your machine type.


### S3 Trio64

`machine = svga_s3` *default*{ .default }

The **S3 Trio64** fro 1994 is the default machine type and the most capable
adapter in DOSBox Staging. By the mid-1990s, S3 cards were ubiquitous, and the
Trio64 represents the peak of the DOS-era SVGA landscape. It provides full
VESA VBE 2.0 support with a linear framebuffer, defaults to 4 MB of video
memory (configurable from 512 KB to 8 MB via
[`vmemsize`](../system/general.md#vmemsize)), and handles everything from
standard VGA up to high-resolution SVGA modes including 1600×1200.

This is the right choice for the vast majority of DOS games — if you're not
sure which adapter to use, stay here. Use the
[`vesa_modes`](../system/general.md#vesa_modes) setting to control which SVGA
modes are exposed to software.

!!! note

    The S3 Trio64 is *not* compatible with Tandy, PCjr, or Hercules graphics.
    Games requiring these standards need the corresponding `machine` type.

!!! tip

    Some games and GOG releases ship with **UniVBE** (Universal VESA BIOS
    Extensions), a third-party VESA driver from the 1990s. DOSBox Staging's
    built-in VESA implementation is fully compliant, making UniVBE
    unnecessary --- and it can actually cause graphical glitches. If a game
    has trouble with SVGA modes, check for a bundled `UNIVBE.EXE` or
    `UVCONFIG.EXE` and try preventing it from loading (remove the line from
    the game's batch file, or rename the file).

Two variants are available for specific compatibility needs:

- `machine = vesa_oldvbe` --- Same S3 Trio64, but limited to VESA VBE 1.2.
  Use for games that malfunction with VBE 2.0.
- `machine = vesa_nolfb` --- S3 Trio64 with VESA VBE 2.0 but without the
  linear framebuffer hack. Needed by a small number of titles.


## Summary

<div class="compact" markdown>

| `machine` value    | Adapter                                          | Year   | Max colours                   | Max resolution           |
| ------------------ | ------------------------------------------------ | ------ | ----------------------        | ------------------------ |
| `hercules`         | Hercules HGC                                     | 1982   | 2 (monochrome)                | 720&times;348            |
| `cga_mono`         | CGA (monochrome&nbsp;monitor)                    | 1981   | 16 (monochrome)               | 640&times;200            |
| `cga`              | CGA                                              | 1981   | 4 (16&nbsp;in&nbsp;composite) | 640&times;200            |
| `pcjr`             | PCjr                                             | 1983   | 16                            | 640&times;200            |
| `tandy`            | Tandy 1000                                       | 1984   | 16                            | 640&times;200            |
| `ega`              | EGA                                              | 1984   | 16                            | 640&times;350            |
| `svga_paradise`    | Paradise PVGA1A                                  | 1988   | 256                           | 1024&times;768           |
| `svga_et3000`      | Tseng Labs ET3000                                | 1987   | 256                           | 1024&times;768           |
| `svga_et4000`      | Tseng Labs ET4000                                | 1989   | 65K                           | 1280&times;1024          |
| `svga_s3`          | S3 Trio64, VBE 2.0 *default*{ .default }         | 1994   | 16.7M                         | 1600&times;1200          |
| `vesa_oldvbe`      | S3 Trio64, VBE 1.2                               | 1994   | 16.7M                         | 1600&times;1200          |
| `vesa_nolfb`       | S3 Trio64, VBE 2.0 (no&nbsp;LFB)                 | 1994   | 16.7M                         | 1600&times;1200          |

</div>


## 3D and video accelerators

DOSBox Staging also emulates two add-on cards that work alongside the primary
video adapter:

- **[3dfx Voodoo](3dfx-voodoo.md)** --- The iconic 3D accelerator released in
  1996.

- **[ReelMagic](reelmagic.md)** --- Sigma Designs MPEG-1 hardware decoder
  from 1993 for full-motion video in a handful of DOS titles.

These are independent of the [machine](../system/general.md#machine) setting
and can be used with any adapter.
