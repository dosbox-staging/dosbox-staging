#!/bin/bash

##
#
#  Copyright (c) 2019 Kevin R. Croft
#  SPDX-License-Identifier: GPL-2.0-or-later
#
#  This script builds DOSBox within supported environments including
#  MacOS, Ubuntu Linux, and MSYS2 using specified compilers and build types.
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
readonly INVOCATION="${0}"
readonly IFS=$'\n\t'

function usage() {
	if [ -n "${1}" ]; then
		errcho "${1}"
	fi
	local script
	script=$(basename "${INVOCATION}")
	echo "Usage: ${script} [-b release|debug|profile|*san] [-c gcc|clang] [-d] [-f linux|macos|msys2]"
	echo "                 [-i 32|64] [-l] [-p /custom/bin] [-u #] [-b release|debug|profile|*san]"
	echo "                 [-s /your/src] [-t #]"
	echo ""
	echo "  FLAG                    Description                                            Default"
	echo "  ----------------------- -----------------------------------------------------  -------"
	echo "  -b, --build-type        Choose a build-type: release, debug, profile, or ...   [$(print_var "${BUILD_TYPE}")]"
	echo "                            usan:  detect undefined behavior and integer overflows"
	echo "                            asan:  detect out of bounds use, double free, and leaks"
	echo "                            msan:  detect unitialized reads and use-after-destruction"
	echo "                            uasan: detect both undefined (usan) and address (asan) "
	echo "                            tsan:  detect data-race threading bugs"
	echo ""
	echo "  -c, --compiler          Choose to compile with gcc or clang                    [$(print_var "${COMPILER}")]"
	echo "  -d, --fdo               Use Feedback-Directed Optimization (FDO) data          [$(print_var "${FDO}")]"
	echo "  -f, --force-system      Force the system to be linux, macos, or msys2          [$(print_var "${SYSTEM}")]"
	echo "  -i, --bit-depth         Choose to build a 64 or 32 bit binary                  [$(print_var "${BITS}")]"
	echo "  -l, --lto               Perform Link-Time-Optimizations (LTO)                  [$(print_var "${LTO}")]"
	echo "  -p, --bin-path          Prepend PATH with the one provided to find executables [$(print_var "${BIN_PATH}")]"
	echo "  -u, --compiler-version  Customize the compiler postfix (ie: 9 -> gcc-9)        [$(print_var "${COMPILER_VERSION}")]"
	echo ""
	echo "  -s, --src-path          Enter a different source directory before building     [$(print_var "${SRC_PATH}")]"
	echo "  -t, --threads           Override the number of threads with which to compile   [$(print_var "${THREADS}")]"
	echo "  -v, --version           Print the version of this script                       [$(print_var "${SCRIPT_VERSION}")]"
	echo "  -x, --clean             Clean old objects prior to building                    [$(print_var "${CLEAN}")]"
	echo "  -h, --help              Print this usage text"
	echo ""
	echo "Example: ${script} -b 32 --compiler clang -u 8 --bin-path /mingw64/bin -r small --lto"
	echo ""
	echo "Note: the last value will take precendent if duplicate flags are provided."
	exit 1
}

function parse_args() {
	set_defaults
	while [[ "${#}" -gt 0 ]]; do case ${1} in
		-b|--build-type)        BUILD_TYPE="${2}";      shift;shift;;
		-i|--bit-depth)         BITS="${2}";            shift;shift;;
		-c|--compiler)          COMPILER="${2}";        shift;shift;;
		-d|--fdo)               FDO="true";             shift;;
		-f|--force-system)      SYSTEM="${2}";          shift;shift;;
		-l|--lto)               LTO="true";             shift;;
		-p|--bin-path)          BIN_PATH="${2}";        shift;shift;;
		-u|--compiler-version)  COMPILER_VERSION="${2}";shift;shift;;
		-s|--src-path)          SRC_PATH="${2}";        shift;shift;;
		-t|--threads)           THREADS="${2}";         shift;shift;;
		-v|--version)           print_version;          shift;;
		-x|--clean)             CLEAN="true";           shift;;
		-h|--help)              usage "Show usage";     shift;;
		*) usage "Unknown parameter: ${1}";             shift;shift;;
	esac; done
}

