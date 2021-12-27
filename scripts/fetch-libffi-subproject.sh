#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2021-2021  kcgen <kcgen@users.noreply.github.com>

# This script exists only to easily fetch the libffi subproject
# because the repository's HTTPS interface used by the glib wrap
# is frequently down.
#
# Ref: https://gitlab.freedesktop.org/gstreamer/meson-ports/libffi/-/issues/8
#
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"

subproject_branch="meson"
subproject_dir="$repo_root/subprojects/libffi"
subproject_repo="git@gitlab.freedesktop.org:gstreamer/meson-ports/libffi.git"
fallback_tarball="https://github.com/dosbox-staging/dosbox-staging/files/7780837/libffi.tar.xz.zip"

if [[ ! -d "$subproject_dir" ]]; then
	GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=no -o LogLevel=ERROR" \
	git clone --depth 1 --branch "$subproject_branch" "$subproject_repo" "$subproject_dir" || \
	curl -L "$fallback_tarball" | xz -dc | tar -x -C "$repo_root/subprojects/"
else
	echo "subproject_dir already exists, skipping clone"
fi
