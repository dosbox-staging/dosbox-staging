#!/usr/bin/env python3

# SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
# SPDX-License-Identifier: MIT

"""Find PRs in a draft release notes file that are missing from the
current work-in-progress release notes. Both files must use the DOSBox
Staging MkDocs release notes format with '??? note "Full PR list ..."'
collapsible sections."""

# pylint: disable=invalid-name

import argparse
import re

SECTION_RE = re.compile(r'^\?\?\? note "Full PR list .+"')
ITEM_RE    = re.compile(r"^ {4}- .+\(#(\d+)\)\s*$")


def parse_pr_lists(path):
    """Return list of (header, [(line_text, pr_number), ...]) for each
    'Full PR list' section."""

    sections = []
    current_header = None
    current_items = []

    with open(path, encoding="UTF-8") as f:
        for line in f:
            if SECTION_RE.match(line):
                if current_header:
                    sections.append((current_header, current_items))

                current_header = line.rstrip("\n")
                current_items = []
                continue

            if current_header:
                m = ITEM_RE.match(line)
                if m:
                    current_items.append((line.rstrip("\n"), m.group(1)))
                elif line.strip() and not line.startswith("    "):
                    # Left the indented block
                    sections.append((current_header, current_items))

                    current_header = None
                    current_items = []

    if current_header:
        sections.append((current_header, current_items))

    return sections


# pylint: disable=missing-docstring
def main():
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument("current", help="work-in-progress release notes Markdown file")
    parser.add_argument("draft", help="draft release notes Markdown file")
    parser.add_argument("diff", help="output file containing the new PR items")

    args = parser.parse_args()

    # Collect all PR numbers from the current file
    current_sections = parse_pr_lists(args.current)
    current_prs = set()

    for _, items in current_sections:
        for _, pr in items:
            current_prs.add(pr)

    # Find new items in the draft
    draft_sections = parse_pr_lists(args.draft)
    output_sections = []

    for header, items in draft_sections:
        new_items = [(text, pr) for text, pr in items if pr not in current_prs]
        if new_items:
            output_sections.append((header, new_items))

    # Write diff file
    with open(args.diff, "w", encoding="UTF-8") as f:
        for i, (header, items) in enumerate(output_sections):
            if i > 0:
                f.write("\n")
            f.write(header + "\n\n")

            for text, _ in items:
                f.write(text + "\n")
            f.write("\n")

    total = sum(len(items) for _, items in output_sections)

    print(f"{total} new PRs across {len(output_sections)} sections")


if __name__ == "__main__":
    main()
