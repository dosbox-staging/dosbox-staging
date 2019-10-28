#!/bin/bash

##
#
#  Copyright (c) 2019 Kevin R. Croft
#  SPDX-License-Identifier: GPL-2.0-or-later
#
#  This script builds DOSBox within supported environments including
#  MacOS, Ubuntu Linux, and MSYS2 using specified compilers and release types.
#
#  For automation without prompts, Windows should have User Account Control (UAC)
#  disabled, which matches the configuration of GitHub'Windows VMs, described here:
#  https://help.github.com/en/articles/virtual-environments-for-github-actions
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
	echo "Usage: ${script} [-b 32|64] [-c gcc|clang] [-f linux|macos|msys2] [-d] [-l] \\"
	echo "                [-p /custom/bin] [-u #] [-r fast|small|debug] [-s /your/src] [-t #]"
	echo ""
	echo "  FLAG                     Description                                            Default"
	echo "  -----------------------  -----------------------------------------------------  -------"
	echo "  -b, --bit-depth          Build a 64 or 32 bit binary                            [$(print_var "${BITS}")]"
	echo "  -c, --compiler           Choose either gcc or clang                             [$(print_var "${COMPILER}")]"
	echo "  -f, --force-system       Force the system to be linux, macos, or msys2          [$(print_var "${SYSTEM}")]"
	echo "  -d, --fdo                Used feedback-Directed Optimization data               [$(print_var "${FDO}")]"
	echo "  -l, --lto                Perform link-time-optimization                         [$(print_var "${LTO}")]"
	echo "  -p, --bin-path           Prepend PATH with the one provided to find executables [$(print_var "${BIN_PATH}")]"
	echo "  -u, --compiler-version # Use a specific compiler version (ie: 9 -> gcc-9)       [$(print_var "${COMPILER_VERSION}")]"
	echo "  -r, --release            Build a fast, small, or debug release                  [$(print_var "${RELEASE}")]"
	echo "  -s, --src-path           Use a different source directory to build              [$(print_var "${SRC_PATH}")]"
	echo "  -t, --threads #          Override auto-detection of number of logical CPUs      [$(print_var "${THREADS}")]"
	echo "  -v, --version            Print the version of this script                       [$(print_var "${SCRIPT_VERSION}")]"
	echo "  -x, --clean              Clean old object and build file before making          [$(print_var "${CLEAN}")]"
	echo "  -h, --help               Print this usage text"
	echo ""
	echo "Example: ${script} -b 32 --compiler clang -u 8 --bin-path /mingw64/bin -r small --lto"
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
		-d|--fdo)               FDO="true";             shift;;
		-f|--force-system)      SYSTEM="${2}";          shift;shift;;
		-l|--lto)               LTO="true";             shift;;
		-p|--bin-path)          BIN_PATH="${2}";        shift;shift;;
		-u|--compiler-version)  COMPILER_VERSION="${2}";shift;shift;;
		-r|--release)           RELEASE="${2}";         shift;shift;;
		-s|--src-path)          SRC_PATH="${2}";        shift;shift;;
		-t|--threads)           THREADS="${2}";         shift;shift;;
		-v|--version)           print_version;          shift;;
		-x|--clean)             CLEAN="true";           shift;;
		-h|--help)              usage "Show usage";     shift;;
		*) usage "Unknown parameter: ${1}";             shift;shift;;
	esac; done
}

function defaults() {
	# variables that are directly set via user arguments
	BITS="64"
	CLEAN="false"
	COMPILER="gcc"
	COMPILER_VERSION="unset"
	FDO="false"
	LTO="false"
	BIN_PATH="unset"
	RELEASE="fast"
	SRC_PATH="$(dirname "$(dirname "$(realpath -s "$0")")")"
	SYSTEM="auto"
	THREADS="auto"

	# derived variables with initial values
	EXECUTABLE="unset"
	VERSION_POSTFIX=""
	MACHINE="unset"
	CONFIGURE_OPTIONS=("--enable-core-inline")
	CFLAGS_ARRAY=("-Wall" "-pipe")
	LDFLAGS_ARRAY=("")
	LIBS_ARRAY=("")
	CALL_CACHE=("")

	# read-only strings
	readonly SCRIPT_VERSION="0.9"
	readonly REPO_URL="https://github.com/dreamer/dosbox-staging"

	# environment variables passed onto the build
	export CC="CC_is_not_set"
	export CXX="CXX_is_not_set"
	export LD="LD_is_not_set"
	export AR="AR_is_not_set"
	export RANLIB="RANLIB_is_not_set"
	export CFLAGS=""
	export CXXFLAGS=""
	export LDFLAGS=""
	export LIBS=""
	export GCC_COLORS="error=01;31:warning=01;35:note=01;36:range1=32:range2=34:locus=01:\
quote=01:fixit-insert=32:fixit-delete=31:diff-filename=01:\
diff-hunk=32:diff-delete=31:diff-insert=32:type-diff=01;32"
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

function exists() {
	command -v "${1}" &> /dev/null
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
	       || "${SYSTEM}" == "msys2" ]]; then
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

