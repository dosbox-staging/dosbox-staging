#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Launches any and all setup*.sh scripts found in the
# contrib/test-cases/*/ sub-directories.
#
# Setup scripts:
#   - are launched from within their containing directory
#   - are independent of one another (ie: can be run in parallel)
#   - fetch and unzip any binary content, such as games or media files
#   - create any directories or files used by one or more test-cases
#   - should quit early if they've already successfully been run
#   - should be manually-runnable from within their test-case directory
#     to facilitate testing and setting up the content manually
#
# For example:
#   contrib/common/setup-Doom.sh, contains the following (pseudo-code):
#    1. check if ./Doom/doom.cfg exists, if so then quit
#    2. wget doom.zip from public URL
#    3. unzip doom.zip into a new ./Doom diretory
#    4. delete doom.zip
#    5. exit with proper error-code
#
set -euo pipefail

# Move to our repo root
cd "$(git rev-parse --show-toplevel)/contrib/test-cases"

# Run up to eight setup jobs in parallel regardless of the number of
# cores because setup tasks are largely network-limited, and it's good
# etiquite to consume no more than 8 simultaneous stream to a given
# web-sever.
find . -name 'setup*.sh' \
	| parallel --jobs 8 --halt "soon,fail=10%" \
		'echo -n {.}' \
		'&& cd {//}' \
		'&& ./{/}' \
		'&& echo " [done]"' \
		'|| echo " [failed]"'

