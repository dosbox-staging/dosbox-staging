#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Kevin R. Croft <krcroft@gmail.com>
 
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
	repo_root="$(git rev-parse --show-toplevel)"
	mdl --version
	echo "Checking files:"
	list_markdown_files
	list_markdown_files | xargs -L 1000 mdl --style "$repo_root/.mdl-styles" "$@"
}

>&2 main "$@"
