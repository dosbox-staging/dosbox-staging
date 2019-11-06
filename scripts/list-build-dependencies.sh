#!/bin/bash

##
#  Copyright (c) 2019 Kevin R. Croft
#  SPDX-License-Identifier: GPL-2.0-or-later
#
#  This script lists development packages and DOSBox dependencies needed to build DOSBox.
#  This package names provided are tailor based on the provided package manager,
#  choice of compiler, and optionally its bit-depth.
#
#  See the usage arguments below for details or run it with the -h or --help.
#
#  In general, this script adheres to Google's shell scripting style guide
#  (https://google.github.io/styleguide/shell.xml), however some deviations (such as
#  tab indents instead of two-spaces) are used to fit DOSBox's in-practice coding style.
#

set -euo pipefail
IFS=$'\n\t'

function usage() {
	if [ -n "${1}" ]; then
		errcho "${1}"
	fi
	local script
	script=$(basename "${0}")
	echo "Usage: ${script} -p apt|xcode|brew|macports|msys2|vcpkg [-b 32|64] [-c gcc|clang] [-o FILE] [-u #]"
	echo ""
	echo "  FLAG                       Description                                          Default"
	echo "  -----------------------    ---------------------------------------------------  -------"
	echo "  -b, --bit-depth            Choose either a 64 or 32 bit compiler                [$(print_var "${BITS}")]"
	echo "  -c, --compiler             Choose either gcc or clang                           [$(print_var "${COMPILER}")]"
	echo "  -u, --compiler-version <#> Customize the compiler version (if available)        [$(print_var "${COMPILER_VERSION}")]"
	echo "  -v, --version              Print the version of this script                     [$(print_var "${SCRIPT_VERSION}")]"
	echo "  -h, --help                 Print this usage text"
	echo "  -p, --package-manager      Choose one of the following:"
	echo "                               - apt      (Linux: Debian, Ubuntu, Raspbian)"
	echo "                               - dnf      (Linux: Fedora, RedHat, CentOS)"
	echo "                               - pacman   (Linux: Arch, Manjaro)"
	echo "                               - zypper   (Linux: SuSE, OpenSUSE)"
	echo "                               - xcode    (OS X: Apple-supported)"
	echo "                               - brew     (OS X: Homebrew community-supported)"
	echo "                               - macports (OS X: MacPorts community-supported)"
	echo "                               - msys2    (Windows: Cygwin-based, community-support)"
	echo "                               - vcpkg    (Windows: Visual Studio builds)"
	echo "  -----------------------    ---------------------------------------------------  -------"
	echo ""
	echo "Example: ${script} --package-manager apt --compiler clang --compiler-version 8"
	echo ""
	echo "Note: the last value will take precendent if duplicate flags are provided."
	exit 1
}

function defaults() {
	BITS="64"
	COMPILER="gcc"
	COMPILER_VERSION=""
	PACKAGE_MANAGER="unset"
	readonly SCRIPT_VERSION="0.2"
	readonly REPO_URL="https://github.com/dreamer/dosbox-staging"
}

# parse params
function parse_args() {
	defaults
	while [[ "${#}" -gt 0 ]]; do case ${1} in
		-b|--bit-depth)        BITS="${2}";             shift;shift;;
		-c|--compiler)         COMPILER="${2}";         shift;shift;;
		-p|--package-manager)  PACKAGE_MANAGER="${2}";  shift;shift;;
		-u|--compiler-version) COMPILER_VERSION="${2}"; shift;shift;;
		-v|--version)          print_version;           shift;;
		-h|--help)             usage "Show usage";      shift;;
		*) usage "Unknown parameter: ${1}";             shift;shift;;
	esac; done

	# Check mandatory arguments
	if [[ "${PACKAGE_MANAGER}" == "unset" ]]; then
		usage "A choice of package manager must be provided; use '-p <choice>' or '--package-manager <choice>'"
	fi
}

function errcho() {
	local CLEAR='\033[0m'
	local RED='\033[0;91m'
	>&2 echo ""
	>&2 echo -e " ${RED}👉  ${*}${CLEAR}" "\\n"
}

function print_var() {
	if [[ -z "${1}" ]]; then
		echo "unset"
	else
		echo "${1}"
	fi
}

