#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Patryk Obara <patryk.obara@gmail.com>

# This script exists only to easily run shellcheck on all files in the repo.
# You can pass additional parameters to this script itself, e.g.:
#
# $ ./verify-bash.sh --format=json

list_bash_files () {
	git ls-files \
		| xargs file \
		| grep "Bourne-Again shell script" \
		| cut -d ':' -f 1
}

main () {
	shellcheck --version >&2
	echo "Checking files:" >&2
	list_bash_files >&2
	list_bash_files | xargs -L 1000 shellcheck --color "$@"
}

Anotate_github () {
	jq -r -j \
		'.comments[]
		 |"::error"
		 ," file=",.file
		 ,",line=",.line
		 ,"::SC",.code,": ",.message
		 ,"\n"
		' | grep "" || return 0 && return 123
# return 123 same as xargs return code would be
}

if [[ -z $GITHUB_ACTOR ]]
then
	main "$@"
else
	Anotate_github < <( main --format=json1 "$@" )
fi
