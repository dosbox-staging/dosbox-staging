#!/bin/bash

# Copyright (C) 2019-2020  Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

readonly svn_url=https://svn.code.sf.net/p/dosbox/code-0/dosbox

list_directories () {
	find . -type d | grep -v "./.git"
}

svn_ignore_content () {
	echo "# svn:ignore"
	svn propget svn:ignore "$svn_url/trunk/$1"
}

update_svn_ignore_files () {
	while read -r dir ; do
		echo "Reading svn:ignore for: $dir"
		svn_ignore_content "$dir" > "$dir/svn-ignore"

		if [ ! -f "$dir/.gitignore" ] ; then
			echo "New file (add)"
			mv "$dir/svn-ignore" "$dir/.gitignore"
			git add "$dir/.gitignore"
			continue
		fi

		if diff "$dir/.gitignore" "$dir/svn-ignore" > /dev/null ; then
			echo "No change (skip)"
			rm "$dir/svn-ignore"
		else
			echo "File differs (merge $dir/.gitignore and $dir/svn-ignore manually)"
		fi
	done
}

cd "$(git rev-parse --show-toplevel)" || exit

list_directories | update_svn_ignore_files
