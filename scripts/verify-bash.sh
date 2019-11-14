#!/bin/bash

# Copyright (c) 2019 Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script exists only to easily run shellcheck on all files in the repo.
# You can pass additional parameters to this script itself, e.g.:
#
# $ ./verify-bash.sh --format=json

run_shellcheck () {
	git ls-files \
		| xargs file \
		| grep "Bourne-Again shell script" \
		| cut -d ':' -f 1 \
		| xargs -L 1000 shellcheck "$@"
}

shellcheck --version
run_shellcheck "$@"
