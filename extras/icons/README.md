# Icon guide

## Overview

This directory contains the source and the derived icon files used by the
application and the installer for our supported platforms (Windows, macOS, and
Linux. The derived files are also checked in, so they should only be
regenerated with the `make` command when making changes to the source icon's
design (more on that below).

The subfolders contain the following files:

- `macos` -- The 1024px source PNG file and the derived `.icns` file to be
   used in the macOS app bundle.

- `old` -- The old DOSBox icons (unused in our packages, just for archival
   purposes).

- `png` -- Small PNG icons derived from the SVG source files. These are used
   directly for Linux, and to derive the Windows `.ico` icon file.

- `svg` -- SVG source files; the PNG bitmap icons in `png` are derived from
  these files. The `*-16.svg`, `*-24.svg`, etc. files contain SVG files
  optimised for 16x16, 24x24, etc. icons. The `dosbox-staging-no-border.svg`
  is not used in the packaging process, it's just included for convenience.

- `windows` -- This contains the `.ico` file derived from the small PNG files
  that gets compiled into the Windows executable as a resource, and two BMP
  files used in the Windows installer.


## Prerequisites

To regenerate the derived icon files, you need the following Linux & macOS
tools:

- `rsvg-convert` to create the small PNG icons from the SVG sources (from the
   `librsvg` package).
- `icotool` to regenerate the Windows `.ico` file (from the `icoutils`
   package; there's no good Windows native CLI tool for this)
- `sips` & `iconutil` to regenerate the macOS `.icns` file (included with
   macOS out-of-the-box).


## Regenerating the icons

You need to update the following files at minimum to change the icon's design:

- `svg/dosbox-staging.svg` -- The source SVG file.

- `svg/dosbox-staging-XY.svg` -- SVG variants optimised for small derived
   icons to have greater control over subpixel rendering
   (`dosbox-staging-16.svg` is used to derive the 16x16px icon, and so on).
   Use the "Icon Preview" feature of Inkscape to modify these special SVG
   files.

- `macos/dosbox-staging-1024.png` -- The 1024px source PNG for the macOS icon.

After you've made changes to one or more of these, run `make all` and check in
the resulting derived files (run `make help` to see detailed info about all
possible targets).


## Platform-specific notes

### Windows

Windows does not support vector icons either. The `.ico` file contains bitmap
icons at various sizes; its built using the small PNG icons.

The 16px icon is used in the window's title bar, the 24px one is the taskbar
icon. If the 24px icon is missing, Windows will pick the wrong icon for the
window's titlebar as well (it will scale down the biggest icon it can find, so
the result will be blurry).

The icon file stored in the repo is packaged into the final executable via
Visual Studio, so make sure to update `src/winres.rc` if you change the name
or path of the `.ico` file.


### macOS

macOS does not support vector icons, so we generate the `.icns`
file from `macos/dosbox-staging-macos-1024.png` source file.

When building the app bundle, the `.icns` file must be placed in
`Contents/SharedSupport/Resources/`. The `Info.plist` files contains the name
of the icon file.

Please refer to Apple's documentation for further details.


### Linux

`svg/dosbox-staging.svg` is a scalable icon of size 128x128, which is the
recommended default size for most Linux desktop environments. This base SVG
icon follows the size and colour recommendations of GNOME HIG. This icon looks
good in most contexts, but for very small sizes (e.g., 16px to 32px), it's
better to use the PNG icons from the `png` directory.

When packaging for Linux, copy `svg/dosbox-staging.svg` into the distro
specific directory (usually `/usr/share/icons/hicolor/scalable/apps/`).

You will need an additional
[.desktop](https://specifications.freedesktop.org/desktop-entry-spec/latest/)
file to correlate the icon file with the application.

