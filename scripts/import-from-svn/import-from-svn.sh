#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2019-2021  Patryk Obara <patryk.obara@gmail.com>

# Import script suitable for SVN repositories using standard layout.
# Uses git-svn to download whole history, converts svn tag paths to git tags,
# filters repo and removes temporary git refs.

set -euo pipefail

readonly svn_url="https://svn.code.sf.net/p/dosbox/code-0/dosbox"

readonly git_destination_repo=$(git config --get remote.origin.url)

readonly git_default_branch=$(git branch -rl '*/HEAD' | rev | cut -d/ -f1 | rev)

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
	local -r svn_rev_range=$1
	local -r repo_name=$2
	local -r default_branch=$3

	if ! git -c "init.defaultBranch=$default_branch" \
		svn init --stdlayout "$svn_url" "$repo_name" ; then
		echo_err "'git svn init' failed. Do you have git-svn installed?"
		exit 1
	fi

	local -r authors_file=$PWD/svn-dosbox-authors
	#
	# TODO:
	# warning: do not use --use-log-author
	#
	# removing From: lines from message might (will?) mess up preserving info for
	# fast imports, as I am rebasing commits, and overwriting committer
	#
	# TODO: implement our own authorship filtering
	#
	git -C "$repo_name" svn fetch \
		--revision="$svn_rev_range" \
		--authors-file="$authors_file"
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

git_rewrite_committers () {
	local -r repo_name=$1
	local -r rev_list=$2
	# shellcheck disable=SC2016
	# --env-filter is going to evaluate the variables
	git -C "$repo_name" filter-branch \
		-f --env-filter '
			GIT_COMMITTER_NAME=$GIT_AUTHOR_NAME
			GIT_COMMITTER_EMAIL=$GIT_AUTHOR_EMAIL
		' \
		-- "$rev_list"
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
	local -r default_branch=$1

	git -C "$1" checkout svn/trunk
	git -C "$1" branch -D "$default_branch"
}

# Perform import of the whole SVN history
#
full_import () {
	local -r repo=$1
	local -r default_branch=$2

	if ! git_svn_clone_dosbox "1:HEAD" "$repo" "$default_branch" ; then
		echo_err "Preparing git-svn repo failed."
		exit 1
	fi

	git_rewrite_import_links "$repo"
	name_active_branches "$repo"
	import_svn_tagpaths_as_git_tags "$repo"
	cleanup "$repo" "$default_branch"
}

# TODO: Fast import
#
fast_import () {
	local -r repo=$1
	local -r git_repo=$2
	local -r default_branch=$3

	local -r svn_base_rev=4385
	# commit svn_base_rev before rebase:
	local -r git_old_base_commit=132fa665fc3b5eea11fdc2236080afa5bbadbd9d
	# commit svn_base_rev after rebase:
	local -r git_new_base_commit=b5c09dd8ebf69c1ccc5c3d0722c80d30a1780576

	if ! git_svn_clone_dosbox "$svn_base_rev:HEAD" "$repo" "$default_branch" ; then
		echo_err "TODO!!!" # TODO
		exit 1
	fi

	git_rewrite_import_links "$repo"
	name_active_branches "$repo"
	cleanup "$repo"

	git -C "$repo" remote add dosbox-staging "$git_repo"
	git -C "$repo" fetch dosbox-staging
	git -C "$repo" rebase \
		--committer-date-is-author-date \
		--onto="$git_new_base_commit" \
		"$git_old_base_commit"

	git_rewrite_committers "$repo" "$git_new_base_commit..HEAD"

	echo
	echo "Tip of svn/trunk in $repo:"
	echo

	git -C "$repo" log --oneline dosbox-staging/svn/trunk~1..svn/trunk

	echo
	echo "Inspect new commits in $repo in detail."
	echo "If the import looks good, invoke:"
	echo
	echo "    $ git -C $repo push dosbox-staging svn/trunk:svn/trunk"
	echo
}

## Confirm the remote repo to use with recommended push command
#
confirm_remote () {
        local -r git_repo=$1
        local -r default_branch=$2

	echo ""
	echo "Remote repo: $git_repo"
	echo ""
	echo "Default branch: $default_branch"
	echo ""
	read -r -p "Press Enter to continue ot Ctrl+C to abort"
}

# main
#
main () {
	confirm_remote "$git_destination_repo" "$git_default_branch"

	local -r repo=dosbox-git-svn-$(svn_get_repo_revision)
	if [ -e "$repo" ] ; then
		echo_err "Repository '$repo' exists already."
		exit 1
	fi

	full_import "$repo" "$git_default_branch"

	# fast_import "$repo" "$git_destination_repo" "$git_default_branch"
}

main "$@"
