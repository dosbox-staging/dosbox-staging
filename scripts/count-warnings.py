#!/usr/bin/python3

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Patryk Obara <patryk.obara@gmail.com>

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
GCC_WARN_PATTERN = re.compile(r'([^:]+):(\d+):\d+: warning: .* \[-W(.+?)\](.*)')
#                                ~~~~~   ~~~  ~~~           ~~      ~~~    ~~
#                                ↑       ↑    ↑             ↑       ↑      ↑
#                                file    line column  message    type  extra

# For recognizing warnings in MSVC format:
#
MSVC_WARN_PATTERN = re.compile(r'.+>([^\(]+)\((\d+),\d+\): warning ([^:]+): .*')
#                                ~~  ~~~~~~    ~~~  ~~~             ~~~~~   ~~
#                                ↑   ↑         ↑    ↑               ↑        ↑
#                          project   file      line column       code  message

# For removing color when GCC is invoked with -fdiagnostics-color=always
#
ANSI_COLOR_PATTERN = re.compile(r'\x1b\[[0-9;]*[mGKH]')


class warning_summaries:

    def __init__(self):
        self.types = {}
        self.files = {}
        self.lines = set()

    def count_type(self, name):
        self.types[name] = self.types.get(name, 0) + 1

    def count_file(self, name):
        self.files[name] = self.files.get(name, 0) + 1

    def list_all(self):
        for line in sorted(self.lines):
            print(line)
        print()

    def print_files(self):
        print("Warnings grouped by file:\n")
        print_summary(self.files)

    def print_types(self):
        print("Warnings grouped by type:\n")
        print_summary(self.types)


def remove_colors(line):
    return re.sub(ANSI_COLOR_PATTERN, '', line)


def count_warning(gcc_format, line_no, line, warnings):
    line = remove_colors(line)

    pattern = GCC_WARN_PATTERN if gcc_format else MSVC_WARN_PATTERN
    match = pattern.match(line)
    if not match:
        return 0

    # Some warnings (e.g. effc++) are reported multiple times, once
    # for every usage; ignore duplicates.
    line = line.strip()
    if line in warnings.lines:
        return 0
    warnings.lines.add(line)

    file = match.group(1)
    # wline = match.group(2)
    wtype = match.group(3)

    if pattern == GCC_WARN_PATTERN and match.group(4):
        print('Log file is corrupted: extra characters in line',
              line_no, file=sys.stderr)

    _, fname = os.path.split(file)
    warnings.count_type(wtype)
    warnings.count_file(fname)
    return 1


def get_input_lines(name):
    if name == '-':
        return sys.stdin.readlines()
    if not os.path.isfile(name):
        print('{}: no such file.'.format(name))
        sys.exit(2)
    with open(name, 'r', encoding='utf-8') as logs:
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

    parser.add_argument(
        '--msvc',
        action='store_true',
        help='Look for warnings using MSVC format.')

    return parser.parse_args()


def main():
    rcode = 0
    total = 0
    warnings = warning_summaries()
    args = parse_args()
    use_gcc_format = not args.msvc
    line_no = 1
    for line in get_input_lines(args.logfile):
        total += count_warning(use_gcc_format, line_no, line, warnings)
        line_no += 1
    if args.list:
        warnings.list_all()
    if args.files and warnings.files:
        warnings.print_files()
    if warnings.types:
        warnings.print_types()
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
