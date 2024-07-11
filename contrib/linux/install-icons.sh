#!/usr/bin/env sh

# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2023-2023  The DOSBox Staging Team
#
# Installs icon files and desktop shortcut.

set -e

DEST_PATH="$HOME/.local/share"
REMOVE=false
USAGE=false

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
    cp desktop/org.dosbox-staging.DOSBoxStaging.desktop $DEST_PATH/applications/org.dosbox-staging.DOSBoxStaging.desktop

    ## Replace executable location in the .desktop file
    sed -i 's\Exec=dosbox\Exec='$(realpath dosbox)'\g' $DEST_PATH/applications/org.dosbox-staging.DOSBoxStaging.desktop
}

remove_icons() {
    find $DEST_PATH/icons -name dosbox-staging* -delete
    rm $DEST_PATH/applications/org.dosbox-staging.DOSBoxStaging.desktop
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
