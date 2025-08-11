#!/bin/bash

# SPDX-FileCopyrightText:  2020-2021 The DOSBox Staging Team
# SPDX-FileCopyrightText:  2020-2021 kcgen <kcgen@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This is a convenient helper to run mdl (markdownlint) on the
# *.md files in the repo, except for those inside directories
# that are likely to hold 3rd party documentation.
#
# You can pass additional mdl arguments to this script itself, e.g.:
#
#   ./verify-markdown.sh --verbose --json

set -euo pipefail

list_markdown_files () {
	git ls-files -- \
	  '*.md' \
	  ':!:.github/*.md' \
	  ':!:.github/**/*.md' \
	  ':!:src/libs/*.md' \
	  ':!:src/hardware/reelmagic/docs/*.md' \
	  ':!:resources/*.md' \
	  ':!:resources/translations/*.md'
}

main () {
	repo_root="$(git rev-parse --show-toplevel)"
	mdl --version
	echo "Checking files:"
	list_markdown_files
	list_markdown_files | xargs -L 1000 mdl --style "$repo_root/.mdl-styles" "$@"
}

>&2 main "$@"
