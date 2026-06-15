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

print_failure() {
	local REASON=$1
	local COMMIT=$2
	local TITLE=$3
	local REPRO_CMD=$4

	echo
	echo -e "${RED}*** ERROR: $REASON:"
	echo -e "    $COMMIT $TITLE"
	echo
	echo -e "    Execute the following to reproduce the failure:"
	echo
	echo -e "      git checkout -"
	echo -e "      $REPRO_CMD${NC}"
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

		local CONFIGURE_CMD="cmake --preset=$CMAKE_PRESET"
		if ! $CONFIGURE_CMD >/dev/null; then
			print_failure "CMake configure failed for commit" \
				"$COMMIT" "$TITLE" "$CONFIGURE_CMD"
			restore_branch
			exit 1
		fi
	fi

	# Compile commit
	local BUILD_CMD="cmake --build --preset=$CMAKE_PRESET"
	if ! $BUILD_CMD >/dev/null; then
		print_failure "Building commit failed" \
			"$COMMIT" "$TITLE" "$BUILD_CMD"
		restore_branch
		exit 1
	fi
}

run_tests() {
	local COMMIT
	COMMIT=$1

	local TITLE
	TITLE=$2

	local CMD=(GTEST_COLOR=1 ctest -j 8 --preset "$CMAKE_PRESET")
	if [[ -n $TEST_FILTER ]]; then
		CMD+=(-R "$TEST_FILTER")
	fi

	if ! env "${CMD[@]}"; then
		print_failure "Tests failed for commit" \
			"$COMMIT" "$TITLE" "${CMD[*]}"
		restore_branch
		exit 1
	fi
}

usage() {
	echo "Usage: compile-commits.sh CMAKE_PRESET [FROM_HASH] [--test] [--test-filter PATTERN]"
	echo
	echo "If FROM_HASH is not specified, the script compiles all commits in the"
	echo "current branch (assuming the parent branch is 'main')."
	echo
	echo "  --test                  Run the ctest suite after each commit compiles."
	echo "  --test-filter PATTERN   Run only tests matching PATTERN (implies --test)."
	exit 1
}

#-----------------------------------------------------------------------------

RUN_TESTS=0
TEST_FILTER=
POSITIONAL=()

while [[ $# -gt 0 ]]; do
	case $1 in
		--test)
			RUN_TESTS=1
			shift
			;;

		--test-filter)
			if [[ -z ${2:-} ]]; then
				echo "Error: --test-filter requires a PATTERN argument."
				echo
				usage
			fi
			RUN_TESTS=1
			TEST_FILTER=$2
			shift 2
			;;

		-h|--help)
			usage
			;;

		--*)
			echo "Error: unknown option: $1"
			echo
			usage
			;;

		*)
			POSITIONAL+=("$1")
			shift
			;;
	esac
done

if [[ ${#POSITIONAL[@]} -lt 1 || ${#POSITIONAL[@]} -gt 2 ]]; then
	usage
fi

CMAKE_PRESET=${POSITIONAL[0]}
FROM_HASH=${POSITIONAL[1]:-}
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

	if [[ $RUN_TESTS -eq 1 ]]; then
		run_tests "$COMMIT" "$TITLE"
	fi

	(( COMMIT_NO++ ))
done

restore_branch

echo
echo -e "${GREEN}Building commits succeeded.${NC}"
