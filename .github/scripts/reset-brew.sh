#!/bin/bash

# Copyright (c) 2019 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script removes all existing brew packages, and then
# re-installs some important basic packages (listed below).
#
# Usage: ./reset-brew.sh
#
set -xuo pipefail
set +e

# Pre-cleanup size
sudo du -sch /usr/local 2> /dev/null

# Uninstall all packages
# shellcheck disable=SC2046
brew remove --force $(brew list) --ignore-dependencies
# shellcheck disable=SC2046
brew cask remove --force $(brew cask list)

# Reinstall important packages
brew install git git-lfs python curl wget jq binutils zstd gnu-tar

# Clean the brew cache
rm -rf "$(brew --cache)"

# Post-clean up size
sudo du -sch /usr/local 2> /dev/null

# This script is best-effort, so always return success
exit 0

