#!/bin/bash

# Copyright (c) 2019 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script builds the software for the given build-type (release,
# debug, ... ) and compiler (gcc or clang).
#
# If run without arguments, the script asks for required arguments one
# by one, which includes the above two (--compiler and --build-type).
#
# Optional arguments include the version of compiler and additional
# build modifiers such as link-time-optimizations (--modifier lto),
# feedback-directed-optimizations (--modifier fdo), and taking advantage
# of the building-machine's full instructions sets (--modifier native).
# All modifiers are available simulatenously.
#
# Usage examples:
#   $ ./build.sh                             # asks for a compiler
#   $ ./build.sh --compiler gcc              # asks for a build-type
#   $ ./build.sh --compiler clang --build-type debug       # builds!
#   $ ./build.sh -c gcc -t release -m lto -m native        # builds!
#
# This script makes use of the automator package, see automator/main.sh.
#
set -euo pipefail

function parse_args() {
	# Gather the parameters that define this build
	postfix=""
	modifiers=("")
	while [[ "${#}" -gt 0 ]]; do case ${1} in
		-c|--compiler)          compiler="${2}";        shift 2;;
		-v|--version-postfix)   postfix="-${2}";        shift 2;;
		-t|--build-type)        selected_type="${2}";   shift 2;;
		-m|--modifier)          modifiers+=("${2}");    shift 2;;
		-p|--bin-path)          PATH="${2}:${PATH}";    shift 2;;
		*) >&2 echo "Unknown parameter: ${1}"; exit 1;  shift 2;;
	esac; done

	# Import our settings and report missing values
	machine="$(uname -m | sed 's/-.*//')"; import machine "${machine}"
	os="$(uname -s | sed 's/-.*//')"; import os "${os}"
	import compiler "${compiler:-}"
	import "${compiler:-}" "${os}_${machine}"
	if [[ -z "${selected_type:-}" ]]; then arg_error "--build-type" "${TYPES[*]}"; fi

	# Create a pretty modifier string that we can add to our build-type
	printf -v mod_string '+%s' "${modifiers[@]:1}"
	if [[ "${mod_string}" == "+" ]]; then mod_string=""; fi

	# Print a summary of our build configuration
	underline "Compiling a ${selected_type}${mod_string} build using "`
	          `"${compiler}${postfix} on ${os}-${machine}" "="

	# Ensure our selected_type is lower-case before proceeding
	selected_type=$(lower "${selected_type}")
}

# shellcheck source=scripts/automator/main.sh
source "$(dirname "$0")/automator/main.sh"
