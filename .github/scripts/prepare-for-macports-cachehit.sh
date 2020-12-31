#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Kevin R. Croft <krcroft@gmail.com>

# This script creates a new writable /opt/local directory with
# sane ownership, permissions, and attributes on the directory.
# Usage: ./prepare-for-macports-cachehit.sh
#
set -xeuo pipefail

# Ensure we have sudo rights to create the directory
if [[ $(id -u) -ne 0 ]] ; then echo "Please run as root" ; exit 1 ; fi
user="${SUDO_USER}"
group="$(id -g "${user}")"

mkdir -p /opt/local
cd /opt/local
chflags nouchg .
xattr -rc .
chmod 770 .
chown "${user}:${group}" .
