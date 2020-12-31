#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Kevin R. Croft <krcroft@gmail.com>

# This script downloads and builds MacPorts from source.
# Usage: ./download-and-build-macports.sh
#
set -xeuo pipefail

# Move to the top of our source directory
cd "$(dirname "${0}")/../.."

# Download and install MacPorts
git clone --quiet --depth=1 https://github.com/macports/macports-base.git
(
	cd macports-base
	./configure
	make -j"$(sysctl -n hw.physicalcpu || echo 4)"
)
