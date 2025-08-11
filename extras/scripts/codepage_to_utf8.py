#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
#
# Copyright (C) 2023 rderooy <rderooy@users.noreply.github.com>

"""
Convert file from legacy codepage to Unicode UTF-8

For example to convert a legacy GOG dosbox config file such that menus are correctly drawn.
"""
import argparse
import os
import sys

parser = argparse.ArgumentParser(
    description="Convert file from legacy codepage to Unicode UTF-8",
    formatter_class=argparse.RawTextHelpFormatter,
)
parser.add_argument("filename", type=str, help="filename to convert")
parser.add_argument(
    "--cp", type=int, default=437, help="codepage to convert from (default: 437)"
)
parser.add_argument(
    "-o", "--output", type=str, help="output filename (default: overwrite input file)"
)
args = parser.parse_args()

if not args.output:
    args.output = args.filename

try:
    with open(
        os.path.expanduser(args.filename), "r", encoding="cp" + str(args.cp)
    ) as input_file:
        lines = input_file.read()
except FileNotFoundError:
    print("Error: could not locate the input file")
    sys.exit(1)
except LookupError:
    print(f"Error: Unknown encoding '{args.cp}'")
    sys.exit(1)
except UnicodeDecodeError:
    print("Error: input file does not appear to be a text file")
    sys.exit(1)
except PermissionError:
    print("Error: permission denied trying to read the file")
    sys.exit(1)

try:
    with open(os.path.expanduser(args.output), "w", encoding="utf-8") as output_file:
        output_file.write(lines)
except PermissionError:
    print("Error: permission denied trying to write to the file")
    sys.exit(1)

print(f"File '{args.filename}' converted to UTF-8")
