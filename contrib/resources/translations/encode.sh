#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Patryk Obara <patryk.obara@gmail.com>

# Encode translation files in old .lng format from utf-8 to the codepage
# appropriate in DOS for a particular language.

# Literal conversion - for languages that can be fully encoded
tconv_l () {
	echo "Encoding $2 with $1 to $3.lng"
	iconv -f UTF-8 -t "$1" "$2" > "$3.lng"
}

# Transliteration - lossy encoding, some letters will be replaced
tconv_t () {
	echo "Encoding $2 with $1 to $3.lng"
	iconv -f UTF-8 -t "$1//TRANSLIT" "$2" > "$3.lng"
}

# Normalization - for UTF-8 format locale
nconv_t () {
	echo "Encoding $2 with $1 to $3.lng"
	uconv -f "$1" -t UTF-8 -x '::nfc;' "$2" > "$3.lng"
}

trans_dir=$(dirname "$0")

pushd "$trans_dir" > /dev/null || exit

echo "In directory $trans_dir:"

# (default)
# keyb us
nconv_t CP437 "utf-8/en.txt" en

# keyb de
nconv_t UTF-8 "utf-8/de.txt" de

# keyb es
nconv_t UTF-8 "utf-8/es.txt" es

# keyb fr
nconv_t UTF-8 "utf-8/fr.txt" fr

# keyb it
nconv_t UTF-8 "utf-8/it.txt" it

# keyb nl
nconv_t UTF-8 "utf-8/nl.txt" nl

# keyb pl
nconv_t UTF-8 "utf-8/pl.txt" pl

# keyb ru
nconv_t UTF-8 "utf-8/ru.txt" ru

popd > /dev/null || exit
