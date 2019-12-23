#!/bin/bash

# This script uses unifdef to mass-remove code inside ifdefs, that are not
# relevant to Linux.
#
# Use it only to improve readability when researching code.

cd "$(git rev-parse --show-toplevel)" || exit

git ls-files ./*.{h,cpp} | xargs unifdef \
	-U WIN32 \
	-U _WIN32 \
	-U _WIN64 \
	-U MACOSX \
	-m
