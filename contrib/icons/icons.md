This directory contains resources to re-build icon files for various operating
systems.

# Base design

Base icon source file is `dosbox-staging.svg`. It contains scalable icon of
size 128x128 (recommended default size for most Linux desktop environments).
This icon looks good in most contexts, but for very small sizes (16px up to
32px) it's better to render icon based on one of files in `small-svg/`
directory.

# Rendering bitmap font

Use bundled Makefile for that.  It includes targets for rendering .png files
in a number of sizes using `rsvg-convert` command - it generates greatly
optimised small files with correctly set alpha channel (unlike `convert`
command from ImageMagick suite, which is traditionally used for this purpose,
but it's more suited for raster formats).

`rsvg-convert` program is available in most Linux repos and in macOS brew repo.

Run:

    make help

To learn about available targets.

# Small icon variants

Files in `small-svg/` directory are vector icons optimized for rendering in
small sizes; using vector format allows for easy adjustments with live
preview, and partly controlling sub-pixel rendering by carefully placing the
shape edges.

It's recommended to use Inkscape's "icon preview" feature when modifying these
files.

# OS-specific instructions

## Linux

Base design icon follows size and colour recommendations of Gnome HIG, so you
can use it directly, no pre-processing is needed.

When packaging for Linux, copy and rename `dosbox-staging.svg` icon to distro
specific directory (usually `/usr/share/icons/hicolor/scalable/apps/`).

You will need additional
[.desktop](https://specifications.freedesktop.org/desktop-entry-spec/latest/)
file entry to correlate icon file to the application.

Use Makefile to render small icons if necessary (some desktop environments use
them).

## macOS

macOS does not support scalable icons, so you'll need to generate .icns
file, which bundles raster icons in various sizes:

    make dosbox-staging.icns

When building App bundle, the .icns file should be placed in directory:
`Contents/SharedSupport/Resources/` and then you'll need to update information
in `Info.plist` file.

Refer to Apple documentation for details.

## Windows

Windows does not support vector icons and does not provide good programs for
rendering/bundling files in .ico format - developers are expected to edit
icons using raster editor provided by Visual Studio.

For this reason, preprocessed .ico file is bundled inside the repo.

**When changing icon, update the generated .ico file only after the design is
finalized.**

To rebuild .ico file, boot to Linux, install `icotool` program and run:

    make dosbox-staging.ico

When changing design, take extra steps for sizes:

- 16x16 - used next to window title
- 24x24 - taskbar icon; if missing, Windows will pick wrong icon for
          window titlebar as well (it will scale down biggest icon, so
          the result will be blurry).

Icon file stored in the repo is bundled via Visual Studio inside Windows
executable file; when changing file name or directory, update the file
`src/winres.rc`.
