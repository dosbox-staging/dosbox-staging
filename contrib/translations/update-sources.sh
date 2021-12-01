#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Patryk Obara <patryk.obara@gmail.com>

set -euo pipefail

VERSION="0.78.0-alpha"

PACKAGED_BUILD="build/lang"

LNG_DIR="contrib/translations"

EN_LANG_PATH="$LNG_DIR/en/en_US.lng"

OUTPUT_DIR="$LNG_DIR/utf-8"

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
	echo "  2. meson setup --wrap-mode=forcefallback -Db_asneeded=true \\"
	echo "                 -Ddefault_library=static -Dtry_static_libs=fluidsynth,mt32emu,png \\"
	echo "                 -Dfluidsynth:enable-floats=true -Dfluidsynth:try-static-deps=true \\"
	echo "                 -Db_asneeded=true -Dunit_tests=disabled -Dstrip=true \\"
	echo "                 -Dwarning_level=0 -Dc_args=-w -Dcpp_args=-w build/full-static"
	echo ""
	echo "  3. meson compile -C build/full-static"
	echo ""
	echo "  4. ./scripts/create-package.sh -f -p linux build/full-static build/lang"
	echo ""
	echo "  5. cd contrib/translations"
	echo ""
	echo "  6. ./update-sources.sh"
	exit 1
}

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
	./build/lang/dosbox \
		-lang "$lang" \
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

# Staging needs to be packaged up (using the create-pacakge.sh) script
# to use the simpler -lang loader format supported by 0.78.
check_package

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
