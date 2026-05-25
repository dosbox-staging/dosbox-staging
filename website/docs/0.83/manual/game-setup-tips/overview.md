# Game setup tips

## Why is setting up DOS games so complicated?

If you've just come from console gaming --- or even modern PC gaming --- the
amount of configuration that DOS games sometimes need can feel baffling. You
found a game, you want to play it, and instead you're staring at settings for
IRQ numbers and CPU cycles. What gives?

The answer is that the DOS PC was never a fixed platform. Unlike a console,
where every machine is identical and every game is guaranteed to run on it,
the DOS-era PC was a sprawling ecosystem of competing hardware from hundreds
of manufacturers. Two "IBM PC compatible" machines sitting side by side in
1993 might have completely different sound cards, different amounts of RAM,
different CPU speeds, and different video adapters --- and games had to try to
support all of them. Developers couldn't rely on a standard; they had to
probe, detect, and guess what hardware was present, and hope for the best.

This meant that DOS games shipped with setup utilities that required you to
know your own hardware. What IRQ is your sound card on? Which DMA channel? Is
it a Sound Blaster, a Sound Blaster Pro, or an Ad Lib? If you answered wrong,
the game would either crash or play no sound at all. This was normal. It was
the wild west, and players were expected to know their machines.

On top of that, the DOS era spanned about fifteen years of rapidly evolving
hardware generations --- from the original 4.77 MHz 8088 up through Pentiums
running at hundreds of megahertz. Games written for a 286 could behave
unpredictably on a fast 486 because they assumed a certain speed. Memory
management was a patchwork of overlapping standards that conflicted with each
other. Sound hardware was fractured across dozens of incompatible cards and
driver APIs. Every generation of video hardware introduced new modes and new
quirks for developers to work around.

DOSBox Staging emulates a DOS PC faithfully, which means it inherits all of
this complexity. Most of the time the defaults work fine, but some games need
you to meet them where they were: on specific hardware, at a specific speed,
with specific settings. The pages in this section collect the most common
things you'll need to know, pointing to the relevant reference documentation
along the way.

!!! tip "New to DOSBox Staging?"

    The [Getting Started guide](../../getting-started/introduction.md) walks
    through real-world game configuration in detail. The [DOS
    primer](../introduction/dos-primer.md) and [DOS
    eras](../introduction/dos-eras.md) pages explain the historical context in
    depth. The [Game issues wiki
    page](https://github.com/dosbox-staging/dosbox-staging/wiki/Game-issues)
    documents known problems and fixes for specific titles.

## Use per-game configs

One of the most important habits to develop is giving each game its own [local
configuration
file](../using-dosbox-staging/configuration.md#local-configuration). Rather
than editing the global `dosbox-staging.conf` for every game you set up, put a
`dosbox-staging.conf` file directly in the game's directory. DOSBox Staging
picks it up automatically when you launch from that directory, and the
settings apply only to that game. This keeps your global config clean, makes
it easy to share configs with other players, and means a cycle setting you
tweak for one game won't silently break another.

The [Getting Started guide](../../getting-started/introduction.md) shows this
pattern in action for every example game it covers.

## In this section

- **[CPU & Memory](cpu-and-memory.md)** --- Emulated CPU speed, memory types
  (EMS/XMS/UMB), DOS extenders, disk space, and installers.

- **[Sound](sound.md)** --- Sound card setup, MIDI, MT-32, FluidSynth, CD audio,
  and games that require EMS for audio.

- **[Graphics](graphics.md)** --- Screen flickering, VESA modes, VGA rendering
  quirks, and display settings.

- **[Mouse](mouse.md)** --- Sensitivity, button count, mouse drivers, and movement quirks.

- **[Troubleshooting](troubleshooting.md)** --- Problem/solution reference, with
  game examples.
