#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

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
