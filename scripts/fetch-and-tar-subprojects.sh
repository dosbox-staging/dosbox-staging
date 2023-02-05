#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2023-2023  kcgen <kcgen@users.noreply.github.com>
#
# A simple script that fetches all non-repository subproject
# content and tars it to a given target directory and tar file.
#
# The resulting tar file can be used for all build types and
# platform types as the content is pre-setup just source.
#
# Compression is not used because it's compressed by zstd during
# CI's caching process (and double-zipping is slower).
#

set -euo pipefail

usage()
{
    printf "%s\n" "\
    Usage: $0 [TARFILE]

    The tar, git, and meson executables can be optionally
    overridden with upper-case environment variables, such as:
      TAR=gtar
      GIT=/usr/local/bin/git
      MESON=/usr/cross-arm64/bin/meson"
}

create_dirs_for()
{
	mkdir -p "$(dirname "$1")"
}

list_non_repo_subprojects_dirs()
{
	"$GIT" status --ignored --porcelain \
	| cut -d' ' -f2 \
	| grep subprojects/
}

tar_non_repo_subprojects_dirs()
{
	local tarfile="$1"

	[[ ! -f "$tarfile" ]]

	list_non_repo_subprojects_dirs \
	| "$TAR" -cf "$tarfile" --ignore-failed-read --files-from -

	test -f "$tarfile"
}

fetch_subprojects()
{
	test -d subprojects

	"$MESON" subprojects download
	"$MESON" subprojects update --reset
	"$MESON" subprojects foreach meson subprojects download

	while read -r dir; do
		(
			set -eu
			cd "$(dirname "$dir")"
			"$MESON" subprojects download
		)
	done < <(find . -type d -name subprojects)
}

setup_binaries()
{
	if [[ -z "${GIT:-}" ]]; then
		GIT=git
	else
		echo "Using GIT=$GIT"
	fi


	if [[ -z "${TAR:-}" ]]; then
		if tar --version | grep -q GNU; then
			TAR=tar
		else
			TAR=gtar
		fi
	else
		echo "Using TAR=$TAR"
	fi


	if [[ -z "${MESON:-}" ]]; then
		MESON=meson
	else
		echo "Using MESON=$MESON"
	fi
}

main()
{
	if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
		usage
		exit 0
	fi

	if [[ -z "${1:-}" ]]; then
		local tarfile="subprojects.tar"
	else
		local tarfile="$1"
	fi

	create_dirs_for "$tarfile"

	setup_binaries

	fetch_subprojects

	tar_non_repo_subprojects_dirs "$tarfile"
}

>&2 main "$@"
