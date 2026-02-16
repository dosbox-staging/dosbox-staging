#!/bin/bash

# SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
# SPDX-FileCopyrightText:  2022-2022 Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# update-copyright-date.sh - Fix copyright lines in files touched this year and in metadata.

pushd "$(git rev-parse --show-toplevel)" > /dev/null || exit

year=$(date +%Y)
team="The DOSBox Staging Team"
spdx_sed_script="s|SPDX-FileCopyrightText:  \([0-9][0-9]*\)\(-[0-9][0-9]*\)* $team|SPDX-FileCopyrightText:  \1-$year $team|"
copyright_c_sed_script="s|Copyright (C) \([0-9][0-9]*\)\(-[0-9][0-9]*\)* $team|Copyright (C) \1-$year $team|"

# BSD sed (macOS) requires an argument after -i, GNU sed (Linux) does not
if sed --version 2>/dev/null | grep -q 'GNU sed'; then
	sed_inplace=(sed -i)
else
	sed_inplace=(sed -i '')
fi

find_files () {
	find {src,tests,resources,extras,scripts} -type f \( \
		-name '*.bat' -o \
		-name '*.cpp' -o \
		-name '*.c' -o \
		-name '*.glsl' -o \
		-name '*.h' -o \
		-name '*.in' -o \
		-name '*.py' -o \
		-name '*.sh' -o \
		-name '*.txt' \
		\)
}

filter_new_files () {
	while read -r path ; do
		committer_year=$(git log -1 --format=%cd --date=format:%Y "$path")
		if [[ "$committer_year" == "$year" ]] ; then
			echo "$path"
		fi
	done
}

# C, C++, headers, shell scripts, meson template inputs
# Run both SPDX and Copyright (C) patterns on each file
find_files | filter_new_files | xargs "${sed_inplace[@]}" -e "$spdx_sed_script" -e "$copyright_c_sed_script"

# Metadata files and dates visible to end users
"${sed_inplace[@]}" "$copyright_c_sed_script" extras/linux/org.dosbox_staging.dosbox_staging.metainfo.xml
"${sed_inplace[@]}" "s|Copyright [0-9][0-9]* $team|Copyright $year $team|" extras/macos/Info.plist.template
"${sed_inplace[@]}" "s|Copyright (C) [0-9][0-9]* $team|Copyright (C) $year $team|" src/platform/windows/winres.rc.in

echo "Some suspicious copyright lines (fix them manually if necessary):"
git grep -E "(Copyright \(C\)|SPDX-FileCopyrightText:) +$year-$year"
echo
echo "Review modified files and commit changes:"
git diff --shortstat

popd > /dev/null || exit
