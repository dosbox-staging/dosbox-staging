#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Patryk Obara <patryk.obara@gmail.com>

# git-rlog.sh - Show git log decorated with SVN revisions
#
# Usage: git-rlog.sh [<options>] [<revision range>] [[--] <path>…]
#
# Example: $ git-rlog.sh origin/svn/trunk
#
# Accepts the same options as git-log (see: git-log(1) for details).
# Revision range are documented in man page: gitrevisions(7).
#
# You can either use this script directly or add it to your git aliases in
# ~/.gitconfig or .git/config:
#
#     [alias]
#     rlog = !$(git rev-parse --show-toplevel)/scripts/git-rlog.sh
#
# With alias configured, you will be able to invoke it as other git commands:
#
#     $ git rlog
#
# You can read more about aliases in the manual: git-config(1)

max () {
	local -r x=$1
	local -r y=$2
	((x > y)) && echo "$x" || echo "$y"
}

# This is a simple invocation of git-log with a very specific message
# formatting used. Formatting of log messages is explained in detail
# in the manual page: git-log(1), section "PRETTY FORMATS".
#
git_log () {
	read -r _ term_cols < <(stty size)
	local -r cols_available_for_msg_subject=$((term_cols - 54))
	local -r s=$(max 25 $cols_available_for_msg_subject)
	local -r fmt="%h %cd %<(20,trunc)%cn [[%(trailers)%-]] %<($s,trunc)%s"
	git log --date="format:%Y-%m-%d %H:%M" --format="$fmt" "$@"
}

# This terrible sed expression rips out SVN revision out of SVN path
# and ignores trailers other than "Imported-from". This expression depends
# on trailer being surrounded with "[[" and "]]" markers.
#
sed_trailer () {
	sed 's/\[\[\(Imported-from: .*@\([0-9]\+\)\|.*\)\]\]/[\2]/'
	#      ~~~~                 ~~   ~~~~~~~        ~~~~
	#      ↑                    ↑    ↑              ↑
	#      "[["                 path revision       "]]"
}

# main function; piece it together and pass to the pager, keeping behaviour
# of the default Git pager (sans all user overrides possible).
#
main () {
	git_log "$@" | sed_trailer | LESS=FRX less -S
}

main "$@"
