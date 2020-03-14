#!/bin/bash

# Copyright (C) 2020  Kevin Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This script captures pre-processor #define statements
# matching the as-compiled state of the source file.
# The script runs recursively for all source files, and
# saves the results in corresponding <source>.defines file.
#
set -euo pipefail

# Ensure the project has been configured
if [[ ! -f config.h ]]; then
	echo "config.h not found. Please ./configure the project first"
	exit 1
fi

# Gather include paths
root_dir="$(pwd)"
root_include="$root_dir/include"
gcc_includes="$(gcc -xc++ -E -v - < /dev/null 2>&1 | grep '^ ' | tail -n +2 | sed 's/^ /-I/' | xargs)"
sdl_includes="$(sdl2-config --cflags)"

# Round up our source files
sources=( "$(find . -name '*.cpp' -o -name '*.c')" )

# Run it
parallel \
	"g++ -std=gnu++11 \
	    $gcc_includes \
	    $sdl_includes \
	    -I$root_dir \
	    -I$root_include \
	    -I{//} \
	    -DHAVE_CONFIG_H \
	    -E -dM -include '{}' - < /dev/null 2> /dev/null \
	    | grep '^#define' > '{.}.defines' \
	    ; echo '{} to {.}.defines'" ::: "${sources[@]}"
