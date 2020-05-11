#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script prints package dependencies for a given package manmager,
# compiler (gcc or clang), and bit-depth (32 or 64).
#
# If run without arguments, the script asks for required arguments one
# by one, which includes the package manager (--manager) and --compiler.
#
# Optional arguments include the bit-depth, defaults to 64-bit;
# and the compiler version, which defaults to the package manager's
# default version.
#
# Usage examples:
#   $ ./list-build-dependencies.sh               # asks for --manager
#   $ ./list-build-dependencies.sh --manager apt # asks for --compiler
#   $ ./list-build-dependencies.sh --manager apt --compiler gcc -v 9
#   $ ./list-build-dependencies.sh --manager zypper --compiler clang
#   $ ./list-build-dependencies.sh --manager msy2 --compiler gcc -b 32
#
# This script makes use of the automator package, see automator/main.sh.
#
set -euo pipefail

function parse_args() {
	# Gather the parameters that define this build
	bits="64"
	modifiers=("")
	excludes=("")
	# shellcheck disable=SC2034
	while [[ "${#}" -gt 0 ]]; do case ${1} in
		-m|--manager)           manager="${2}";                shift 2;;
		-c|--compiler)          selected_type=$(lower "${2}"); shift 2;;
		-b|--bit-depth)         bits="${2}";                   shift 2;;
		-v|--compiler-version)  version="${2}";                shift 2;;
		-a|--addon)             modifiers+=("${2}");           shift 2;;
		-x|--exclude)           excludes+=("${2}");            shift 2;;
		*) >&2 echo "Unknown parameter: ${1}"; exit 1;
	esac; done

	# Import our settings and report missing values
	import manager "${manager:-}"
	if [[ -z "${selected_type:-}" ]]; then arg_error "--compiler" "${TYPES[*]}"; fi

	# If a version was provided then construct a postfix string given the manager's
	# potential unique delimeter (ie: gcc@9 for brew, gcc-9 for apt).
	# shellcheck disable=SC2034,SC2154
	postfix=$([[ -n "${version:-}" ]] && echo "${delim}${version}" || echo "")
	import "${selected_type:-}" "${manager:-}"
}
# shellcheck disable=SC2034
data_dir="packages"

script_dir="$(cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P)"
# shellcheck source=scripts/automator/main.sh
source "$script_dir/automator/main.sh"
