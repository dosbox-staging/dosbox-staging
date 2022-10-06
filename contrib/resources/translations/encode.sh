#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022       The DOSBox Staging Team
# Copyright (C) 2020-2021  Patryk Obara <patryk.obara@gmail.com>


# Normalization - for UTF-8 format locale
nconv_t () {
	echo "Normalizing $1 to $2.lng"
	uconv -f UTF-8 -t UTF-8 -x '::nfc;' "$1" > "$2.lng"
}

trans_dir=$(dirname "$0")

pushd "$trans_dir" > /dev/null || exit

echo "In directory $trans_dir:"

# (default)
# keyb us
nconv_t "utf-8/en.txt" en

# keyb de
nconv_t "utf-8/de.txt" de

# keyb es
nconv_t "utf-8/es.txt" es

# keyb fr
nconv_t "utf-8/fr.txt" fr

# keyb it
nconv_t "utf-8/it.txt" it

# keyb nl
nconv_t "utf-8/nl.txt" nl

# keyb pl
nconv_t "utf-8/pl.txt" pl

# keyb ru
nconv_t "utf-8/ru.txt" ru

popd > /dev/null || exit
