#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Kevin R. Croft <krcroft@gmail.com>

# A helper script that fetches, converts, and merges kernel sample
# (.prof) files (collected during prior DOSBox testing) into a single
# GCC-compatible AutoFDO record that can be used to optimize builds.

# Dependencies:
#   - zstd
#   - autofdo

set -euo pipefail

# Tarball containing profile records
PROFILES="https://gitlab.com/luxtorpeda/dosbox-tests/-/raw/master/archives/profiles.tar.zst"
BINARY="tests/dosbox"

# Move to our repo root
cd "$(git rev-parse --show-toplevel)"

# Fetch and unpack the profiles
wget "${PROFILES}" -O - | zstd -d | tar -x

# Convert and merge the profiles
find . -name '*.prof' -print0 \
   | xargs -0 -P "$(nproc)" -I {} \
     create_gcov --binary="${BINARY}" --profile="{}" -gcov="{}".afdo -gcov_version=1
profile_merger -gcov_version=1 -output_file=current.afdo tests/*/*.afdo
