#!/usr/bin/env bash

# SPDX-FileCopyrightText:  2024-2024 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later

usage() {
    printf "%s\n" "\
Usage: $0 TYPE

Print DOSBox Staging version information.

TYPE must be one of:
  version            Current DOSBox Staging version without 'v' prefix
                     (e.g., 0.79.1, 0.81.1-alpha)

  hash               Minimum 5-char long Git hash of the currently checked
                     out commit; can be longer to guarantee uniqueness
                     (e.g., da3c5, c22ef8)

  version-and-hash   Version and Git hash concatenated with a dash
                     (e.g., 0.79.1-da3c5, 0.81.1-alpha-c22ef8)
"
}

if [ "$#" -lt 1 ]; then
    usage
    exit 0
fi

ROOT=$(git rev-parse --show-toplevel)

VERSION=$(
    grep "project(dosbox-staging" -A 2 "$ROOT/CMakeLists.txt" \
        | grep "VERSION" | sed "s/ *VERSION *//g" | cut -d"\"" -f2
)

SUFFIX=$(
    grep "set(DOSBOX_VERSION" "$ROOT/CMakeLists.txt" | cut -d" " -f2 \
        | sed "s/\${PROJECT_VERSION}//g" | sed "s/)//g"
)

GIT_HASH=$(git rev-parse --short=5 HEAD)

case $1 in
    version)          echo "$VERSION$SUFFIX" ;;
    hash)             echo "$GIT_HASH" ;;
    version-and-hash) echo "$VERSION$SUFFIX-$GIT_HASH" ;;
    *)                usage; exit 1 ;;
esac
