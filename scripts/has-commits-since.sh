#!/bin/bash

# Copyright (C) 2020  Kevin Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script indicates if the git repository has had
# any commits since a given timeframe.

# Usage: ./has-commits-since.sh TIMEFRAME
# Where TIMEFRAME is provided in git's --since notation
# Example: ./has-commits-since.sh "24 hours ago"

# The script prints either "true" or "false" indicating
# if commits were (or weren't) found for the timeframe.
# The exit code is zero unless the script itself fails.

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
