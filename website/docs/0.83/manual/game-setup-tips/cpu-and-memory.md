# CPU & memory

## CPU speed

Getting the emulated CPU speed right is the single most important thing you'll
need to tweak. It affects not just how fast a game runs, but whether it
correctly detects your sound card, whether its timing logic works, and whether
its installer will complete in a reasonable amount of time.

DOSBox Staging measures emulated speed in **cycles**. See the [CPU
section](../system/cpu.md) for the a detailed explanation. For game setup
purposes:

- **Real-mode games** (generally pre-1993, EGA and early VGA) typically need
  3,000–6,000 cycles.

- **Protected-mode games** (generally post-1993, using DOS extenders like
  DOS/4GW) typically run well at 25,000–100,000 cycles.

- Set these independently with [`cpu_cycles`](../system/cpu.md#cpu_cycles) and
  [`cpu_cycles_protected`](../system/cpu.md#cpu_cycles_protected) in your
  config.

Use ++ctrl+f11++ / ++ctrl+f12++ (++cmd+f11++ / ++cmd+f12++ on macOS) to
decrease/increase cycles on the fly while a game is running, which is the
fastest way to find the right ballpark before committing to a config value.


### Sound detection failures

Many games probe the CPU speed at startup and use the result to detect audio
hardware. If the cycles value is too high during that probe, the game can
miscalibrate and fail to find the sound card at all, even if everything else
is configured correctly. This usually manifests in the game quitting with a
"integer divide by zero" error message.

[The Secret of Monkey
Island](https://www.mobygames.com/game/616/the-secret-of-monkey-island/) and
other early LucasArts titles need cycles set no higher than around 6,000 for
sound detection to succeed. You can raise the cycles setting via the hotkey
after the game has started if needed, since the sound card is only probed once
at launch.

### Speed measurement at startup

Some games measure the CPU speed once at startup to calibrate timing,
animation, or physics for the entire session. For these games, you need a
stable cycle count during the first few seconds --- switching cycles while the
intro plays can produce subtly wrong behaviour throughout the game.

[Transarctica (Arctic
Baron)](https://www.mobygames.com/game/3932/arctic-baron/) is such a title: it measures performance before the first logo appears and uses that
figure as a timing divider. An unstable reading will also affect which sound
output mode the game selects (Adlib only, Adlib + SFX, or SFX only), so
getting cycles right from the start matters for audio as well.


### Slow installers

Real mode installers and patch utilities can take a long time to finish at the
default 3,000 cycles. Before running a game's installer, set `cpu_cycles =
max` (or run `cpu_cycles max` from the DOS prompt), then revert afterwards.
Note that this might cause some timing-sensitive installer to crash or
misbehae, but it's worth a try.


## CPU core and type

See [`core`](../system/cpu.md#core) and [`cputype`](../system/cpu.md#cputype)
in the CPU reference for full details.

**Compatibility issues:** If a game crashes, hangs, or behaves unexpectedly,
try `core = normal` as a baseline before switching to `dynamic`.

**Older CPU type required:** Some games need an older CPU type to function. A
crash at startup with a divide-by-zero or integer error can be a sign of this.
Try `cputype = 386_prefetch`. Games that require this include the DOS port of
Contra, FIFA International Soccer (1994), The Terminator (1991), and X-Men:
Madness in Murderworld.


## Memory

DOS [memory management](../system/dos.md#memory-management) uses several
overlapping standards --- conventional memory, XMS (extended memory), EMS
(expanded memory), and UMB configured in the [`[dos]`
section](../system/dos.md). Getting the wrong combination is a common
cause of games failing to start or not having audio output.

The most frequent patterns:

- **Game requires EMS** --- refuses to start or loses audio without expanded
  memory. See the [Sound page](sound.md#games-that-require-ems) for the full
  list of known games. Fix: `xms = false` and `ems = true`.

- **Game conflicts with XMS or UMB** --- crashes or misbehaves without obvious cause. Try
  disabling these one at a time.

- A useful starting point for stubborn games: `xms = false`, `ems = true`, `umb = false`.


### Loadfix

The [`loadfix`](../using-dosbox-staging/commands.md#loadfix) command allocates
a block of conventional memory before launching a program. Some games that
assume certain fixed memory layouts crash when "too much" conventional memory
is available --- they were written for machines with less RAM than DOSBox
Staging provides by default, and their startup logic gets confused. Running
`loadfix GAME.EXE` works around this. The default allocation is 64 KB, but you
can specify a different size, e.g., `loadfix -25 GAME.EXE`.

Games known to benefit from `loadfix` include [SimCity
2000](https://www.mobygames.com/game/TODO/simcity-2000/), [MechWarrior 1](), and
[Cyber Riders]().


## Free disk space

Some game installers check reported free disk space and refuse to proceed if
they see less than they expect. Use the
[`-freesize`](../using-dosbox-staging/storage.md) parameter when mounting your
C drive:

```
mount c /path/to/games -freesize 1024
```

The number is in megabytes. [Fallout
(1997)](https://www.mobygames.com/game/TODO/fallout/)'s "Humongous
Installation" needs at least 661 MB reported. [Prince of Persia
(1990)](https://www.mobygames.com/game/TODO/prince-of-persia/)'s v1.0 and v1.1
installers actually *crash* if reported free space is 64 MB or more --- use a
value like 50 for those versions.


## DOS version

Some very old games crash on DOS versions newer than 3.1. Set the reported DOS
version in your `[autoexec]` section:

```ini
[autoexec]
ver set 3.1
```

[JetFighter: The Adventure](https://www.mobygames.com/game/5214/jetfighter-the-adventure/) is a
documented case --- it crashes immediately after the title screen with an
"Integer divide by 0" error if the DOS version is higher than 3.1.


