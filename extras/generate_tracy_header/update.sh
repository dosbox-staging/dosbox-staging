#!/bin/bash

# SPDX-FileCopyrightText:  2022-2022 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later

# This script exists only to easily update the Tracy header wrapper if/when
# more Tracy zone definitions are added.

set -euo pipefail

source=$(ls ../../subprojects/tracy-*/public/tracy/Tracy.hpp)

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

SED=""
assign_gnu_sed () {
	# Is sed GNU? (BSD seds don't support --version)
	if sed --version &>/dev/null; then
		SED="sed"
	# No, but do we have gsed?
	elif command -v gsed &> /dev/null; then
		SED="gsed"
	# No, so help the user install it and then quit.
	else
		echo "'sed' is not GNU and 'gsed' is not available."
		if [[ "${OSTYPE:-}" == "darwin"* ]]; then
			echo "Install GNU sed with: brew install gnu-sed"
		else
			echo "Install GNU sed with your $OSTYPE package manager."
		fi
		exit 1
	fi
}

assign_gnu_sed

# Body
"$SED" -n "/^#ifndef TRACY_ENABLE/,\${p;/^#else/q}" "$source" >> "$target"

# Bottom
cat header_bottom.txt >> "$target"

echo "Done"


