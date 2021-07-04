#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Patryk Obara <patryk.obara@gmail.com>

# Import script suitable for SVN repositories using standard layout.
# Uses git-svn to download whole history, converts svn tag paths to git tags,
# filters repo and removes temporary git refs.

readonly svn_url=https://svn.code.sf.net/p/dosbox/code-0/dosbox

echo_err () {
	echo "$@" 1>&2
}

# SVN repository revision. By comparing repo revision we can detect if there
# was any activity on any svn path.
#
svn_get_repo_revision () {
	svn info "$svn_url" | grep "Revision:" | cut -d' ' -f2
}

# Example usage:
#
# svn_get_copied_from branches/0_74_3
#
svn_get_copied_from () {
	local -r repo_path=$1
	svn log -v --stop-on-copy --xml "$svn_url/$repo_path" \
		| xsltproc copied-from.xslt -
}

# Full import takes ~40 minutes
#
git_svn_clone_dosbox () {
	local -r repo_name=$1

	git svn init \
		--stdlayout \
		"$svn_url" \
		"$repo_name"

	local -r authors_prog=$PWD/svn-authors-prog.sh
	git -C "$repo_name" svn fetch \
		--use-log-author \
		--authors-prog="$authors_prog"
}

# Remove UUID of SVN server to avoid issues in case SourceForge will change
# it's infrastructure.
#
git_rewrite_import_links () {
	local -r repo_name=$1
	git -C "$repo_name" filter-branch \
		--msg-filter 'sed "s|git-svn-id: \([^ ]*\) .*|Imported-from: \1|"' \
		-- --all
}

list_svn_branch_paths () {
	echo trunk
	for branch in $(svn ls "$svn_url/branches") ; do
		echo "${branch///}"
	done
}

list_svn_tag_paths () {
	for branch in $(svn ls "$svn_url/tags") ; do
		echo "${branch///}"
	done
}

# Go through SVN "branches", that were not removed from SVN yet and create
# Git branches out of them  using name prefix "svn/" in local repository.
#
name_active_branches () {
	local -r repo_name=$1
	for path in $(list_svn_branch_paths) ; do
		git -C "$repo_name" branch "svn/$path" "origin/$path"
	done
}

find_tagged_git_commit () {
	git -C "$1" log \
		--grep="$svn_copied_path" \
		--branches=svn/* \
		--format=%H
}

# Import SVN "tags" as proper Git tags
#
# Hopefully upstream never committed to tags…
#
import_svn_tagpaths_as_git_tags () {
	local -r repo_name=$1
	local svn_copied_path
	local svn_tagged_commit
	for path in $(list_svn_tag_paths) ; do
		svn_copied_path=$(svn_get_copied_from "tags/$path")
		svn_tagged_commit=$(find_tagged_git_commit "$repo_name")
		git_tag_name="svn/$path"
		echo "$path: $svn_copied_path -> $svn_tagged_commit -> $git_tag_name"
		git -C "$repo_name" tag \
			"$git_tag_name" \
			"$svn_tagged_commit"
	done
}

# Remove any unnecessary refs left behind in imported repo
#
cleanup () {
	git -C "$1" checkout svn/trunk
	git -C "$1" branch -D main
}

# Set up remote repo to prepare for push
#
setup_remote () {
	git -C "$1" remote add dosbox-staging git@github.com:dosbox-staging/dosbox-staging.git
	git -C "$1" fetch dosbox-staging
}

# main
#
main () {
	readonly repo=dosbox-git-svn-$(svn_get_repo_revision)
	if [ -e "$repo" ] ; then
		echo_err "Repository '$repo' exists already."
		exit 1
	fi
	git_svn_clone_dosbox "$repo"
	git_rewrite_import_links "$repo"
	name_active_branches "$repo"
	import_svn_tagpaths_as_git_tags "$repo"
	cleanup "$repo"
	setup_remote "$repo"
}

main "$@"
