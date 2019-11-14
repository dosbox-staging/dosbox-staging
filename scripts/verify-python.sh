#!/bin/bash

# Copyright (c) 2019 Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script exists only to easily run python static checker on all files in
# the repo.  You can pass additional parameters to this script itself, e.g.:
#
# $ ./verify-python.sh --disable=<msg_ids>

run_pylint () {
	git ls-files \
		| xargs file \
		| grep "Python script" \
		| cut -d ':' -f 1 \
		| xargs -L 1000 pylint "$@"
}

pylint --version
run_pylint --rcfile="$(git rev-parse --show-toplevel)/.pylint" "$@"
