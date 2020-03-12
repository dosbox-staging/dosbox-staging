#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# A helper script that fetches, converts, and merges kernel sample
# (.prof) files (collected during prior DOSBox testing) into a single
# GCC-compatible AutoFDO record that can be used to optimize builds.

# For simplicity, this script is dosbox-staging-specific and
# github-workflow-specific.

# Depedencies:
#   - zstd
#   - GNU parallel
#   - https://github.com/google/autofdo, install locally in ./afdo
#     using the same compiler that will use the AutoFDO records

set -euo pipefail

# The tarball containing one or more profile records
PROFILES="https://gitlab.com/luxtorpeda/dosbox-tests/-/raw/master/archives/profiles.tar.zst"
BINARY="tests/dosbox"

# Move to our repo root
cd "$(git rev-parse --show-toplevel)"

# Fetch and unpack the profiles
wget "${PROFILES}" -O - | zstd -d | tar -x

# Convert and merge the profiles
parallel ./afdo/bin/create_gcov --binary="${BINARY}" --profile="{}" -gcov="{.}".afdo -gcov_version=1 ::: tests/*/*.prof
./afdo/bin/profile_merger -gcov_version=1 -output_file=current.afdo tests/*/*.afdo
