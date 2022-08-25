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
# keyb us 437
tconv_l CP437 "utf-8/en.txt" en
nconv_t CP437 "utf-8/en.txt" en.utf8

# keyb de
# keyb de 850
tconv_l CP850 "utf-8/de.txt" de
nconv_t UTF-8 "utf-8/de.txt" de.utf8

# keyb es
# keyb es 850
tconv_l CP850 "utf-8/es.txt" es
nconv_t UTF-8 "utf-8/es.txt" es.utf8

# keyb fr
# keyb fr 850
tconv_l CP850 "utf-8/fr.txt" fr
nconv_t UTF-8 "utf-8/fr.txt" fr.utf8

# keyb it
# keyb it 850
tconv_l CP850 "utf-8/it.txt" it
nconv_t UTF-8 "utf-8/it.txt" it.utf8

# keyb nl
# keyb nl 850
# keyb nl 858
tconv_l CP850 "utf-8/nl.txt" nl

# keyb pl
# keyb pl 852
# keyb pl 437
tconv_l CP852 "utf-8/pl.txt" pl
tconv_t CP437 "utf-8/pl.txt" pl.cp437
nconv_t UTF-8 "utf-8/pl.txt" pl.utf8

# keyb ru
# keyb ru 866
tconv_l CP866 "utf-8/ru.txt" ru
nconv_t UTF-8 "utf-8/ru.txt" ru.utf8

popd > /dev/null || exit
