#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2022  DOSBox Staging Team

# This script exists only to easily update the Tracy header wrapper if/when
# more Tracy zone definitions are added.

set -euo pipefail

source=$(ls ../../subprojects/tracy-*/Tracy.hpp)

if [[ ! -f "$source" ]]; then
  echo "Tracy's source header could not be found."
  echo "Rerun 'meson setup -Dtracy=true' to fetch the subproject."
  exit 1
fi

target=../../include/tracy.h

if [[ ! -f "$target" ]]; then
  echo "The project's existing wrapper header could not be found."
  echo "Please change directories to where this script is, and try again."
  exit 1
fi


echo "Reading content from:   $source"
echo "Writing updated header: $target"

# Top
cat header_top.txt > "$target"

# Body
sed -n "/^#ifndef TRACY_ENABLE/,\${p;/^#else/q}" "$source" >> "$target"

# Bottom
cat header_bottom.txt >> "$target"

echo "Done"


