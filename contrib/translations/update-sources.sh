#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Patryk Obara <patryk.obara@gmail.com>

set -euo pipefail

VERSION="0.78.0-alpha"

LNG_DIR="contrib/translations"

EN_LANG_PATH="$LNG_DIR/en/en_US.lng"

OUTPUT_DIR="$LNG_DIR/utf-8"

cd "$(git rev-parse --show-toplevel)" || exit

update() {
	local -r lang="$1"
	local -r dialect="$2"
	local -r codepage="$3"
	local -r postfix="${4:-}"
	local -r lng_path="$LNG_DIR/$lang/${lang}_${dialect}${postfix}.lng"
	local -r txt_path="$OUTPUT_DIR/$lang/$lang-$VERSION.txt"

	# If the languge is english, then we zero-out the en.lng file
	# to ensure we get only those strings defined int the source-code
	#
	if [[ "$lang" == "en" ]]; then
		:> "$lng_path"
	fi

	# Ensure the language file exists
	if [[ ! -f "$lng_path" ]]; then
		echo "Language file $lng_path is missing"
		exit 1
	fi

	# Dump the messages from source into the .lng file
	./build/dosbox \
		-lang "$lng_path" \
		-c "config -wl $lng_path" \
		-c 'exit' \
		&> /dev/null

	# List messages that no longer exist in the English version
	# and therefore can be safely deleted from the translated source.
	echo "Messages that can be deleted from $txt_path"
	comm -1 -3 <(grep ^: "$EN_LANG_PATH" | sort -u) <(grep ^: "$lng_path" | sort -u)

	# Convert the lng to text, which is what translators use
	iconv -f CP"$codepage" \
		-t UTF-8 "$lng_path" \
		> "$txt_path"

	# Finally, restore the non-English .lng files.
	#
	if [[ "$lang" != "en" ]]; then
		git restore "$lng_path"
	fi
}

# Because English is the source language from which others are
# translated, we first dump the latest english translations.
#
update en US 437

# After this, we update the other language files which will inject
# any missing messages into them in english, so translators can easily
# find and update those new messages.
#
update de DE 858
update es ES 858
update fr FR 858
update it IT 858
update pl PL 437 .CP437
update pl PL 852
update ru RU 866

set +x

echo
echo "Now run 'git add -p' and carefully update the translation files."
