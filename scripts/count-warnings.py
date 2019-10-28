#!/usr/bin/python3

# Copyright (c) 2019 Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script counts all compiler warnings and prints a summary.
#
# Usage: ./count-warnings.py build.log
# Usage: cat "*.log" | ./count-warnings.py -
#
# note: new compilers include additional flag -fdiagnostics-format=[text|json],
# which could be used instead of parsing using regex, but we want to preserve
# human-readable output in standard log.

# pylint: disable=invalid-name
# pylint: disable=missing-docstring

import os
import re
import sys

# Maximum allowed number of issues; if build will include more warnings,
# then script will return with status 1. Simply change this line if you
# want to set a different limit.
#
MAX_ISSUES = 3000

# For recognizing warnings in GCC format in stderr:
#
WARNING_PATTERN = re.compile(r'([^:]+):(\d+):\d+: warning: .* \[-W(.+)\]')
#                               ~~~~~   ~~~  ~~~           ~~      ~~
#                               ↑       ↑    ↑             ↑       ↑
#                               file    line column        message type

# For removing color when GCC is invoked with -fdiagnostics-color=always
#
ANSI_COLOR_PATTERN = re.compile(r'\x1b\[[0-9;]*[mGKH]')


def remove_colors(line):
    return re.sub(ANSI_COLOR_PATTERN, '', line)


def count_warning(line, warning_types):
    line = remove_colors(line)
    match = WARNING_PATTERN.match(line)
    if not match:
        return 0
    # file = match.group(1)
    # line = match.group(2)
    wtype = match.group(3)
    count = warning_types.get(wtype) or 0
    warning_types[wtype] = count + 1
    return 1


def get_input_lines(name):
    if name == '-':
        return sys.stdin.readlines()
    if not os.path.isfile(name):
        print('{}: no such file.'.format(name))
        sys.exit(2)
    with open(name, 'r') as logs:
        return logs.readlines()


def find_longest_name_length(names):
    return max(len(x) for x in names)


def print_summary(issues):
    summary = list(issues.items())
    size = find_longest_name_length(issues.keys()) + 1
    for warning, count in sorted(summary, key=lambda x: -x[1]):
        print('  {text:{field_size}s}: {count}'.format(
            text=warning, count=count, field_size=size))
    print()


def main():
    total = 0
    warning_types = {}
    for line in get_input_lines(sys.argv[1]):
        total += count_warning(line, warning_types)
    if warning_types:
        print("Warnings grouped by type:\n")
        print_summary(warning_types)
    print('Total: {} warnings (out of {} allowed)\n'.format(total, MAX_ISSUES))
    if total > MAX_ISSUES:
        print('Error: upper limit of allowed warnings is', MAX_ISSUES)
        sys.exit(1)


if __name__ == '__main__':
    main()
