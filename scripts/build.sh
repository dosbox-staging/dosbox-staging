#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script builds the software for the given build-type (release,
# debug, ... ) and compiler (gcc or clang).
#
# If run without arguments, the script asks for required arguments one
# by one, which includes the above two (--compiler and --build-type).
#
# Optional arguments allow specifying the version of compiler,
# machine type, and applying build modifiers such as:
# - link-time-optimizations (--modifier lto),
# - feedback-directed-optimizations (--modifier fdo)
# - current processor instructions sets (--modifier native)
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

# Detects the machine type and sets the 'machine' variable
function query_machine() {
	# Start with a sane and safe default
	machine="$(uname -m | sed 's/-.*//')"

	# Only attempt further detection on Linux-based systems
	if [[ ! -f /proc/cpuinfo ]]; then
		return
	fi
	# ARM differentiation based on
	# https://github.com/RetroPie/RetroPie-Setup/blob/master/scriptmodules/system.sh
	case "$(sed -n '/^Hardware/s/^.*: \(.*\)/\1/p' < /proc/cpuinfo)" in
		BCM*)
			# calculated based on information from
			# https://github.com/AndrewFromMelbourne/raspberry_pi_revision
			local rev
			rev="0x$(sed -n '/^Revision/s/^.*: \(.*\)/\1/p' < /proc/cpuinfo)"
			# if bit 23 is not set, we are on a rpi1
			# (bit 23 means the revision is a bitfield)
			if [[ $((("$rev" >> 23) & 1)) -eq 0 ]]; then
				machine="rpi1"
			else
				# if bit 23 is set, get the cpu from bits 12-15
				local cpu
				cpu=$((("$rev" >> 12) & 15))
				case $cpu in
					0)
						machine="rpi1"
						;;
					1)
						machine="rpi2"
						;;
					2)
						machine="rpi3"
						;;
					3)
						machine="rpi4"
						;;
				esac
			fi
			;;
		ODROIDC)
			machine="odroid_c1"
			;;
		ODROID-C2)
			machine="odroid_c2"
			;;
		"Freescale i.MX6 Quad/DualLite (Device Tree)")
			machine="imx6"
			;;
		ODROID-XU[34])
			machine="odroid_xu"
			;;
		"Rockchip (Device Tree)")
			machine="tinker"
			;;
		Vero4K|Vero4KPlus)
			machine="vero4k"
			;;
		"Allwinner sun8i Family")
			machine="armv7_mali"
			;;
		*)
			;;
	esac
}

function parse_args() {
	# Gather the parameters that define this build
	postfix=""
	modifiers=("")
	configure_additions=("")
	while [[ "${#}" -gt 0 ]]; do case ${1} in
		-c|--compiler)          compiler="${2}";               shift 2;;
		-v|--version-postfix)   postfix="-${2}";               shift 2;;
		-t|--build-type)        selected_type="${2}";          shift 2;;
		-m|--modifier)          modifiers+=("${2}");           shift 2;;
		-p|--bin-path)          PATH="${2}:${PATH}";           shift 2;;
		-a|--machine)           machine="${2}";                shift 2;;
		*)                      configure_additions+=("${1}"); shift;;
	esac; done

	# Determine and import machine-specific rules
	if [[ -z "${machine:-}" ]]; then
		query_machine
	fi
	import machine "${machine}"

	# Determine and import OS-specific rules
	os="$(uname -s | sed 's/-.*//')"
	import os "${os}"

	# Import compiler rules, plus any OS + machine customizations
	import compiler "${compiler:-}"
	import "${compiler:-}" "${os}_${machine}"
	if [[ -z "${selected_type:-}" ]]; then
		arg_error "--build-type" "${TYPES[*]}"
	fi

	# Create a pretty modifier string that we can add to our build-type
	printf -v mod_string '+%s' "${modifiers[@]:1}"
	if [[ "${mod_string}" == "+" ]]; then
		mod_string=""
	fi

	# Print a summary of our build configuration
	underline "Compiling a ${selected_type}${mod_string} build using "`
	          `"${compiler}${postfix} on ${os}-${machine}" "="

	# Ensure our selected_type is lower-case before proceeding
	selected_type=$(lower "${selected_type}")
}

script_dir="$(cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P)"
# shellcheck source=scripts/automator/main.sh
source "$script_dir/automator/main.sh"
