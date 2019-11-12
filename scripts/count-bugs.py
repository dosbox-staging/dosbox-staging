#!/usr/bin/python3

# Copyright (c) 2019 Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# This script prints a summary snippet of information out of reports created
# by scan-build or analyze-build for Clang's static code analysis.
#
# This script counts the number of reported issues and compares that
# against provided maximum. if the count exceeds the maximum then the
# script will return a status of 1 (failure), otherwise the script
# returns 0 (success).
#
# If you prefer to not set a maximum, then supply a negative maximum
# such as -1.
#
# Usage: ./count-warnings.py max-issues path/to/report/index.html
#
# This script depends on BeautifulSoup module, if you're distribution is
# missing it, you can use pipenv to install it for virtualenv spanning only
# this repo: pipenv install beautifulsoup4 html5lib

# pylint: disable=invalid-name
# pylint: disable=missing-docstring

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
        return {bug: count for bug, count in summary_values(summary)}


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
    rcode = 0
    max_issues = int(sys.argv[1])
    bug_types = read_soup(sys.argv[2])
    total = bug_types.pop('All Bugs')
    if bug_types:
        print("Bugs grouped by type:\n")
        print_summary(bug_types)
    print('Total: {} bugs'.format(total), end='')

    if max_issues >= 0:
        print(' (out of {} allowed)\n'.format(max_issues))
        if total > max_issues:
            print('Error: upper limit of allowed bugs is', max_issues)
            rcode = 1
    else:
        print('\n')

    return rcode

if __name__ == '__main__':
    sys.exit(main())
