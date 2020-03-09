#!/usr/bin/python3

# Copyright (C) 2020  Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# pylint: disable=invalid-name
# pylint: disable=missing-docstring

"""
Count known GCC/Clang sanitizer runtime issues and print a summary.

It returns success to the shell if the number or issues encountered
is less than or equal to the desired maximum issues (default: 0).

You can override the default limit with MAX_ISSUES environment variable or
using --max-issues option (see the description of argument in --help for
details).
"""

import argparse
import os
import re
import sys

SAN_WARN_PATTERN = re.compile(r'==\d+==WARNING: ([^:]+): (.*)')
#                                                ~~~~~    ~~
#                                                ↑        ↑
#                                        sanitizer        message

SAN_ERR_PATTERN = re.compile(r'==\d+==ERROR: ([^:]+): (.*)')
#                                             ~~~~~    ~~
#                                             ↑        ↑
#                                     sanitizer        message

RT_ERR_PATTERN = re.compile(r'([^:]+):(\d+):\d+: runtime error: .*')
#                              ~~~~~   ~~~                      ~~
#                              ↑       ↑                        ↑
#                              file    line                     message

MEMLEAK_PATTERN_1 = re.compile(r'Direct leak of \d+ byte\(s\) in .*')

MEMLEAK_PATTERN_2 = re.compile(r'Indirect leak of \d+ byte\(s\) in .*')


def match_line(line):
    match = SAN_WARN_PATTERN.match(line)
    if match:
        return match.group(2)

    match = SAN_ERR_PATTERN.match(line)
    if match:
        return match.group(2)

    match = RT_ERR_PATTERN.match(line)
    if match:
        return 'runtime error'

    match = MEMLEAK_PATTERN_1.match(line)
    if match:
        return 'direct leak'

    match = MEMLEAK_PATTERN_2.match(line)
    if match:
        return 'indirect leak'

    return None


def count_issue(line, issue_types):
    issue = match_line(line)
    if issue:
        count = issue_types.get(issue) or 0
        issue_types[issue] = count + 1
        return 1
    return 0


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

    max_issues = int(os.getenv('MAX_ISSUES', '0'))
    parser.add_argument(
        '-m', '--max-issues',
        type=int,
        default=max_issues,
        help='Override the maximum number of issues.\n'
             'Use value -1 to disable the check.')

    return parser.parse_args()


def main():
    rcode = 0
    total = 0
    issue_types = {}
    args = parse_args()
    for line in get_input_lines(args.logfile):
        total += count_issue(line, issue_types)
    if issue_types:
        print("Issues grouped by type:\n")
        print_summary(issue_types)
    print('Total: {} issues'.format(total), end='')
    if args.max_issues >= 0:
        print(' (out of {} allowed)\n'.format(args.max_issues))
        if total > args.max_issues:
            print('Error: upper limit of allowed issues is', args.max_issues)
            rcode = 1
    else:
        print('\n')
    return rcode

if __name__ == '__main__':
    sys.exit(main())
