#!/bin/bash

# Strips all ANSI sequences from stdin.
#
# Works with both GNU and non-GNU 'sed' (e.g., the default macOS 'sed'
# command).
#
# This removes anything that starts with '[' (left square bracket), has any
# number of decimals and semicolons, and ends with a letter. It should catch
# any of the common ANSI escape sequences. including 256-color colour codes.
#
# ---
# From: https://stackoverflow.com/a/51141872

sed 's/\x1B\[[0-9;]\{1,\}[A-Za-z]//g'
