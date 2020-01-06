#!/bin/bash

# Copyright (C) 2020  Kevin Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Based on `verify-bash.sh` by Patryk Obara <patryk.obara@gmail.com>
# Copyright (C) 2019 licensed GPL-2.0-or-later
#
# This script exists only to easily run mdl (markdownlint) on all
# *.md files in the repo.
#
# You can pass additional parameters to this script itself, e.g.:
#
#   ./verify-markdown.sh --verbose --json

set -euo pipefail

list_markdown_files () {
	git ls-files | grep '.md$'
}

main () {
	mdl --version
	echo "Checking files:"
	list_markdown_files
	list_markdown_files | xargs -L 1000 mdl "$@"
}

>&2 main "$@"
