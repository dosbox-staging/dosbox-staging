#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Kevin R. Croft <krcroft@gmail.com>

# This script reduces the size of a MacPorts installation
# under macOS. Its main use is to shrink this area to fit within
# GitHub's cache limits; however it can also be used by end-users
# wanting to save space.
#
# Usage: ./shrink-macports.sh
#
set -xuo pipefail
set +e

# If MacPorts doesn't exist then our job here is done!
cd /opt/local || exit 0

# Ensure we have sudo rights
if [[ $(id -u) -ne 0 ]] ; then echo "Please run as root" ; exit 1 ; fi

# Cleanup permissions and attributes
chflags nouchg .
find . -type d -exec xattr -c {} +
find . -type f -exec xattr -c {} +

for dir in var/macports libexec/macports share/man; do
	rm -rf "${dir}" &> /dev/null || true
done

# Binary-stripping is extremely verbose, so send these to the ether
find . -name '*' -a ! -iname 'strip' -type f -perm +111 -exec ./bin/strip {} + &> /dev/null
find . -name '*.a' -type f -exec ./bin/strip {} + &> /dev/null

# This entire script is best-effort, so always return success
exit 0
