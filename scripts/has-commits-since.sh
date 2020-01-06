#!/bin/bash

# Copyright (C) 2020  Kevin Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This trivial script indicates if the current git repository
# had had any commit within the given timeframe, which is provided
# in git's --since notation.

# Usage:   has-commits-since.sh TIMEFRAME
# Example: has-commits-since.sh "24 hours ago"

# Output is a single word: true or false, indicating if commits
# were found over the provided timeframe. The exit code is always
# zero unless the script or it's commands fail.

set -euo pipefail

if [[ "$#" != 1 ]]; then
	echo "Usage:   $0 TIMEFRAME"
	echo "Example: $0 \"24 hours ago\""
	exit 1
fi

timeframe="${1}"
commits="$(git --no-pager log --pretty=oneline --since="${timeframe}" | wc -l)"

if [[ "${commits}" -gt "0" ]]; then
	echo "true"
else
	echo "false"
fi