function set_defaults() {
	# variables that are directly set via user arguments
	BITS="64"
	CLEAN="false"
	COMPILER="gcc"
	COMPILER_VERSION="unset"
	FDO="false"
	LTO="false"
	BIN_PATH="unset"
	BUILD_TYPE="release"
	SRC_PATH="unset"
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
	readonly SCRIPT_VERSION="1.0"
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

##
#  Uses
#  ----
#  Alows function to indicate which other functions they depend on.
#  For example: "uses system" indicates a function needs the system
#  to be defined prior to running.
#  This uses function acts like a call cache, ensuring each used function
#  is only actually called once.  This allows all functions to thoroughly
#  employ the 'uses' mechanism without the performance-hit of repeatedly
#  executing the same function.  Likely, functions that are 'used' are
#  atomic in that they will only be called once per script invocation.
#
function uses() {
	# assert
	if [[ "${#}" != 1 ]]; then
		bug "The 'uses' function was called without an argument"
	fi

	# only operate on functions in our call-scope, otherwise fail hard
	func="${1}"
	if [[ "$(type -t "${func}")" != "function" ]]; then
		bug "The 'uses' function was passed ${func}, which isn't a function"
	fi

	# Check the call cache to see if the function has already been called
	local found_in_previous="false"
	for previous_func in "${CALL_CACHE[@]}"; do
		if [[ "${previous_func}" == "${func}" ]]; then
			found_in_previous="true"
			break
		fi
	done

	# if it hasn't been called then record it and run it
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
	if   [[ "${MACHINE}" == "unset" ]]; then MACHINE="$(uname -m)"; fi
	if   [[ "${SYSTEM}" == "auto" ]]; then SYSTEM="$(uname -s)"; fi
	case "$SYSTEM" in
		Darwin|macos) SYSTEM="macos" ;;
		MSYS*|msys2)  SYSTEM="msys2" ;;
		Linux|linux)  SYSTEM="linux" ;;
		*)            error "Your system, $SYSTEM, is not currently supported" ;;
	esac
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
		RANLIB="gcc-ranlib${VERSION_POSTFIX}"
		if ! exists "${AR}"; then AR="ar"; fi
		if ! exists "${RANLIB}"; then RANLIB="ranlib"; fi

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
		elif [[ "${SYSTEM}" == "macos" ]]; then LD="ld"; fi

		# CLANG and MSYS2
		if [[ "${SYSTEM}" == "msys2" ]]; then CFLAGS_ARRAY+=("-DWIN32"); fi
	fi

	# macOS universal
	if [[ "${SYSTEM}" == "macos" ]]; then
		AR="ar"
		RANLIB="ranlib"
	fi
}

