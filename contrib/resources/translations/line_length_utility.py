#!/usr/bin/env python3
import sys
import re
from typing import Tuple

def print_usage():
    print("Usage: line_length_utility.py inputfile [outputfile] [max_line_length]")
    print("If the optional parameter [outputfile] is not set, the script generates a file under the same input name with _new added.")
    print("Optional parameter [max_line_length] requires [outputfile] to be set. If not the script defaults to 80.")

def process_line(line: str, max_line_length: int, input_line_num: int, output_line_num: int, skip_line: bool = False, skip_reason: str = "") -> Tuple[str, int]:
    if skip_line:
        sys.stderr.write(f"Skipped input line {input_line_num} due to {skip_reason}!\n")
        return line, output_line_num

    line_without_brackets = re.sub(r'\[[^\]]*\]', '', line)
    line_length = len(line_without_brackets)

    if line_length > max_line_length:
        sys.stderr.write(f"Character limit exceeded at input line {input_line_num}: {line}\n")
        wrapped_lines = re.sub(r'((?:\[(?:[^\]])*\]|[^[\r\n])(?:(?:[ \t]*?=[ \t]*?([a-z\-]+))?){' + str(max_line_length) + '})', r'\1\n', line)
        num_wrapped_lines = wrapped_lines.count('\n')
        sys.stderr.write(f"Created a line break at output lines: {output_line_num}-{output_line_num + num_wrapped_lines - 1}\n")
        output_line_num += num_wrapped_lines
        return wrapped_lines, output_line_num
    else:
        output_line_num += 1
        return line, output_line_num

if len(sys.argv) < 2:
    print_usage()
    sys.exit(1)

inputfile = sys.argv[1]
if len(sys.argv) > 2:
    if re.search(r'\.[^.]+$', sys.argv[2]):
        outputfile = sys.argv[2]
        if len(sys.argv) > 3:
            try:
                max_line_length = int(sys.argv[3])
            except ValueError:
                sys.stderr.write(f"Invalid max_line_length value: {sys.argv[3]}\n")
                sys.exit(1)
        else:
            max_line_length = 80
    else:
        outputfile = re.sub(r'\.([^\.]+)$', r'_new.\1', inputfile)
        try:
            max_line_length = int(sys.argv[2])
        except ValueError:
            sys.stderr.write(f"Invalid max_line_length value: {sys.argv[2]}\n")
            sys.exit(1)
else:
    outputfile = re.sub(r'\.([^\.]+)$', r'_new.\1', inputfile)
    max_line_length = 80

try:
    max_line_length = int(sys.argv[3]) if len(sys.argv) > 3 else 80
except ValueError:
    sys.stderr.write(f"Invalid max_line_length value: {sys.argv[3]}\n")
    sys.exit(1)

try:
    max_line_length = int(sys.argv[3]) if len(sys.argv) > 3 else 80
except ValueError:
    sys.stderr.write(f"Invalid max_line_length value: {sys.argv[3]}\n")
    sys.exit(1)

try:
    with open(inputfile, 'r', encoding='utf-8') as infile, open(outputfile, 'w', encoding='utf-8') as outfile:
        input_line_num = 1
        output_line_num = 1
        skip_line = False
        skip_next_line = False 
        skip_reason = ""
        for line in infile:
            if skip_next_line: 
                skip_line = True
                skip_reason = "line following :TITLEBAR_"
                skip_next_line = False
            elif ':TITLEBAR_' in line:
                skip_line = True
                skip_reason = ":TITLEBAR_"
                skip_next_line = True
            elif any(symbol in line for symbol in ["╚", "═", "╝", "║"]):
                skip_line = True
                skip_reason = "special symbol"
            else:
                skip_line = False
                skip_reason = ""
            processed_line, output_line_num = process_line(line, max_line_length, input_line_num, output_line_num, skip_line, skip_reason)
            outfile.write(processed_line)
            input_line_num += 1
    print(f"Finished writing output file at {outputfile}")
except FileNotFoundError:
    sys.stderr.write(f"File {inputfile} not found.\n")
    sys.exit(1)