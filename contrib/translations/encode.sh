#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2020  Patryk Obara <patryk.obara@gmail.com>
#
# Encode translation files in old .lng format from utf-8 to the codepage
# appropriate in DOS for a particular language.

# Literal conversion - for languages that can be fully encoded
tconv_l () {
	echo "Encoding $2 with $1 to $3.lng"
	iconv -f UTF-8 -t "$1" "$2" > "$3.lng"
}

# Transliteration - lossy encoding, some letters will be replaced
tconv_t () {
	echo "Encoding $2 with $1 to $3.$1.lng"
	iconv -f UTF-8 -t "$1//TRANSLIT" "$2" > "$3.$1.lng"
}

trans_dir=$(dirname "$0")

pushd "$trans_dir" > /dev/null || exit

echo "In directory $trans_dir:"

# (default)
# keyb us 437
# keyb us 858
tconv_l CP437 "utf-8/en/en-0.77.0-alpha.lng" en/en_US

# keyb fr
# keyb fr 437
# keyb fr 858
tconv_l CP858 "utf-8/fr/fr-0.77.0-alpha.lng" fr/fr_FR

# keyb pl
# keyb pl 852
tconv_l CP852 "utf-8/pl/pl-0.77.0-alpha.lng" pl/pl_PL

# keyb pl 437
tconv_t CP437 "utf-8/pl/pl-0.77.0-alpha.lng" pl/pl_PL

popd > /dev/null || exit
