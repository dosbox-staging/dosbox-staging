#!/usr/bin/env bash

# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2023-2023  The DOSBox Staging Team
#
# Compiles all branch commits from the current commit backward.
# It Automatically uses the latest supported version of GCC and Clang,
# building with both compilers if available.
#
# When a failure occurs, the log is printed and the problematic commit
# will be checked out, so you can inspect and amend the commit.
#
# CLI tools used:
# - Bash 3.x or Zsh, for enhanced error handling versus SH
# - GNU ls, for natural-sorting the native files by version
# - GNU sed, to parse the default branch
# - Git, (any version should be fine)
# - All the project's build depenencies (Meson, Ninja, etc)
#

set -euo pipefail

# Common variables that are setup and used below
declare temp_file
declare head_commit
declare current_branch
declare -a commits
declare -a compilers
declare meson_setup

# GNU tools
declare LS
declare SED

populate_gnu_ls() {
	# shellcheck disable=2010
	if \ls --version 2> /dev/null | grep -q GNU; then
		LS=$(command -v ls)

	elif command -v gdir > /dev/null; then
		LS=$(command -v gdir)

	else
		echo "Please install corutils with your package manager and then:"
		echo " - Adjust your PATH to find GNU 'ls' first, or"
		echo " - Symlink 'ls' to your GNU 'ls' binary, or"
		echo " - If on macOS, simply 'brew install coreutils' and you're done!"
		exit 1
	fi
}

populate_gnu_sed() {
	if \sed --version 2> /dev/null | grep -q GNU; then
		SED=$(command -v sed)

	elif command -v gsed > /dev/null; then
		SED=$(command -v gsed)

	else
		echo "Please install GNU sed with your package manager."
		echo " - If on macOS, simply 'brew install gnu-sed' and you're done!"
		exit 1
	fi
}

extract_cpp_from_native_file() {
	grep cpp "$1" | cut -d "'" -f 4
}

get_native_file_for_compiler() {
	local compiler
	compiler="$1"

	populate_gnu_ls

	local native_file
	for native_file in $("$LS" \
	                     --reverse \
	                     --sort=version \
	                     .github/meson/native-"$compiler"*.ini); do

		local cpp
		cpp=$(extract_cpp_from_native_file "$native_file")
		if command -v "$cpp" &> /dev/null; then
			echo "$native_file"
			return
		fi
	done
}

allocate_temp_file() {
	temp_file=$(mktemp 2> /dev/null || mktemp -t temp)
	echo "Realtime log: $temp_file"

	# Cleanup the log on termination
	trap 'rm -f "$temp_file"' EXIT
}

run() {
	# Split the command into an array
	local -a cmds
	IFS=" " read -r -a cmds <<< "$1"

	# Execute the command with arguments
	if ! "${cmds[@]}" &> "$temp_file"; then
		echo "failed"
		echo ""
		echo "---"
		cat "$temp_file"
		echo "---"
		echo ""
		return 1
	fi
	return 0
}

populate_commits() {

	populate_gnu_sed

	current_branch=$(git rev-parse --abbrev-ref HEAD)

	local default_branch
	default_branch=$(git symbolic-ref refs/remotes/origin/HEAD 2> /dev/null \
		| "$SED" 's@^refs/remotes/origin/@@')

	if [[ -z $default_branch ]]; then
		echo "Failed to determine the default branch."
		exit 1
	fi

	commits=()
	while IFS= read -r line; do
		commits+=("$line")
	done < <(git log --format="%h" HEAD --not "$default_branch" --reverse)

	if ((${#commits[@]} <= 1)); then
		echo "Branch does not have multiple commits; skipping"
		exit 0
	fi

	head_commit="${commits[0]}"
}

meson_setup_for_compiler() {
	local compiler
	compiler="$1"

	local native_file
	native_file=$(get_native_file_for_compiler "$compiler")

	if [[ -z $native_file ]]; then
		echo "No supported $compiler versions found, skipping"
		return 1
	fi

	echo "Using $compiler native file: $native_file"

	local options
	options="-Dbuildtype=debug -Dunit_tests=disabled -Dwarning_level=0 -Dc_args=-w -Dcpp_args=-w"

	meson_setup="meson setup $options --native-file=$native_file build/$compiler"
	if ! run "$meson_setup"; then
		echo "Reproduce with:"
		echo "$meson_setup"
		echo ""
		return 1
	fi
	return 0
}

populate_compilers() {
	compilers=()
	if meson_setup_for_compiler gcc; then
		compilers+=(gcc)
	fi
	if meson_setup_for_compiler clang; then
		compilers+=(clang)
	fi
	if ((${#compilers[@]} == 0)); then
		echo "No supported compilers found, quitting"
		exit 1
	fi
}

compile_commit() {
	local compiler
	compiler="$1"

	local commit
	commit="$2"

	local title

	local meson_compile
	meson_compile="meson compile -C build/$compiler"

	if ! run "$meson_compile"; then
		echo "Reproduce with:"
		echo "git checkout $head_commit"
		echo "$meson_setup"
		echo "git checkout $commit"
		echo "$meson_compile"
		exit 1
	fi

	echo -n " $compiler"
}

main() {
	allocate_temp_file

	populate_commits

	populate_compilers

	for commit in "${commits[@]}"; do

		git checkout "$commit" &> /dev/null

		title=$(git show \
		        --no-patch \
		        --format="%s" "$commit")

		printf "Compiling %s %-40.40s.." "$commit" "$title"

		for compiler in "${compilers[@]}"; do
			compile_commit "$compiler" "$commit"
		done
		echo ""

	done

	git checkout "$current_branch" &> /dev/null
}

main
