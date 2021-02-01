#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Kevin R. Croft <krcroft@gmail.com>

# This simple script is used to build, test, and archive the log output from
# various sanitizer-builds.
#
# Usage:
#   ./build-and-run-sanitizers.sh COMPILER VERSION SANITIZER [SANITIZER [...]]"
#
# Examples:
#   ./build-and-run-sanitizers.sh gcc address undefined
#   ./build-and-run-sanitizers.sh clang thread
#
# SANITIZER values are the names supported by meson for b_sanitize build
# option: "address", "thread", "undefined", "memory", "address,undefined"
#
set -euo pipefail

# let build logs be printed to the output, we don't need them stored in artifacts
set -x

# Check the arguments
#if [[ "$#" -lt 3 ]]; then
#	echo "Usage: $0 COMPILER SANITIZER [SANITIZER [...]]"
#	exit 1
#fi

# Defaults and arguments
compiler="${1}"
logs="${compiler}-logs"
shift 1
sanitizers=("$@")

# Move to the top of our source directory
cd "$(git rev-parse --show-toplevel)"

# Make a directory to hold our build and run output
mkdir -p "${logs}"

# SAN-specific environment variables
export LSAN_OPTIONS="suppressions=.lsan-suppress:verbosity=0"

for sanitizer in "${sanitizers[@]}"; do

	# Build for each sanitizer
	meson setup -Db_sanitize="${sanitizer}" "${sanitizer}-build"
	ninja -C "${sanitizer}-build"

	# Exercise the testcase(s) for each sanitizer
	# Sanitizers return non-zero if one or more issues were found,
	# so we or-to-true to ensure our script doesn't end here.
	time xvfb-run "./${sanitizer}-build/dosbox" \
		-c "autotype -w 0.1 e x i t enter" \
		&> "${logs}/${sanitizer}-EnterExit.log" || true

done

# Compress the logs
(
	cd "${logs}"
	xz -T0 -e ./*.log
)
