#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Kevin R. Croft <krcroft@gmail.com>

# This simple script is used to test, and archive the log output from
# various sanitizer-builds.
#
# Usage:
#   ./run-sanitizers.sh BUILDDIR LOGDIR"
#
set -euo pipefail

# let run logs be printed to the output, we don't need them stored in artifacts
set -x

# Check the arguments
if [[ "$#" -lt 2 ]]; then
	echo "Usage: $0 BUILDDIR LOGDIR"
	exit 1
fi

# Defaults and arguments
build_dir="$1"
logs="$2"

# Move to the top of our source directory
cd "$(git rev-parse --show-toplevel)"

# Make a directory to hold our run output
mkdir -p "${logs}"

# SAN-specific environment variables
export LSAN_OPTIONS="suppressions=.lsan-suppress:verbosity=0"

# Sanitizers return non-zero if one or more issues were found,
# so we or-to-true to ensure our script doesn't end here.
# We'll detect issues by analyzing logs instead.
time xvfb-run "./${build_dir}/dosbox" \
	-c "autotype -w 2 e x i t enter" \
	&> "${logs}/EnterExit.log" || true

# Compress the logs
(
	cd "${logs}"
	xz -T0 -e ./*.log
)
