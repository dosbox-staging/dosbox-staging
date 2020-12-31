#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Kevin R. Croft <krcroft@gmail.com>

# Builds and installs the AutoFDO package from
# https://github.com/google/autofdo
#
# AutoFDO is used to convert sample profiling data (collected using perf
# or ocperf.py from a Linux kernel built with with last branch
# record (LBR) tracing support running on an Intel CPU with LBR
# support) to GCC-specific AutoFDO records or LLVM's raw profile
# records.
#
# Pre-requisites: autoconf automake git libelf-dev libssl-dev pkg-config
# If building for LLVM, clang and llvm-dev are needed.
#
# Usage: build-autofdo.sh [LLVM version]
# Examples: ./build-autofdo.sh
#           ./build-autofdo.sh 10
#
# Where the optional [LLVM version] allows building with LLVM support
# for the provided LLVM version.

set -euo pipefail

rootdir="$(pwd)"
prefix="$rootdir/afdo"

# Clone the repo
if [[ ! -d autofdo ]]; then
	git clone --depth 1 --recursive https://github.com/google/autofdo.git
fi

# Enter and sync the repo (if it already exists)
pushd autofdo
git pull

# Initialize auto-tools
aclocal -I .
autoheader
autoconf
automake --add-missing -c

# Configure with the specified LLVM version if provided
if [[ -n "${1:-}" ]]; then
	ver="$1"
	withllvm="--with-llvm=$(command -v llvm-config-"$ver")"
fi

# Configure
flags="-Os -DNDEBUG -pipe"
./configure CFLAGS="$flags" CXXFLAGS="$flags" --prefix="$prefix" "${withllvm:-}"

# Build and install
# Note: make cannot be run in parallel because the sub-projects'
#       need to be configured serially with respect to each other
make
make install
popd

# Strip the binaries
cd "$prefix/bin"
strip ./*
