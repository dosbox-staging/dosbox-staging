#!/usr/bin/env bash

# SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
# SPDX-License-Identifier: MIT

# Compiles all commits in the current branch with the specified CMake preset.
# Expects the branch you're running it on to be freshly rebased to the parent
# branch.

#set -x

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'


restore_branch() {
	git checkout "$CURRENT_BRANCH" &> /dev/null
}

compile_commit() {
	local COMMIT
	COMMIT=$1

	local TITLE
	TITLE=$2

	# Only perform a CMake (re-)configure when necessary to save time
	local DO_CONFIGURE

	# Configure if the build directory does not exist (this happens when
	# compiling the first commit)
	if ! test -d "${BUILD_DIR:?}/$CMAKE_PRESET"; then
		DO_CONFIGURE=1
	fi

	# Configure when files have been added, deleted, or renamed
	if [[ $DO_CONFIGURE -eq 0 ]]; then
		if git diff --name-only --diff-filter=m HEAD~1 | grep .\* >/dev/null; then
			DO_CONFIGURE=1
		fi
	fi

	# Configure when any CMake or vcpkg config has been changed
	if [[ $DO_CONFIGURE -eq 0 ]]; then
		if git diff --name-only HEAD~1 | grep -E '^CMake|^vcpkg^' >/dev/null; then
			DO_CONFIGURE=1
		fi
	fi

	# Perform CMake configure if we need to
	if [[ $DO_CONFIGURE -eq 1 ]]; then
		rm -rf $BUILD_DIR

		if ! cmake --preset="$CMAKE_PRESET" >/dev/null; then
			echo
			echo -e "${RED}*** CMake configure failed for commit:"
			echo -e "    $COMMIT $TITLE${NC}"
			restore_branch
			exit 1
		fi
	fi

	# Compile commit
	if ! cmake --build --preset="$CMAKE_PRESET" >/dev/null; then
		echo
		echo -e "${RED}*** ERROR: Building commit failed:"
		echo -e "    $COMMIT $TITLE"
		echo
		echo -e "    Execute the following to reproduce the failure:"
		echo
		echo -e "      git checkout -"
		echo -e "      cmake --build --preset=$CMAKE_PRESET${NC}"
		restore_branch
		exit 1
	fi
}

#-----------------------------------------------------------------------------

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
	echo "Usage: compile-commits.sh CMAKE_PRESET [FROM_HASH]"
	echo
	echo "If FROM_HASH is not specified, the script compiles all commits in the"
	echo "current branch (assuming the parent branch is 'main')."
	exit 1
fi

CMAKE_PRESET=$1
FROM_HASH=$2
BUILD_DIR=build
echo

# Ensure CMake configure is triggered before compiling the first commit
rm -rf "${BUILD_DIR:?}/$CMAKE_PRESET"

PARENT_BRANCH=main
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

if [ -z "$FROM_HASH" ]; then
	COMMITS=$(git log --format="%h" HEAD --not "$PARENT_BRANCH" --reverse)
else
	COMMITS=$(git log --format="%h" "$FROM_HASH"^.. --reverse)
fi
NUM_COMMITS=$(wc -w <<< "$COMMITS")

COMMIT_NO=1
for COMMIT in $COMMITS
do
	git checkout "$COMMIT" &> /dev/null
	TITLE=$(git show --no-patch --format="%s" "$COMMIT")

	printf "${YELLOW}[%2d/%2d] ${CYAN}%s${NC} %s" "$COMMIT_NO" "$NUM_COMMITS" "$COMMIT" "$TITLE"
	echo
	compile_commit "$COMMIT" "$TITLE"

	(( COMMIT_NO++ ))
done

restore_branch

echo
echo -e "${GREEN}Building commits succeeded.${NC}"
