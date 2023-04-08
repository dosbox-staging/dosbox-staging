#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 The DOSBox Staging Team

print_usage() {
    echo "Usage: ./line_length_utility.sh inputfile outputfile [LINE LENGTH]"
    echo "If the optional parameter [LINE LENGTH] is not set, the script defaults to 80."
}

process_line() {
    local line="$1"
    local max_length="$2"
    local input_line_num="$3"
    local output_line_num="$4"

    line_without_brackets=$(echo "$line" | perl -pe 's/\[[^\]]*\]//g')
    line_length=${#line_without_brackets}

    if [ $line_length -gt $max_length ]; then
        echo "Character limit exceeded at input line $input_line_num: $line" >&2
        wrapped_lines=$(perl -CSD -pe 's/^((?:\[(?:[^\]])*\]|[^[\r\n]){'"$max_length"'})/$1\n/gm;' <<<"$line")
        echo "$wrapped_lines"
        num_wrapped_lines=$(echo "$wrapped_lines" | wc -l)
        echo "Created a line break at output lines: $output_line_num-$((output_line_num + num_wrapped_lines - 1))" >&2
        output_line_num=$((output_line_num + num_wrapped_lines))
    else
        echo "$line"
        output_line_num=$((output_line_num + 1))
    fi

    return_value=$output_line_num
}

inputfile="$1"
outputfile="$2"
max_line_length="${3:-80}"

if [ -z "$inputfile" ] || [ -z "$outputfile" ]; then
    print_usage
    exit 1
fi

if [ ! -f "$inputfile" ]; then
    echo "File $inputfile not found." >&2
    exit 1
fi

if ! [[ "$max_line_length" =~ ^[0-9]+$ ]]; then
    echo "Invalid max_line_length value: $max_line_length" >&2
    exit 1
fi

(
input_line_num=1
output_line_num=1
IFS=$'\n'
while read -r line; do
    process_line "$line" "$max_line_length" "$input_line_num" "$output_line_num"
    output_line_num=$return_value
    input_line_num=$((input_line_num + 1))
done < "$inputfile"
) > "$outputfile"

echo "Finished writing output file at $outputfile"
