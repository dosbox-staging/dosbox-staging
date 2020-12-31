#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Kevin R. Croft <krcroft@gmail.com>

# This script installs and updates MacPorts from an already-compiled
# source tree assumed to be located n a directory called
# 'macports-base' off the root of our source-tree.
#
# Usage: ./install-and-update-macports.sh
#
set -xeuo pipefail

# Ensure we have sudo rights to install MacPorts
if [[ $(id -u) -ne 0 ]] ; then echo "Please run as root" ; exit 1 ; fi

# Move to the top of our source directory
cd "$(dirname "${0}")/../.."
(
	cd macports-base
	make install
)
# Purge the now-unecessary source
rm -rf macports-base

# Update our ports collection
/opt/local/bin/port -q selfupdate || true
