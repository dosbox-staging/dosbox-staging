#!/bin/bash

##
#  Copyright (c) 2019 Kevin R. Croft
#  SPDX-License-Identifier: GPL-2.0-or-later
#
#  This script lists DOSBox package dependencies as determined by the runtime
#  architecture, operating system, and selected compiler type and and its version.
#
#  See the usage block below for details or run it with the -h or --help arguments.
#
#  In general, this script adheres to Google's shell scripting style guide
#  (https://google.github.io/styleguide/shell.xml), however some deviations (such as
#  tab indents instead of two-spaces) are used to fit with DOSBox's in-practice"
#  coding style.
#

set -euo pipefail
IFS=$'\n\t'

function usage() {
	if [ -n "${1}" ]; then
		errcho "${1}"
	fi
	local script
	script=$(basename "${0}")
	echo "Usage: ${script} [-b 32|64] [-c gcc|clang] [-f linux|macos|msys2] [-o FILE] [-u #]"
	echo ""
	echo "  FLAG                     Description                                            Default"
	echo "  -----------------------  -----------------------------------------------------  -------"
	echo "  -b, --bit-depth          Build a 64 or 32 bit binary                            [$(print_var "${BITS}")]"
	echo "  -c, --compiler           Choose either gcc or clang                             [$(print_var "${COMPILER}")]"
	echo "  -f, --force-system       Force the system to be linux, macos, or msys2          [$(print_var "${SYSTEM}")]"
	echo "  -o, --output-file <file> Write the packages to a text file instead of stdout    [$(print_var "${OUTPUT_FILE}")]"
	echo "  -u, --compiler-version # Use a specific compiler version (ie: 9 -> gcc-9)       [$(print_var "${COMPILER_VERSION}")]"
	echo "  -v, --version            Print the version of this script                       [$(print_var "${SCRIPT_VERSION}")]"
	echo "  -h, --help               Print this usage text"
	echo ""
	echo "Example: ${script} --bit-depth 32 --compiler clang --compiler-version 8"
	echo ""
	echo "Note: the last value will take precendent if duplicate flags are provided."
	exit 1
}

# parse params
function parse_args() {
	defaults
	while [[ "${#}" -gt 0 ]]; do case ${1} in
		-b|--bit-depth)         BITS="${2}";            shift;shift;;
		-c|--compiler)          COMPILER="${2}";        shift;shift;;
		-f|--force-system)      SYSTEM="${2}";          shift;shift;;
		-o|--output-file)       OUTPUT_FILE="${2}";     shift;shift;;
		-u|--compiler-version)  COMPILER_VERSION="${2}";shift;shift;;
		-v|--version)           print_version;          shift;;
		-h|--help)              usage "Show usage";     shift;;
		*) usage "Unknown parameter: ${1}";             shift;shift;;
	esac; done
}

function defaults() {
	# variables that are directly set via user arguments
	BITS="64"
	COMPILER="gcc"
	COMPILER_VERSION="unset"
	OUTPUT_FILE="unset"
	SYSTEM="auto"

	# derived variables with initial values
	VERSION_POSTFIX=""
	MACHINE="unset"
	CALL_CACHE=("")

	# read-only strings
	readonly SCRIPT_VERSION="0.1"
	readonly REPO_URL="https://github.com/dreamer/dosbox-staging"
}

function errcho() {
	local CLEAR='\033[0m'
	local RED='\033[0;91m'
	>&2 echo ""
	>&2 echo -e " ${RED}👉  ${*}${CLEAR}" "\\n"
}
function bug() {
	local CLEAR='\033[0m'
	local YELLOW='\033[0;33m'
	>&2 echo -e " ${YELLOW}Please report the following at ${REPO_URL}${CLEAR}"
	errcho "${@}"
	exit 1
}

function error() {
	errcho "${@}"
	exit 1
}

function print_var() {
	if [[ -z "${1}" ]]; then
		echo "unset"
	else
		echo "${1}"
	fi
}

function uses() {
	# assert
	if [[ "${#}" != 1 ]]; then
		bug "The 'uses' function was called without an argument"
	fi

	# only handles function calls, so filter everything else
	func="${1}"
	if [[ "$(type -t "${func}")" != "function" ]]; then
		bug "The 'uses' function was passed ${func}, which isn't a function"
	fi

	local found_in_previous="false"
	# has our function already been called?
	for previous_func in "${CALL_CACHE[@]}"; do
		if [[ "${previous_func}" == "${func}" ]]; then
			found_in_previous="true"
			break
		fi
	done

	# if it hasn't, record it and run it
	if [[ "${found_in_previous}" == "false" ]]; then
		CALL_CACHE+=("${func}")
		"${func}"
	fi
}


