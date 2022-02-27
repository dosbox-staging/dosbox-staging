#!/bin/bash

set -e
set -x

INSTALL_PREFIX='/usr/i686-w64-mingw32'
PACKAGE_DIR="dosbox_reelmagic_`git describe --abbrev=0 --tags 2>/dev/null || echo 'notag'`"

rm -Rf "$PACKAGE_DIR" "$PACKAGE_DIR.zip"
mkdir -p "$PACKAGE_DIR"
cp ../../example.dosbox.conf "$PACKAGE_DIR/dosbox.conf"
cp ../../dosbox-0.74-3/src/dosbox.exe "$PACKAGE_DIR/"
cp ../../dosbox-0.74-3/src/dosbox_heavydebug.exe "$PACKAGE_DIR/"
cp "$INSTALL_PREFIX/bin/"{SDL.dll,SDL_net.dll} "$PACKAGE_DIR/"

mkdir -p "$PACKAGE_DIR/diagtools"
cp ../../tools/*.exe "$PACKAGE_DIR/diagtools/"

zip -9r "$PACKAGE_DIR.zip" "$PACKAGE_DIR"
