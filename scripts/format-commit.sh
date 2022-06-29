#!/bin/bash

set -eu

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Patryk Obara <patryk.obara@gmail.com>

SCRIPT=$(basename "$0")
readonly SCRIPT

print_usage () {
	echo "usage: $SCRIPT [-V|--verify|-d|--diff|-a|--amend] [<commit>]"
	echo
	echo "Fixes formatting of C and C++ files, that were touched by "
	echo "commits since <commit>.  Changes are limited only to files and "
	echo "lines that were modified. If a file was moved/renamed and only "
	echo "slightly modified (less than 10%), then it's not reformatted."
	echo
	echo "If no <commit> is passed, then default is HEAD~1 - which means "
	echo "that only lines touched by the latest commit will be fixed, "
	echo "therefore usage of this script is simply:"
	echo
	echo "  $ ./$SCRIPT"
	echo
	echo "Files are formatted only if .clang-format file is located in one "
	echo "of the parent directories of the source file."
	echo
	echo "Optional parameter --diff (or --verify) displays diff of what was "
	echo "formatted and exits with a failure status if diff is not empty"
	echo "(this option is intended for CI usage):"
	echo
	echo "  $ ./$SCRIPT --diff"
	echo
	echo "Optional parameter --amend will amend the latest commit for you."
	echo
	echo "  $ ./$SCRIPT --amend"
	echo
	echo "If you want to format a whole file instead then use clang-format "
	echo "directly, e.g.:"
	echo
	echo "  $ clang-format path/to/file.cpp"
}

main () {
	case ${1:-} in
		-h|-help|--help) print_usage ;;
		-d|--diff)       handle_dependencies ; shift ; format "$@" ; assert_empty_diff ;;
		-V|--verify)     handle_dependencies ; shift ; format "$@" ; assert_empty_diff ;;
		-a|--amend)      handle_dependencies ; shift ; format "$@" ; amend ;;
		*)               handle_dependencies ; format "$@" ; show_tip ;;
	esac
}

handle_dependencies () {
	assign_gnu_sed
	assert_min_version git 1007010 "Use git version 1.7.10 or newer."
	assert_min_version clang-format 12000000 "Use clang-format version 12.0.0 or newer."
}

SED=""
assign_gnu_sed () {
	# Is sed GNU? (BSD seds don't support --version)
	if sed --version &>/dev/null; then
		SED="sed"
	# No, but do we have gsed?
	elif command -v gsed &> /dev/null; then
		SED="gsed"
	# No, so help the user install it and then quit.
	else
		echo "'sed' is not GNU and 'gsed' is not available."
		if [[ "${OSTYPE:-}" == "darwin"* ]]; then
			echo "Install GNU sed with: brew install gnu-sed"
		else
			echo "Install GNU sed with your $OSTYPE package manager."
		fi
		exit 1
	fi
}

assert_min_version () {
	if ! check_min_version "$1" "$2" ; then
		echo "$3"
		exit 1
	fi
}

check_min_version () {
	$1 --version
	$1 --version \
		| "$SED" -e "s|.* \([0-9]*\)\.\([0-9]*\)\.\([0-9]*\).*|\1 \2 \3|" \
		| test_version "$2"
	return "${PIPESTATUS[2]}"
}

test_version () {
	read -r major minor patch
	local -r version=$((major * 1000000 + minor * 1000 + patch))
	test "$1" -le "$version"
}

format () {
	local -r since_ref=${1:-HEAD~1}
	pushd "$(git rev-parse --show-toplevel)" > /dev/null
	echo "Using paths relative to: $(pwd)"
	find_cpp_files "$since_ref" | run_clang_format
	popd > /dev/null
}

assert_empty_diff () {
	if [[ -n "$(git_diff HEAD)" ]] ; then
		git_diff HEAD
		echo
		echo "clang-format formatted some code for you."
		echo
		echo "Run 'git commit -a --amend' to save the result."
		exit 1
	fi
}

show_tip () {
	if [[ -n "$(git_diff HEAD)" ]] ; then
		echo
		echo "clang-format formatted some code for you."
		echo
		echo "Run 'git diff' to see what changed."
		echo "Run 'git commit -a --amend' to save the result."
		exit 0
	fi
}

amend () {
	git commit -a --amend
}

git_diff () {
	git diff --no-ext-diff "$@"
}

find_cpp_files () {
	set +e
	list_changed_files "$1" | grep -E "\.(h|hpp|c|cpp|cc)$"
	set -e
}

list_changed_files () {
	git_diff \
		--no-renames \
		--diff-filter=AMrcd \
		--find-renames=90% \
		--find-copies=90% \
		--name-only \
		"$1"
}

run_clang_format () {
	while read -r src_file ; do
		local ranges=()
		while IFS=$'\n' read -r range ; do
			ranges+=("$range")
		done < <(git_diff_to_clang_line_range "$src_file")

		if (( "${#ranges[@]}" )); then
			echo "clang-format -i ${ranges[*]} \"$src_file\""
			clang-format -i "${ranges[@]}" "$src_file"
		fi
	done
}

git_diff_to_clang_line_range () {
	local -r file=$1
	git_diff --ignore-space-at-eol -U0 HEAD~1 "$file" \
		| grep -E "^@@" \
		| filter_line_range \
		| to_clang_line_range
}

# expects line in diff format: "@@ -<line range> +<line range> @@ <context>"
# where <line range> is either <line_number> or <line_number>,<offset>
#
filter_line_range () {
	"$SED" -e 's|@@ .* +\([0-9]\+\),\?\([0-9]\+\)\? @@.*|\1 \2|'
}

to_clang_line_range () {
	while read -r from_line offset ; do
		local to_line=$(( from_line + offset ))
		if [[ $from_line -gt 0 && $from_line -le $to_line ]] ; then
			echo "-lines=$from_line:$to_line"
		fi
	done
}

main "$@"