function tools_and_flags() {
	uses compiler_type
	uses compiler_version
	uses system

	# GCC universal
	if [[ "${COMPILER}" == "gcc" ]]; then
		CC="gcc${VERSION_POSTFIX}"
		LD="gcc${VERSION_POSTFIX}"
		CXX="g++${VERSION_POSTFIX}"
		CFLAGS_ARRAY+=("-fstack-protector" "-fdiagnostics-color=always")

		# Prioritize versioned lib-tools over generics
		AR="gcc-ar${VERSION_POSTFIX}"
		if ! exists "${AR}"; then
			AR="ar"
		fi

		RANLIB="gcc-ranlib${VERSION_POSTFIX}"
		if ! exists "${RANLIB}"; then
			RANLIB="ranlib"
		fi

	# CLANG universal
	elif [[ "${COMPILER}" == "clang" ]]; then
		CC="clang${VERSION_POSTFIX}"
		CXX="clang++${VERSION_POSTFIX}"
		CFLAGS_ARRAY+=("-fcolor-diagnostics")

		# CLANG on Linux and MSYS2
		if [[ "${SYSTEM}" == "linux" || "${SYSTEM}" == "msys2" ]]; then
			LD="llvm-link${VERSION_POSTFIX}"
			AR="llvm-ar${VERSION_POSTFIX}"
			RANLIB="llvm-ranlib${VERSION_POSTFIX}"

		# CLANG on MacOS
		elif [[ "${SYSTEM}" == "macos" ]]; then
			LD="ld"
		fi

		# CLANG and MSYS2
		if [[ "${SYSTEM}" == "msys2" ]]; then
			CFLAGS_ARRAY+=("-DWIN32")
		fi
	fi

	# macOS universal
	if [[ "${SYSTEM}" == "macos" ]]; then
		AR="ar"
		RANLIB="ranlib"

	# MSYS universal
	elif [[ "${SYSTEM}" == "msys2" ]]; then
		LIBS_ARRAY+=("-lwinmm" "-lws2_32")
	fi
}

function src_path() {
	if [[ ! -d "${SRC_PATH}" ]]; then
		usage "The requested source directory (${SRC_PATH}) does not exist, is not a directory, or is not accessible"
	fi
	cd "${SRC_PATH}"
}

function bin_path() {
	uses system
	uses compiler_type

	# By default, if we're on macOS and using GCC then always include /usr/local/bin, because
	# that's where brew installs all the binaries.  If the user adds their own --bin-path,
	# that will be prefixed ahead of /usr/local/bin and take precedent.
	if [[ "${SYSTEM}" == "macos" && "${COMPILER}" == "gcc" ]]; then
		PATH="/usr/local/bin:${PATH}"
	fi

	if [[ "${BIN_PATH}" != "unset" ]]; then
		if [[ ! -d "${BIN_PATH}" ]]; then
			usage "The requested PATH (${BIN_PATH}) does not exist, is not a directory, or is not accessible"
		else
			PATH="${BIN_PATH}:${PATH}"
		fi
	fi
}

function check_build_tools() {
	for tool in "${CC}" "${CXX}" "${LD}" "${AR}" "${RANLIB}"; do
		if ! exists "${tool}"; then
			error "${tool} was not found in your PATH or is not executable. If it's in a custom path, use --bin-path"
		fi
	done
}

