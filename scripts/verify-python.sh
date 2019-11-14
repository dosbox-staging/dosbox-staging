#!/bin/bash

# Copyright (c) 2019 Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script exists only to easily run python static checker on all files in
# the repo.  You can pass additional parameters to this script itself, e.g.:
#
# $ ./verify-python.sh --disable=<msg_ids>

list_python_files () {
	git ls-files \
		| xargs file \
		| grep "Python script" \
		| cut -d ':' -f 1
}

main () {
	pylint --version >&2
	echo "Checking files:" >&2
	list_python_files >&2
	local -r rc="$(git rev-parse --show-toplevel)/.pylint"
	list_python_files | xargs -L 1000 pylint --rcfile="$rc" "$@"
}

main "$@"
