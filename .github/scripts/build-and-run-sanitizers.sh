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
#   ./build-and-run-sanitizers.sh clang 8 msan usan
#   ./build-and-run-sanitizers.sh gcc 9 asan uasan usan tsan
#
set -euo pipefail

# Check the arguments
if [[ "$#" -lt 3 ]]; then
	echo "Usage: $0 COMPILER VERSION SANITIZER [SANITIZER [...]]"
	exit 1
fi

# Defaults and arguments
compiler="${1}"
compiler_version="${2}"
logs="${compiler}-logs"
shift 2
sanitizers=("$@")

# Move to the top of our source directory
cd "$(dirname "${0}")/../.."

# Make a directory to hold our build and run output
mkdir -p "${logs}"

# SAN-specific environment variables
export LSAN_OPTIONS="suppressions=.lsan-suppress:verbosity=0"

for sanitizer in "${sanitizers[@]}"; do

	# Build DOSBox for each sanitizer
	time ./scripts/build.sh \
		--compiler "${compiler}" \
		--version-postfix "${compiler_version}" \
		--build-type "${sanitizer}" \
		&> "${logs}/${compiler}-${sanitizer}-compile.log"

	# Exercise the testcase(s) for each sanitizer
	# Sanitizers return non-zero if one or more issues were found,
	# so we or-to-true to ensure our script doesn't end here.
	time xvfb-run ./src/dosbox -c "autotype -w 0.1 e x i t enter" \
		&> "${logs}/${compiler}-${sanitizer}-EnterExit.log" || true

done

# Compress the logs
(
	cd "${logs}"
	xz -T0 -e ./*.log
)