function compiler_version() {
	if [[ "${COMPILER_VERSION}" != "unset" ]]; then
		VERSION_POSTFIX="-${COMPILER_VERSION}"
	fi
}

function release_flags() {
	uses compiler_type

	if [[ "${RELEASE}" == "fast" ]]; then
		CFLAGS_ARRAY+=("-Ofast")
	elif [[ "${RELEASE}" == "small" ]]; then
		CFLAGS_ARRAY+=("-Os")
		if [[ "${COMPILER}" == "gcc" ]]; then
			CFLAGS_ARRAY+=("-ffunction-sections" "-fdata-sections")

			# ld on MacOS doesn't understand --as-needed, so exclude it
			uses system
			if [[ "${SYSTEM}" != "macos" ]]; then
				LDFLAGS_ARRAY+=("-Wl,--as-needed")
			fi
		fi
	elif [[ "${RELEASE}" == "debug" ]]; then
		CFLAGS_ARRAY+=("-g" "-O1")
	else
		usage "The release type of ${RELEASE} is not allowed. Choose fast, small, or debug"
	fi
}

function threads() {
	uses system

	if [[ "${THREADS}" != "auto" ]]; then
		if [[ "${THREADS}" -lt 1 || "${THREADS}" -gt 256 ]]; then
			usage "The number of threads, ${THREADS}, needs to be between 1 and 256, or don't set it at all"
		fi
	else
		if [[ -n "${NUMBER_OF_PROCESSORS:-}" && "${NUMBER_OF_PROCESSORS}" -gt 0 ]]; then
			THREADS="${NUMBER_OF_PROCESSORS}"
		elif [[ "${SYSTEM}" == "macos" ]]; then
			THREADS="$(sysctl -n hw.physicalcpu || echo 4)"
		elif exists nproc; then
			THREADS="$(nproc)"
		else
			THREADS=4 # if auto-detection fails fallback to a reasonable
			          # number of logical CPUs for 2019
		fi
	fi
}

function fdo_flags() {
	if [[ "${FDO}" != "true" ]]; then
		return
	fi

	uses compiler_type
	local fdo_file="${SRC_PATH}/scripts/profile-data/${COMPILER}.profile"
	if [[ ! -f "${fdo_file}" ]]; then
		error "The Feedback-Directed Optimization file provided (${fdo_file}) does not exist or could not be accessed"
	fi

	if [[ "${COMPILER}" == "gcc" ]]; then

		# Don't let GCC 6.x and under use both FDO and LTO
		uses compiler_version
		if [[ ( "${COMPILER_VERSION}" == "unset"
		        && "$(2>&1 gcc -v | grep -Po '(?<=version )[^.]+')" -lt "7"
		        || "${COMPILER_VERSION}" -lt "7" )
		      && "${LTO}" == "true" ]]; then
			error "GCC versions 6 and under cannot handle FDO and LTO simultaneously; please change one or more these."
		fi
		CFLAGS_ARRAY+=("-fauto-profile=${fdo_file}")
	elif [[ "${COMPILER}" == "clang" ]]; then
		CFLAGS_ARRAY+=("-fprofile-sample-use=${fdo_file}")
	fi
}

function lto_flags() {
	if [[ "${LTO}" != "true" ]]; then
		return
	fi

	uses system
	# Only allow LTO on Linux and MacOS; it currently fails under Windows
	if [[ "${SYSTEM}" == "msys2" ]]; then
		usage "LTO currently does not link or build on Windows with GCC or Clang"
	fi

	uses compiler_type
	if [[ "${COMPILER}" == "gcc" ]]; then
		CFLAGS_ARRAY+=("-flto")

		# The linker performs the code optimizations across all objects, and therefore
		# needs the same flags as the compiler would normally get
		LDFLAGS_ARRAY+=("${CFLAGS_ARRAY[@]}")

		# GCC on MacOS fails to parse the thread flag, so we only provide it under Linux
		# (error: unsupported argument 4 to option flto=)
		if [[ "${SYSTEM}" == "linux" ]]; then
			uses threads
			LDFLAGS_ARRAY+=("-flto=$THREADS")
		fi

	elif [[ "${COMPILER}" == "clang" ]]; then
		CFLAGS_ARRAY+=("-flto=thin")

		# Clang LTO on Linux is incompatible with -Os so replace them with -O2
		# (ld: error: Optimization level must be between 0 and 3)
		if [[ "${SYSTEM}" == "linux" ]]; then
			CFLAGS_ARRAY=("${CFLAGS_ARRAY[@]/-Os/-O2}")
		fi # clang-linux-lto-small exclusion
	fi # gcc & clang
}

