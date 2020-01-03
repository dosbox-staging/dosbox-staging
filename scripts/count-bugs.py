#!/usr/bin/python3

# Copyright (C) 2019-2020  Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script prints a summary snippet of information out of reports
created by scan-build or analyze-build for Clang's static code
analysis.

It returns success to the shell if the number or bugs encountered
is less than or equal to the desired maximum bugs. See --help
for a description of how to set the maximum.

If the count exceeds the maximum then the script will return a
status of 1 (failure), otherwise the script returns 0 (success).

"""
# This script depends on BeautifulSoup module, if you're distribution is
# missing it, you can use pipenv to install it for virtualenv spanning only
# this repo: pipenv install beautifulsoup4 html5lib

# pylint: disable=invalid-name
# pylint: disable=missing-docstring

import os
import argparse
import sys

from bs4 import BeautifulSoup

def summary_values(summary_table):
    if not summary_table:
        return
    for row in summary_table.find_all('tr'):
        description = row.find('td', 'SUMM_DESC')
        value = row.find('td', 'Q')
        if description is None or value is None:
            continue
        yield description.get_text(), int(value.get_text())


def read_soup(index_html):
    with open(index_html) as index:
        soup = BeautifulSoup(index, 'html5lib')
        tables = soup.find_all('table')
        summary = tables[1]
        return dict(summary_values(summary))


def find_longest_name_length(names):
    return max(len(x) for x in names)


def print_summary(issues):
    summary = list(issues.items())
    size = find_longest_name_length(issues.keys()) + 1
    for warning, count in sorted(summary, key=lambda x: -x[1]):
        print('  {text:{field_size}s}: {count}'.format(
            text=warning, count=count, field_size=size))
    print()

def to_int(value):
    return int(value) if value is not None else None

def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter, description=__doc__)

    parser.add_argument(
        'report',
        metavar='REPORT',
        help=("Path to the HTML report file"))

    max_bugs = to_int(os.getenv('MAX_BUGS', None))
    parser.add_argument(
        '-m', '--max-bugs',
        type=int,
        required=not isinstance(max_bugs, int),
        default=max_bugs,
        help='Defines the maximum number of bugs permitted to exist\n'
             'in the report before returning a failure to the shell.\n'
             'If not provided, the script will attempt to read it from\n'
             'the MAX_BUGS environment variable, which is currently\n'
             'set to: {}.  If a maximum of -1 is set then success is\n'
             'always returned to the shell.\n\n'.format(str(max_bugs)))

    return parser.parse_args()

def main():
    rcode = 0
    args = parse_args()
    bug_types = read_soup(args.report)
    total = bug_types.pop('All Bugs')
    if bug_types:
        print("Bugs grouped by type:\n")
        print_summary(bug_types)

    print('Total: {} bugs'.format(total), end='')
    if args.max_bugs >= 0:
        print(' (out of {} allowed)\n'.format(args.max_bugs))
        if total > args.max_bugs:
            print('Error: upper limit of allowed bugs is', args.max_bugs)
            rcode = 1
    else:
        print('\n')

    return rcode

if __name__ == '__main__':
    sys.exit(main())