function src_path() {
	if [[ "${SRC_PATH}" == "unset" ]]; then
		SRC_PATH="$(cd "$(dirname "${INVOCATION}")" && cd .. && pwd -P)"
	elif [[ ! -d "${SRC_PATH}" ]]; then
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

function build_type_flags() {
	uses compiler_type
    uses system

	##
	#  Release flags
	#
	if [[ "${BUILD_TYPE}" == "release" ]]; then
		if [[ "${COMPILER}" == "gcc" ]]; then
			CFLAGS_ARRAY+=("-Ofast" "-ffunction-sections" "-fdata-sections")
			if [[ "${SYSTEM}" != "macos" ]]; then LDFLAGS_ARRAY+=("-Wl,--as-needed"); fi
		else # Clang
			CFLAGS_ARRAY+=("-Os")
			# -Os is called _fastest, smallest_ under Clang and is the recommended flag for release builds
			#  - https://developer.apple.com/library/archive/documentation/Performance/Conceptual/CodeFootprint/Articles/CompilerOptions.html
			#  - https://dmtopolog.com/code-optimization-for-swift-and-objective-c
		fi

	##
	#  Debug flags
	#
	elif [[ "${BUILD_TYPE}" == "debug" ]]; then
		CFLAGS_ARRAY+=("-g" "-O0" "-fno-omit-frame-pointer")

	##
	#  Profile flags
	#
	elif [[ "${BUILD_TYPE}" == "profile" ]]; then
		CFLAGS_ARRAY+=("-g" "-O0" )
		if   [[ "${COMPILER}" == "gcc"   ]]; then
			CFLAGS_ARRAY+=("-pg")
		elif [[ "${COMPILER}" == "clang" ]]; then
			CFLAGS_ARRAY+=("-fprofile-instr-generate" "-fcoverage-mapping")
			LDFLAGS_ARRAY+=("-fprofile-instr-generate")
		fi
	##
	# Sanitizer flags
	#
	elif [[ "${BUILD_TYPE}" == *"san" && ("${SYSTEM}" == "linux" || "${SYSTEM}" == "macos") ]]; then
        CFLAGS_ARRAY+=("-g" "-O1")
		SAN_ARRAY=("")

		if [[ "${COMPILER}" == "clang" ]]; then case "${BUILD_TYPE}" in

			# working!
			msan) SAN_ARRAY+=("-fsanitize-recover=all" "-fsanitize=memory" "-fno-omit-frame-pointer");;
			usan) SAN_ARRAY+=("-fsanitize-recover=all" "-fsanitize=undefined");;

			# broken, but leaving active for fixing
			asan) SAN_ARRAY+=("-fsanitize=address");;
			# ./configure fails...
			# Direct leak of 6 byte(s) in 1 object(s) allocated from:
			#  0 0x4c6cb3 in __interceptor_malloc /build/llvm-toolchain-8-F3l7P1/llvm-toolchain-8-8/projects/compiler-rt/lib/asan/asan_malloc_linux.cc:146:3
			#  1 0x4f6159 in my_strdup /ssd_backup/src/dosbox-staging/conftest.c:24:25
			#  2 0x7f89450c8b6a in __libc_start_main /build/glibc-KRRWSm/glibc-2.29/csu/../csu/libc-start.c:308:16

			# broken, but leaving active for fixing
			uasan) SAN_ARRAY+=("-fsanitize-recover=all" "-fsanitize=address,undefined");;
			# ./configure fails...
			# Direct leak of 6 byte(s) in 1 object(s) allocated from:
			# 0 0x4c6cb3 in __interceptor_malloc /build/llvm-toolchain-8-F3l7P1/llvm-toolchain-8-8/projects/compiler-rt/lib/asan/asan_malloc_linux.cc:146:3
			# 1 0x4f6159 in my_strdup /ssd_backup/src/dosbox-staging/conftest.c:24:25
			# 2 0x7f77dc057b6a in __libc_start_main /build/glibc-KRRWSm/glibc-2.29/csu/../csu/libc-start.c:308:16

			tsan) usage "tsan is currently unavailable for clang";;
	        *) usage "Unknown sanitizer: ${BUILD_TYPE}";;
		    esac

		elif [[ "${COMPILER}" == "gcc" ]]; then case "${BUILD_TYPE}" in

			# Working!
			usan)  SAN_ARRAY+=("-fsanitize-recover=signed-integer-overflow" "-fsanitize=undefined");;
			uasan) SAN_ARRAY+=("-fsanitize-recover=signed-integer-overflow" "-fsanitize=address,undefined");;
			tsan)  SAN_ARRAY+=("-fsanitize=thread");;

			# broken, but leaving active for fixing
			asan)  SAN_ARRAY+=("-fsanitize=address");;
			# ./configure fails...
			# Direct leak of 6 byte(s) in 1 object(s) allocated from:
			#  0 0x7efc7f759448 in malloc (/usr/lib/x86_64-linux-gnu/libasan.so.5+0x10c448)
			#  1 0x401230 in my_strdup /ssd_backup/src/dosbox-staging/conftest.c:24

			msan) usage "msan is currently unavailable for gcc";;
	        *) usage "Unknown sanitizer: ${BUILD_TYPE}";;
		esac; fi
		CFLAGS_ARRAY+=("${SAN_ARRAY[@]}")
		LDFLAGS_ARRAY+=("${SAN_ARRAY[@]}")

	##
	#  Unknown release-type
	#
	else usage "The release type of ${BUILD_TYPE} is not valid; see the --release options below:"
	fi
}