function configure_options() {
	uses system
	if [[ "${MACHINE}" != *"86"* ]]; then
		CONFIGURE_OPTIONS+=("--disable-dynamic-x86" "--disable-fpu-x86" "--disable-fpu-x64")
	fi
}

function do_autogen() {
	uses src_path
	if [[ ! -f autogen.sh ]]; then
		error "autogen.sh doesn't exist in our current directory $PWD. If your DOSBox source is somewhere else set it with --src-path"
	fi

	# Only autogen if needed ..
	if [[ ! -f configure ]]; then
		uses bin_path
		./autogen.sh
	fi
}

function do_configure() {
	uses bin_path
	uses src_path
	uses tools_and_flags
	uses release_flags
	uses fdo_flags
	uses lto_flags
	uses check_build_tools
	uses configure_options

	# Convert our arrays into space-delimited strings
	LIBS=$(    printf "%s " "${LIBS_ARRAY[@]}")
	CFLAGS=$(  printf "%s " "${CFLAGS_ARRAY[@]}")
	CXXFLAGS=$(printf "%s " "${CFLAGS_ARRAY[@]}")
	LDFLAGS=$( printf "%s " "${LDFLAGS_ARRAY[@]}")

	local lto_string=""
	if [[ "${LTO}" == "true" ]]; then
		lto_string="-LTO"
	fi

	local fdo_string=""
	if [[ "${FDO}" == "true" ]]; then
		fdo_string="-FDO"
	fi

	echo ""
	echo "Launching with:"
	echo ""
	echo "    CC       = ${CC}"
	echo "    CXX      = ${CXX}"
	echo "    LD       = ${LD}"
	echo "    AR       = ${AR}"
	echo "    RANLIB   = ${RANLIB}"
	echo "    CFLAGS   = ${CFLAGS}"
	echo "    CXXFLAGS = ${CXXFLAGS}"
	echo "    LIBS     = ${LIBS}"
	echo "    LDFLAGS  = ${LDFLAGS}"
	echo "    THREADS  = ${THREADS}"
	echo ""
	echo "Build type: ${SYSTEM}-${MACHINE}-${BITS}bit-${COMPILER}-${RELEASE}${lto_string}${fdo_string}"
	echo ""
	"${CC}" --version
	sleep 5

	if [[ ! -f configure ]]; then
		error "configure script doesn't exist in $PWD. If the source is somewhere else, set it with --src-path"

	elif ! ./configure "${CONFIGURE_OPTIONS[@]}"; then
		>&2 cat "config.log"
		error "configure failed, see config.log output above"
	fi
}

function executable() {
	uses src_path
	EXECUTABLE="src/"
	if [[ "${SYSTEM}" == "msys2" ]]; then
		EXECUTABLE+="dosbox.exe"
	else
		EXECUTABLE+="dosbox"
	fi

	if [[ ! -f "${EXECUTABLE}" ]]; then
		error "${EXECUTABLE} does not exist or hasn't been created yet"
	fi
}

function build() {
	uses src_path
	uses bin_path
	uses threads

	if [[ "${CLEAN}" == "true" && -f "Makefile" ]]; then
		make clean 2>&1 | tee -a build.log
	fi
	do_autogen
	do_configure
	make -j "${THREADS}" 2>&1 | tee -a build.log
}

function strip_binary() {
	if [[ "${RELEASE}" == "debug" ]]; then
		echo "[skipping strip] Debug symbols will be left in the binary because it's a debug release"
	else
		uses bin_path
		uses executable
		strip "${EXECUTABLE}"
	fi
}

function show_binary() {
	uses bin_path
	uses system
	uses executable

	if [[ "$SYSTEM" == "macos" ]]; then
		otool -L "${EXECUTABLE}"
	else
		ldd "${EXECUTABLE}"
	fi
	ls -1lh "${EXECUTABLE}"
}


function main() {
	parse_args "$@"
	build
	strip_binary
	show_binary
}

main "$@"
