#!/usr/bin/python3

# Copyright (C) 2019-2020  Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# pylint: disable=invalid-name
# pylint: disable=missing-docstring

"""
Count all compiler warnings and print a summary.

It returns success to the shell if the number or warnings encountered
is less than or equal to the desired maximum warnings (default: 0).

You can override the default limit with MAX_WARNINGS environment variable or
using --max-warnings option (see the description of argument in --help for
details).

note: new compilers include additional flag -fdiagnostics-format=[text|json],
which could be used instead of parsing using regex, but we want to preserve
human-readable output in standard log.
"""

import argparse
import os
import re
import sys

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


def count_warning(line, warning_types, warning_files, warning_lines):
    line = remove_colors(line)
    match = WARNING_PATTERN.match(line)
    if not match:
        return 0
    warning_lines.append(line.strip())
    file = match.group(1)
    # line = match.group(2)
    wtype = match.group(3)
    _, fname = os.path.split(file)
    type_count = warning_types.get(wtype) or 0
    file_count = warning_files.get(fname) or 0
    warning_types[wtype] = type_count + 1
    warning_files[fname] = file_count + 1
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
    size = find_longest_name_length(issues.keys()) + 1
    items = list(issues.items())
    for name, count in sorted(items, key=lambda x: (x[1], x[0]), reverse=True):
        print('  {text:{field_size}s}: {count}'.format(
            text=name, count=count, field_size=size))
    print()


def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter, description=__doc__)

    parser.add_argument(
        'logfile',
        metavar='LOGFILE',
        help=("Path to the logfile, or use - to read from stdin"))

    max_warnings = int(os.getenv('MAX_WARNINGS', '0'))
    parser.add_argument(
        '-m', '--max-warnings',
        type=int,
        default=max_warnings,
        help='Override the maximum number of warnings.\n'
             'Use value -1 to disable the check.')

    parser.add_argument(
        '-f', '--files',
        action='store_true',
        help='Group warnings by filename.')

    parser.add_argument(
        '-l', '--list',
        action='store_true',
        help='Display sorted list of all warnings.')

    return parser.parse_args()


def main():
    rcode = 0
    total = 0
    warning_types = {}
    warning_files = {}
    warning_lines = []
    args = parse_args()
    for line in get_input_lines(args.logfile):
        total += count_warning(line,
                               warning_types,
                               warning_files,
                               warning_lines)
    if args.list:
        for line in sorted(warning_lines):
            print(line)
        print()
    if args.files and warning_files:
        print("Warnings grouped by file:\n")
        print_summary(warning_files)
    if warning_types:
        print("Warnings grouped by type:\n")
        print_summary(warning_types)
    print('Total: {} warnings'.format(total), end='')
    if args.max_warnings >= 0:
        print(' (out of {} allowed)\n'.format(args.max_warnings))
        if total > args.max_warnings:
            print('Error: upper limit of allowed warnings is', args.max_warnings)
            rcode = 1
    else:
        print('\n')
    return rcode

if __name__ == '__main__':
    sys.exit(main())
