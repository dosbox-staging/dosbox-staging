#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2023 The DOSBox Staging Team
# Copyright (C) 2020-2021 Patryk Obara <patryk.obara@gmail.com>

set -euo pipefail

PACKAGED_BUILD="build/lang"

LNG_DIR="contrib/resources/translations"

EN_LANG_PATH="$LNG_DIR/en.lng"

cd "$(git rev-parse --show-toplevel)" || exit

check_package() {
	if [[ -d "$PACKAGED_BUILD" ]]; then
		return 0
	fi
	echo "Perform the following steps to build Staging statically"
	echo "and package it into the 'build/lang' directory:"
	echo ""
	echo "  1. cd $(git rev-parse --show-toplevel)"
	echo ""
	echo "  2. meson setup -Dunit_tests=disabled build/full-static"
	echo ""
	echo "  3. meson compile -C build/full-static"
	echo ""
	echo "  4. ./scripts/packaging/create-package.sh -f -p linux build/full-static build/lang"
	echo ""
	echo "  5. cd contrib/resources/translations"
	echo ""
	echo "  6. ./update-sources.sh"
	exit 1
}

update() {
	local -r lang="$1"
	local -r lng_path="$LNG_DIR/${lang}.lng"

	# If the languge is english, then we zero-out the en.lng file
	# to ensure we get only those strings defined int the source-code
	#
	if [[ "$lang" == "en" ]]; then
		:> "$lng_path"
	fi

	# Ensure the language file exists
	#
	if [[ ! -f "$lng_path" ]]; then
		echo "Language file $lng_path is missing"
		exit 1
	fi

	# List messages that no longer exist in the English version
	# and therefore can be safely deleted from the translated source
	#
	if [[ "$lang" != "en" ]]; then
		echo "Messages that can be deleted from $lng_path"
		comm -1 -3 <(grep ^: "$EN_LANG_PATH" | sort -u) <(grep ^: "$lng_path" | sort -u)
	fi

	# Dump the messages from source into the .lng file
	#
	./build/lang/dosbox \
		-noprimaryconf \
		-nolocalconf \
		-lang "$lang" \
		-c "config -wl $lng_path" \
		-c 'exit' \
		&> /dev/null

	# NFC-normalize the translation file - for UTF-8 format locale
	#
	temp_file="$(mktemp)"
	uconv -f UTF-8 -t UTF-8 -x '::nfc;' "$lng_path" > "$temp_file"
	mv "$temp_file" "$lng_path"
}

# Staging needs to be packaged up (using the create-pacakge.sh) script
# to use the simpler -lang loader format supported by 0.78+.
#
git status

# Because English is the source language from which others are
# translated, we first dump the latest English translations.
#
update en

# After this, we update the other language files which will inject
# any missing messages into them in English, so translators can easily
# find and update those new messages.
#
update br
update de
update es
update fr
update it
update nl
update pl
update ru

set +x

echo
echo "Now run 'git add -p' and carefully update the translation files."
