#!/bin/bash

# Copyright (c) 2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# A helper script that fetches, converts, and merges kernel sample
# (.prof) files (collected during prior DOSBox testing) into a single
# LLVM-compatible Raw Profile record that can be used to optimize builds.

# For simplicity, this script is dosbox-staging-specific and
# github-workflow-specific.

# Depedencies:
#   - zstd
#   - GNU parallel
#   - https://github.com/google/autofdo, installed locally in ./afdo
#     configured to use same LLVM that will use the AutoFDO records

set -euo pipefail

# The tarball containing one or more profile records
PROFILES="https://gitlab.com/luxtorpeda/dosbox-tests/-/raw/master/archives/profiles.tar.zst"
BINARY="tests/dosbox"

# Move to our repo root
cd "$(git rev-parse --show-toplevel)"

# Fetch and unpack the profiles
wget "${PROFILES}" -O - | zstd -d | tar -x

# Convert and merge the profiles
parallel ./afdo/bin/create_llvm_prof --binary="${BINARY}" --profile="{}" --out="{.}".profraw ::: tests/*/*.prof
llvm-profdata-11 merge -sample -output=current.profraw tests/*/*.profraw