function threads() {
	if [[ "${THREADS}" == "auto" ]]; then
		if exists nproc; then THREADS="$(nproc)"
		else THREADS="$(sysctl -n hw.physicalcpu || echo 4)"; fi
	fi
	# make presents a descriptive error message in the scenario where the user overrides
	# THREADS with an illegal value: the '-j' option requires a positive integer argument.
}

function query_compiler_version() {
	uses compiler_type
	uses compiler_version
	if [[ "${COMPILER_VERSION}" == "unset" ]]; then
		if   [[ "${COMPILER}" == "gcc"   ]]; then 2>&1 gcc -v
		elif [[ "${COMPILER}" == "clang" ]]; then clang --version
		fi | grep -Po '(?<=version )[^.]+'
	else
		echo "${COMPILER_VERSION:0:1}"
	fi
}

function fdo_flags() {
	if [[ "${FDO}" != "true" ]]; then
		return
	fi

	uses compiler_type
	uses src_path
	local fdo_file="${SRC_PATH}/scripts/profile-data/${COMPILER}.profile"
	if [[ ! -f "${fdo_file}" ]]; then
		error "The Feedback-Directed Optimization file provided (${fdo_file}) does not exist or could not be accessed"
	fi

	if [[ "${COMPILER}" == "gcc" ]]; then
		# Don't let GCC 6.x and under use both FDO and LTO
		if [[ "${LTO}" == "true" && "$(query_compiler_version)" -lt "7" ]]; then
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
	uses build_type_flags
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
	local fdo_string=""
	if [[ "${LTO}" == "true" ]]; then lto_string="-LTO"; fi
	if [[ "${FDO}" == "true" ]]; then fdo_string="-FDO"; fi

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
	echo "Build type: ${SYSTEM}-${MACHINE}-${BITS}bit-${COMPILER}-${BUILD_TYPE}${lto_string}${fdo_string}"
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
	if [[ "${SYSTEM}" == "msys2" ]]; then EXECUTABLE+="dosbox.exe"
	else EXECUTABLE+="dosbox"
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
	if [[ "${BUILD_TYPE}" == "release" ]]; then
		uses bin_path
		uses executable
		strip "${EXECUTABLE}"
	else
		echo "[skipping strip] symbols will be left in the binary"
	fi
}

function show_binary() {
	uses bin_path
	uses system
	uses executable

	echo ""
	echo "Dependencies"
	echo "------------"
	if [[ "$SYSTEM" == "macos" ]]; then otool -L "${EXECUTABLE}"
	else ldd "${EXECUTABLE}"; fi

	echo ""
	echo "Binary"
	echo "------"
	ls -1lh "${EXECUTABLE}"
}

function show_tips() {
	if [[ "${BUILD_TYPE}" == "profile" ]]; then
		uses compiler_type
		uses executable
		echo ""
		echo "Profiling Instructions"
		echo "----------------------"
		if [[ "${COMPILER}" == "gcc" ]]; then
			echo "  1. Run DOSBox like normal; it will write gmon.out in the current directory"
			echo "  2. Print the call statistics: gprof ${EXECUTABLE} gmon.out"

		elif [[ "${COMPILER}" == "clang" ]]; then
			local ver="$(query_compiler_version)"
			echo "  1. export LLVM_PROFILE_FILE=./llvm.prof"
			echo "  2. Run DOSBox like like normal; it will write llvm.prof in the current directory"
			echo "  3. Convert the prof file to a merge file: llvm-profdata-${ver} merge -output=llvm.merge -instr llvm.prof"
			echo "  4. Print the call statistics: llvm-profdata-${ver} show -all-functions -counts -ic-targets  llvm.merge"
		fi
		echo ""
	fi
}

function main() {
	parse_args "$@"
	build
	strip_binary
	show_binary
	show_tips
}

main "$@"
