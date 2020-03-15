#!/bin/bash

# Copyright (C) 2020  Kevin Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This script checks if <ctype> is included before <algorithm> by
# querying records written by the dump-preprocessor-defines.sh
# script.  It prints details and returns a non-zero exit code to
# the shell if it detects one or more incorrect include orders.
#
# See the following discussions for why this script exists:
# - https://travisdowns.github.io/blog/2019/11/19/toupper.html
# - https://news.ycombinator.com/item?id=21579333
#
set -euo pipefail

# Colors
declare -gr bold="\\e[1m"
declare -gr red="\\e[31m"
declare -gr green="\\e[32m"
declare -gr cyan="\\e[36m"
declare -gr reset="\\e[0m"

# Path to our define script
write_defines="./scripts/dump-preprocessor-defines.sh"

# Round up defines files
if [[ "$#" == "0" ]]; then
	"$write_defines"

	# shellcheck disable=SC2207
	defines=( $(find . -name '*.defines') )

	# Ensure we have as many as we have source files
	actual="${#defines[@]}"
	expected="$(find . -name '*.cpp' -o -name '*.c' | wc -l)"
	if (( "$actual" != "$expected" )); then
		echo "We should have $expected .defines files but $actual were found"
		exit 1
	fi

# Otherwise verify only those files specified
else
	defines=( )
	for f in "$@"; do
		extension="${f##*.}"
		if [[ "$extension" == "defines" ]]; then
			"$write_defines" "${f%.*}"
			defines+=( "$f" )
		else
			"$write_defines" "$f"
			defines+=( "${f}.defines" )
		fi
	done
fi

# The header strings that we're looking for
ctype="#define _CTYPE_H 1"
algorithm="#define __NO_CTYPE 1"
failures=0

# Check each for the correct order
for def in "${defines[@]}"; do
	result="$(grep -n "${ctype}\\|${algorithm}" "$def" || true)"
	ctype_line="$(echo "$result" | grep "$ctype"     | cut -f1 -d: || echo)"
	algor_line="$(echo "$result" | grep "$algorithm" | cut -f1 -d: || echo)"
	source="${def%.*}"
	if [[ -n "$ctype_line" && -n "$algor_line" ]]; then
		if [[ "$ctype_line" -lt "$algor_line" ]]; then
			echo -e "${bold}${green}[PASS]${reset} $source properly includes <ctype> before <algorithm>"
		else
			echo -e "${bold}${red}[FAIL]${reset} $source should include <ctype> before <algorithm>"
			(( failures++ )) || true
		fi
		echo "${result//^/       }"
		echo ""
	else
		echo -e "${bold}${cyan}[PASS]${reset} $source only includes one or neither"
	fi
done

if [[ "$failures" -gt "0" ]]; then
	exit 1
fi
