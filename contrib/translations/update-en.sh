#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Patryk Obara <patryk.obara@gmail.com>

set -x

cd "$(git rev-parse --show-toplevel)" || exit

./build/dosbox \
	-lang '' \
	-c 'config -wl contrib/translations/en/en_US.lng' \
	-c 'exit' \
	> /dev/null

iconv -f CP437 -t UTF-8 'contrib/translations/en/en_US.lng' \
	> 'contrib/translations/utf-8/en/en-0.77.0-alpha.txt'

set +x

echo
echo "Now run 'git add -p' and carefully update the translation files."
