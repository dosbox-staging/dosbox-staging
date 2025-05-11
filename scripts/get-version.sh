#!/usr/bin/env bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2024-2024  The DOSBox Staging Team

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

# xargs trims the leading whitespace and collapses multiple consecutive
# spaces into single spaces
#
# Gets the version number, e.g. 0.83.0
VERSION_NUMBER=$(
	grep "\\s\+VERSION\\s\+[0-9].[0-9]\+.[0-9]\+" CMakeLists.txt \
		| xargs | cut -d" " -f2
)

# Gets the version suffix, e.g. -alpha
VERSION_SUFFIX=$(
	grep "set(DOSBOX_VERSION\\s\+${PROJECT_VERSION}" CMakeLists.txt \
		| xargs | cut -d"}" -f2 | cut -d")" -f1 | xargs
)

VERSION=$VERSION_NUMBER$VERSION_SUFFIX


GIT_HASH=$(git rev-parse --short=5 HEAD)

case $1 in
    version)          echo "$VERSION" ;;
    hash)             echo "$GIT_HASH" ;;
    version-and-hash) echo "$VERSION-$GIT_HASH" ;;
    *)                usage; exit 1 ;;
esac
