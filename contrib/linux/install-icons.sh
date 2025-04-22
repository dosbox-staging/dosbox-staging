#!/usr/bin/env sh

# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2023-2025  The DOSBox Staging Team
#
# Installs icon files and desktop shortcut.

set -e

DEST_PATH="$HOME/.local/share"
REMOVE=false
USAGE=false
DESKTOP_FILE="org.dosbox_staging.dosbox_staging.desktop"

strstr() {
    [ "${1#*$2*}" = "$1" ] && return 1
    return 0
}

usage() {
    echo "Installs icon files and desktop shortcut."
    echo "  --system: Install to system-wide directory (/usr/share)"
    echo "  --remove: Remove icons and desktop file"
}

install_icons() {
    cp -r icons $DEST_PATH
    cp desktop/$DESKTOP_FILE $DEST_PATH/applications/$DESKTOP_FILE

    ## Replace executable location in the .desktop file
    sed -i 's\Exec=dosbox\Exec='$(realpath dosbox)'\g' $DEST_PATH/applications/$DESKTOP_FILE
}

remove_icons() {
    find $DEST_PATH/icons -name *dosbox_staging* -delete
    rm $DEST_PATH/applications/$DESKTOP_FILE
    ## Also remove legacy files, if present
    find $DEST_PATH/icons -name *dosbox-staging* -delete
    rm $DEST_PATH/applications/dosbox-staging.desktop
    rm $DEST_PATH/applications/org.dosbox-staging.dosbox-staging.desktop
}

for arg in $@
do
    strstr $arg "system" && DEST_PATH="/usr/share"
    strstr $arg "remove" && REMOVE=true
    strstr $arg "help" && USAGE=true
done

if [ $USAGE = true ]; then
    usage
    exit 0
fi

if [ $REMOVE = true ]; then
    remove_icons
else
    install_icons
fi
