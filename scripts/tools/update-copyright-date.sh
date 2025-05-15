#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2024  The DOSBox Staging Team
# Copyright (C) 2022       Patryk Obara <patryk.obara@gmail.com>

# update-copyright-date.sh - Fix copyright lines in files touched this year and in metadata.

pushd "$(git rev-parse --show-toplevel)" > /dev/null || exit

year=$(date +%Y)
team="The DOSBox Staging Team"
code_sed_script="s|Copyright (C) \([0-9]\+\)\(-\([0-9]\+\)\)\?  $team|Copyright (C) \1-$year  $team|"

find_c_cpp_files () {
	find {src,include,tests} -type f \( \
		-name '*.cpp' -o \
		-name '*.c' -o \
		-name '*.h' -o \
		-name '*.in' \
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

# C, C++, headers, meson template inputs
find_c_cpp_files | filter_new_files | xargs sed -i "$code_sed_script"

# Metadata files and dates visible to end users
sed -i "$code_sed_script" src/gui/gui_msg.h
sed -i "$code_sed_script" contrib/linux/org.dosbox_staging.dosbox_staging.metainfo.xml
sed -i "s|Copyright [0-9]\+ $team|Copyright $year $team|" extras/macos/Info.plist.template
sed -i "s|\xa9 [0-9]\+ $team|\xa9 $year $team|" src/winres.rc

echo "Some suspicious copyright lines (fix them manually if necessary):"
git grep "Copyright (C) $year-$year"
echo
echo "Review modified files and commit changes:"
git diff --shortstat

popd > /dev/null || exit
