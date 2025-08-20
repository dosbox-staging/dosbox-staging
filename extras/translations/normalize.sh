#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2025 The DOSBox Staging Team
# Copyright (C) 2020-2021 Patryk Obara <patryk.obara@gmail.com>

if ! which uconv > /dev/null 2>&1; then
    echo
    echo "You need install the uconv package!"
    echo
    echo "For Debian or Ubuntu the command is: 'apt install icu-devtools'"
    echo
    exit 1
fi

# Normalization - for UTF-8 format locale
#
nconv_t () {
	echo "Normalizing file $1"
	temp_file="$(mktemp)"
	uconv -f UTF-8 -t UTF-8 -x '::nfc;' "$1" -o "$temp_file"
	mv "$temp_file" "$1"
}

translation_dir=$(dirname "$0")/../../resources/translations
pushd "$translation_dir" > /dev/null || exit

echo "In directory $translation_dir:"

file_list="*.po"
for f in $file_list
do
	nconv_t "$f"
done

popd > /dev/null || exit
