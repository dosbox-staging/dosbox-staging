#!/bin/bash
#
# A git hook script to find and fix trailing whitespace
# in your commits. Bypass it with the --no-verify option
# to git-commit
#
# Copyright Oldman <imoldman.com@gmail.com> and contributors:
#   https://github.com/imoldman/config
#
# Add this to a project's pre-hooks using:
#  git rev-parse --show-toplevel
#  ln -s ../../scripts/git-prehook-whitespace.sh .git/hooks/pre-commit

set -euo pipefail

# detect platform
platform="win"
uname_result="$(uname)"
if [ "$uname_result" = "Linux" ]; then
	platform="linux"
elif [ "$uname_result" = "Darwin" ]; then
	platform="mac"
fi

# change IFS to ignore filename's space in |for|
IFS="
"
# autoremove trailing whitespace
for line in $(git diff --check --cached | sed '/^[+-]/d') ; do
	# get file name
	if [ "$platform" = "mac" ]; then
		file="$(echo "$line" | sed -E 's/:[0-9]+: .*//')"
	else
		file="$(echo "$line" | sed -r 's/:[0-9]+: .*//')"
	fi
	# display tips
	echo -e "auto remove trailing whitespace in \\033[31m$file\\033[0m!"
	# since $file in working directory isn't always equal to $file in index, so we backup it
	mv -f "$file" "${file}.save"
	# discard changes in working directory
	git checkout -- "$file"
	# remove trailing whitespace
	if [ "$platform" = "win" ]; then
		# sed in msys 1.x does not support -i
		sed 's/[[:space:]]*$//' "$file" > "${file}.bak"
		mv -f "${file}.bak" "$file"
	elif [ "$platform" == "mac" ]; then
		sed -i "" 's/[[:space:]]*$//' "$file"
	else
		sed -i 's/[[:space:]]*$//' "$file"
	fi
	git add "$file"
	# restore the $file
	sed 's/[[:space:]]*$//' "${file}.save" > "$file"
	rm "${file}.save"
done

# Now we can commit
exit