function print_version() {
	echo "${SCRIPT_VERSION}"
	exit 0
}

function packages() {
	# only proceed if the user wants to install packages
	uses system
	if [[ "${OUTPUT_FILE}" != "unset" ]]; then
		"packages_for_${SYSTEM}" > "${OUTPUT_FILE}"
	else
		"packages_for_${SYSTEM}"
	fi
}

function packages_for_macos() {

	uses compiler_type
	uses compiler_version
	local compiler_package="" # for brew, the clang package doesn't exist, so stay empty in this case (?)
	if [[ "${COMPILER}" == "gcc" ]]; then
		compiler_package="gcc"
		if [[ "${COMPILER_VERSION}" != "unset" ]]; then
			compiler_package+="@${COMPILER_VERSION}"
		fi
	fi

	# Typical installation:
	#  - brew update
	#  - brew install <list of packages>

	local packages=(
	   "${compiler_package}"
	   coreutils
	   autogen
	   autoconf
	   automake
	   pkg-config
	   libpng
	   sdl
	   sdl_net
	   opusfile
	   speexdsp )
	echo "${packages[@]}"
}

function packages_for_msys2() {
	uses bits
	local pkg_type=""
	if [[ "${BITS}" == 64 ]]; then
		pkg_type="x86_64"
	else
		pkg_type="i686"
	fi

	uses compiler_type
	uses compiler_version
	local compiler_package="${COMPILER}"
	compiler_package+="${VERSION_POSTFIX}"

	# Typical installation step:
	# pacman -S --noconfirm <list of packages>
	local packages=(
	   autogen
	   autoconf
	   base-devel
	   automake-wrapper
	   "mingw-w64-${pkg_type}-pkg-config"
	   "mingw-w64-${pkg_type}-${compiler_package}"
	   "mingw-w64-${pkg_type}-libtool"
	   "mingw-w64-${pkg_type}-libpng"
	   "mingw-w64-${pkg_type}-zlib"
	   "mingw-w64-${pkg_type}-SDL"
	   "mingw-w64-${pkg_type}-SDL_net"
	   "mingw-w64-${pkg_type}-opusfile"
	   "mingw-w64-${pkg_type}-speexdsp" )
	echo "${packages[@]}"
}

function packages_for_linux() {

	# TODO:
	#  Convert this into an associative map between package
	#  managers and the list of respective package names. We
	#  should have coverage for the major distribution types,
	#  such as:
	#     - RPM-based (RHEL/CentOS, Fedora, and openSUSE)
	#     - Debian-based (Debian, Ubuntu, and Raspbian)
	#     - pacman-based (Arch and Manjero)
	#
	uses compiler_type
	local compiler_package=""
	if [[ "${COMPILER}" == "gcc" ]]; then
		compiler_package="g++"
	else
		compiler_package="clang"
	fi

	uses compiler_version
	compiler_package+="${VERSION_POSTFIX}"

	# Typically install step
	# sudo apt update -y
	# sudo apt install -y <list of packages>
	local packages=(
	   "${compiler_package}"
	   libtool
	   build-essential
	   libsdl1.2-dev
	   libsdl-net1.2-dev
	   libopusfile-dev
	   libspeexdsp-dev )
	echo "${packages[@]}"
}

function system() {
	if [[ "${MACHINE}" == "unset" ]]; then
		MACHINE="$(uname -m)"
	fi

	if [[ "${SYSTEM}" == "auto" ]]; then
		SYSTEM="$(uname -s)"
	fi
	if [[     "${SYSTEM}" == "Darwin" \
	       || "${SYSTEM}" == "macos" ]]; then
		SYSTEM="macos"

	elif [[   "${SYSTEM}" == "MSYS"* \
	       || "${SYSTEM}" == "msys2" \
	       || "${SYSTEM}" == "MINGW"* ]]; then
		SYSTEM="msys2"

	elif [[   "${SYSTEM}" == "Linux" \
	       || "${SYSTEM}" == "linux" ]]; then
		SYSTEM="linux"

	else
		error "Your system, ${SYSTEM}, is not currently supported"
	fi
}

function bits() {
	if [[ "${BITS}" != 64 && "${BITS}" != 32 ]]; then
		usage "A bit-depth of ${BITS} is not allowed; choose 64 or 32"
	fi
}

function compiler_type() {
	if [[ "${COMPILER}" != "gcc" && "${COMPILER}" != "clang" ]]; then
		usage "The choice of compiler (${COMPILER}) is not valid; choose gcc or clang"
	fi
}

function compiler_version() {
	if [[ "${COMPILER_VERSION}" != "unset" ]]; then
		VERSION_POSTFIX="-${COMPILER_VERSION}"
	fi
}

function main() {
	parse_args "$@"
	packages
}

main "$@"
