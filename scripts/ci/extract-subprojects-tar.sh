#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2023-2023  kcgen <kcgen@users.noreply.github.com>
#
# A trivial script that extracts previously fetched and tarred
# subproject content.

set -euo pipefail

usage()
{
    printf "%s\n" "\
    Usage: $0 [TARFILE]

    The tar executable can be optionally overridden by setting
    the TAR environment variables, such as: TAR=gtar"
}

setup_tar()
{
	if [[ -z "${TAR:-}" ]]; then
		if tar --version | grep -q GNU; then
			TAR=tar
		else
			# Fallback to gtar; if that's not found the user
			# will get a verbose command-not-found error.
			TAR=gtar
		fi
	else
		echo "Using TAR=$TAR"
	fi

	if [[ "${OSTYPE:-}" == "msys" ]]; then
		# If not set, tar fails when extracting symlinks
		export MSYS=winsymlinks:lnk
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

	setup_tar

	"$TAR" -xf "$tarfile"
}

>&2 main "$@"
