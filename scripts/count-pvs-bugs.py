#!/usr/bin/python3

# Copyright (C) 2020 Kevin Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Count the number of issues found in an PVS-Studio report.

Usage: count-pvs-issues.py REPORT [MAX-ISSUES]
Where:
 - REPORT is a file in CSV-format
 - MAX-ISSUES is as a positive integer indicating the maximum
   issues that should be permitted before returning failure
   to the shell. Default is non-limit.

"""

# pylint: disable=invalid-name
# pylint: disable=missing-docstring

import collections
import csv
import os
import sys

def parse_issues(filename):
    """
    Returns a dict of int keys and a list of string values, where the:
    - keys are V### PVS-Studio error codes
    - values are the message of the issue as found in a specific file

    """
    issues = collections.defaultdict(list)
    with open(filename) as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            full = row['ErrorCode'] # extract the full code as an URL string
            code = full[full.rfind('V'):full.rfind('"')] # get the trailing "V###" code
            if code.startswith('V'):
                # Convert the V### string into an integer for easy sorting
                issues[int(code[1:])].append(row['Message'])
    return issues


def main(argv):
    # assume success until proven otherwise
    rcode = 0

    # Get the issues and the total tally
    issues = parse_issues(argv[1])
    tally = sum(len(messages) for messages in issues.values())

    if tally > 0:
        # Step through the codes and summarize
        print("Issues are tallied and sorted by code:\n")
        print("   code | issue-string in common to all instances       | tally")
        print("  -----   ---------------------------------------------   -----")

    for code in sorted(issues.keys()):
        messages = issues[code]
        in_common = os.path.commonprefix(messages)[:45]
        if len(in_common.split(' ')) < 4:
            in_common = 'N/A (too little in-common between issues)'
        print(f'  [{code:4}]  {in_common:45} : {len(messages)}')

    # Print the tally against the desired maximum
    if len(sys.argv) == 3:
        max_issues = int(sys.argv[2])
        print(f'\nTotal: {tally} issues (out of {max_issues} allowed)')
        if tally > max_issues:
            rcode = 1
    else:
        print(f'\nTotal: {tally} issues')

    return rcode

if __name__ == "__main__":
    sys.exit(main(sys.argv))