function list_packages() {
	VERSION_DELIM=""
	case "$1" in

		apt)
			# Package repo: https://packages.ubuntu.com/
			# Apt separates GCC into the gcc and g++ pacakges, the latter which depends on the prior.
			# Therefore, we provide g++ in-place of gcc.
			VERSION_DELIM="-"
			if [[ "${COMPILER}" == "gcc" ]]; then
				COMPILER="g++"
			fi
			PACKAGES=(xvfb libtool build-essential autoconf-archive libsdl1.2-dev libsdl-net1.2-dev libopusfile-dev)
			;;

		dnf)
			# Package repo: https://apps.fedoraproject.org/packages/
			VERSION_DELIM="-"
			PACKAGES=(xvfb libtool autoconf-archive SDL SDL_net-devel opusfile-devel)
			;;

		pacman)
			# Package repo: https://www.archlinux.org/packages/
			# Arch offers 32-bit versions of SDL (but not others)
			PACKAGES=(xvfb libtool autoconf-archive sdl_net opusfile)
			if [[ "${BITS}" == 32 ]]; then
				PACKAGES+=(lib32-sdl)
			else
				PACKAGES+=(sdl)
			fi
			;;

		zypper)
			# Package repo: https://pkgs.org/
			# openSUSE offers 32-bit versions of SDL and SDL_net (but not others)
			PACKAGES=(devel_basis xvfb libtool autoconf-archive opusfile)
			if [[ "${BITS}" == 32 ]]; then
				PACKAGES+=(libSDL-devel-32bit libSDL_net-devel-32bit)
			else
				PACKAGES+=(SDL SDL_net)
			fi
			;;

		xcode)
			# If the user doesn't want to use Homebrew or MacPorts, then they are limited to
			# Apple's Clang plus the provided command line development tools provided by Xcode.
			COMPILER=""
			echo "Execute the following:"
			echo "   xcode-select --install    # to install command line development tools"
			echo "   sudo xcodebuild -license  # to accept Apple's license agreement"
			echo ""
			echo "Now download, build, and install the following manually to avoid using Homebrew or MacPorts:"
			echo " - coreutils autogen autoconf automake pkg-config libpng sdl sdl_net opusfile"
			;;

		brew)
			# Package repo: https://formulae.brew.sh/
			# If the user wants Clang, we knock it out because it's provided provided out-of-the-box
			VERSION_DELIM="@"
			if [[ "${COMPILER}" == "clang" ]]; then
				COMPILER=""
			fi
			PACKAGES=(coreutils autogen autoconf autoconf-archive automake pkg-config libpng sdl sdl_net opusfile)
			;;

		macports)
			# Package repo: https://www.macports.org/ports.php?by=name
			PACKAGES=(coreutils autogen autoconf autoconf-archive automake pkgconfig libpng libsdl libsdl_net opusfile)
			;;

		msys2)
			# Package repo: https://packages.msys2.org/base
			# MSYS2 only supports the current latest releases of Clang and GCC, so we disable version customization
			COMPILER_VERSION=""
			local pkg_type
			pkg_type="x86_64"
			if [[ "${BITS}" == 32 ]]; then
				pkg_type="i686"
			fi
			PACKAGES=(autogen autoconf autoconf-archive base-devel automake-wrapper)
			for pkg in pkg-config libtool libpng zlib SDL SDL_net opusfile; do
				PACKAGES+=("mingw-w64-${pkg_type}-${pkg}")
			done
			COMPILER="mingw-w64-${pkg_type}-${COMPILER}"
			;;

		vcpkg)
			# Package repo: https://repology.org/projects/?inrepo=vcpkg
			# VCPKG doesn't provide Clang or GCC, so we knock out the compiler and just give packages
			COMPILER=""
			PACKAGES=(libpng sdl1 sdl1-net opusfile)
			;;
		*)
			usage "Unknown package manager ${1}"
			;;
	esac

	if [[ -n "${COMPILER_VERSION}" && -n "${COMPILER}" ]]; then
		COMPILER+="${VERSION_DELIM}${COMPILER_VERSION}"
	fi
	echo "${COMPILER} ${PACKAGES[*]}"
}

function main() {
	parse_args "$@"
	list_packages "${PACKAGE_MANAGER}"
}

main "$@"
