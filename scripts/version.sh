#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2021-2021  kcgen <kcgen@users.noreply.github.com>
#
# version.sh - Print the currect software release an identifier

set -euo pipefail

vfile=".version"

if [[ ! -f "$vfile" ]]; then
  basedir=$(git rev-parse --show-toplevel) # or fail
  vfile="$basedir/$vfile"
fi

release=$(cat "$vfile") # or fail
identifier=$(git rev-parse --short=5 HEAD 2>/dev/null || echo norepo-source)

echo "VERSION=$release-$identifier"
