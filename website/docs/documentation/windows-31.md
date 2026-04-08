# Running Windows 3.1

DOSBox Staging fully supports Windows 3.1, which was not a standalone
operating system but an operating environment running on top of DOS. A
number of games were released exclusively for Windows 3.1 --- see
[The DOS eras](dos-eras.md#windows-31-games) for notable titles and
recommended hardware settings.

!!! warning

    Windows 9x (95, 98, ME) is **not** supported by DOSBox Staging.


## Installation

You'll need Windows 3.1 (or Windows for Workgroups 3.11) installation media
--- either as floppy disk images or files extracted from them.

- Mount your installation files so they're accessible from DOS (e.g., copy
  them to `C:\INSTALL`).
- Run `SETUP` from the installation directory.
- Install into `C:\WINDOWS`.
- When prompted, choose **386 Enhanced** mode.
- After installation completes, launch Windows with the `WIN` command.

Use the following DOSBox configuration:

``` ini
[dosbox]
machine = svga_s3
memsize = 16

[cpu]
cpu_cycles = 30000
cputype = pentium

[sblaster]
sbtype = sb16

[midi]
mididevice = soundcanvas
```

Setting `cputype` to `pentium` is important --- some Windows 3.1 games
(e.g., [Betrayal in Antara](https://www.mobygames.com/game/1763/betrayal-in-antara/)) require it. Give Windows at least 16 MB of RAM.

!!! tip

    Adding `WIN :` (note the space and colon) to your `[autoexec]` section
    launches Windows automatically and skips the startup logo.


## Video driver

Install the
[S3 Vision 964 v1.41B5](https://github.com/dosbox-staging/dosbox-staging/files/11050083/s3-964-141B5.ZIP)
driver for proper SVGA support inside Windows.

- Extract the ZIP to a directory accessible from DOS (e.g., `C:\DRIVERS\S3`).
- Exit Windows and run `SETUP` from the DOS prompt.
- Navigate to the **Display** section and select **Other**.
- Point to the driver directory and choose **640&times;480, 256 colours** as
  a safe starting point.
- Higher resolutions up to 1600&times;1200 are available if you set
  [`vmemsize`](system/general.md#vmemsize) to `4` or higher.

!!! note

    Always install the video driver *after* the initial Windows
    installation, not during it. Back up your `C:\WINDOWS` directory before
    changing display drivers.


## Audio driver

Install the
[Sound Blaster 16 driver](https://github.com/dosbox-staging/dosbox-staging/files/11039394/SB16W3X.ZIP)
for digital audio and OPL music inside Windows.

- Extract to `C:\DRIVERS\SB16` and run `INSTALL` from within Windows.
- Choose **Full Installation**.
- When prompted for hardware settings, use DOSBox's defaults:
    - Base address: **220**
    - IRQ: **7**
    - DMA: **1**
    - High DMA: **5**
- After installation, delete the `CONFIG.SYS` and `AUTOEXEC.BAT` files the
  installer generates --- they are not needed under DOSBox.
- Test by playing `C:\WINDOWS\CANYON.MID` in Media Player.


## MIDI setup

After installing the Sound Blaster 16 driver, configure MIDI output:

- Open **Control Panel &rarr; MIDI Mapper**.
- Select **SB16 All MIDI** as the active setup.

This routes MIDI output through DOSBox's configured
[`mididevice`](sound/sound-devices/midi.md#mididevice), so you can use
FluidSynth, Sound Canvas emulation, or MT-32 emulation for Windows 3.1
games --- just change the `mididevice` setting in your DOSBox config.


## Mouse setup

The built-in PS/2 mouse driver that Windows installs by default works fine
and requires no additional configuration.

The two-button mouse without a scroll wheel was the standard throughout the
DOS era --- scroll wheels only became common in the late 1990s. DOSBox
emulates a two-button mouse by default, which is the safest setting. Only a
small number of early-to-mid 90s Windows games support a third mouse button;
if you need it, enable three-button mouse emulation via the
[`dos_mouse_driver`](input/mouse.md) setting.

For seamless mouse integration (the pointer moves freely between the DOSBox
window and your desktop without needing to capture/release it), install the
[VirtualBox mouse driver](https://git.javispedro.com/cgit/vbmouse.git/about/).


## Further reading

For the full walkthrough with screenshots, driver troubleshooting, and
optional add-ons (Video for Windows, Win32s, QuickTime), see the
[Windows](https://github.com/dosbox-staging/dosbox-staging/wiki/Windows)
wiki page.
