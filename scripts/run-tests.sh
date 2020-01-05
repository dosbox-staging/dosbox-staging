#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Please see run-tests.md for documentation.

set -euo pipefail
shopt -s nullglob

function print_usage() {
	echo "Usage: $0 [-d dosbox-binary] [-j N] [-t my-test-case-1 [-t my-test-case-2 [...] ] ]"
	echo "Where:"
	echo "  -d | --dosbox   test with a specific DOSBox binary instead of the repo's binary"
	echo "  -j | --jobs     only launch N jobs in parallel instead of auto-determined"
	echo "  -t | --testcase run a specific test-case intead of all of them, can use multiple times"
	echo ""
}

function parse_args() {
	# Move to our repo root
	cd "$(git rev-parse --show-toplevel)"

	# Set default variables
	dosbox="$(realpath src/dosbox)"
	harness="$(realpath scripts/exit-after.sh)"
	jobs=$(( 3 * $(nproc) )) # basis: typical cores can run 3 instances of DOSBox at 50k cycles
	testdirs=() # all test-case dirs executed by default

	# Parse the command-line and override default variables
	while [[ "${#}" -gt 0 ]]; do case ${1} in
		-d|--dosbox)       dosbox="${2}";      shift 2;;
		-j|--jobs)         jobs="${2}";        shift 2;;
		-t|--testcase)     testdirs+=("${2}"); shift 2;;
		-h|--help)         print_usage;        exit 1;;
		*) >&2 echo "Unknown parameter: ${1}"; exit 1;;
	esac; done
}

function check_vars() {
	local rcode=0
	if [[ ! -x "${dosbox}" ]]; then
		echo "The dosbox binary, ${dosbox}, does not exit or is not executable"
		rcode=1
	fi
	local max_jobs=$(( 4 * 128 * 3)) # circa-2020 peak threads: quad-socket x 128-thread processor
	if [[ "${jobs}" -lt "1" && "${jobs}" -gt "${max_jobs}" ]]; then
		echo "Jobs is set to: ${jobs}, but should be between 1 and ${max_jobs}"
		rcode=1
	fi
	return "${rcode}"
}

function batch_files_end_with_exit() {
	local rcode=0
	for batfile in "${batfiles[@]}"; do
		if [[ "$(grep ^exit "${batfile}" | tail -1)" != "exit" ]]; then
			echo ""
			echo "  - $batfile needs to end with an exit command, but does not"
			rcode=1
		fi
	done
	return "${rcode}"
}

function generate_launch_commands() {
	baseconf=""
	# Do we have a common config for all the cases?
	if [[ -f dosbox.conf ]]; then
		baseconf="-conf dosbox.conf"
	fi

	# Sequentially launch jobs
	for batfile in "${batfiles[@]}"; do
		# Reset variables that are configurable in the VARS file
		RUNNING_AFTER="true"
		RUNTIME="15"
		GRACETIME="3"
		ARGS=""

		# Descriptive variables derived from our batch-file name
		jobname="${batfile%.*}"
		joblog="$jobname.log"
		jobconf="$jobname.conf"
		jobvars="$jobname.vars"
		conf=""

		# Pull in any job variables
		if [[ -f "${jobvars}" ]]; then
	        # shellcheck disable=SC1090,SC1091
			source "${jobvars}"
		fi

		# Apply the job-specific configuration to DOSBox
		if [[ -f "${jobconf}" ]]; then
			conf="${baseconf:-} -conf ${jobconf}"
		fi

		# Print the launch command
		echo "${harness}" "${RUNNING_AFTER}" "${RUNTIME}" "${GRACETIME}" "${joblog}" "${jobname}" "${dosbox}" "${batfile}" "${conf}" "${ARGS}"
	done
}

function run_tests() {
	# Move to our test-cases directory
	cd "$(git rev-parse --show-toplevel)/contrib/test-cases"

	# By default we walk all the sub-directories,
	# otherwise we only test the -t <test-case> ... directories
	if [[ "${#testdirs[@]}" == "0" ]]; then
		testdirs+=(*/)
	fi

	local rcode=0
	for testdir in "${testdirs[@]}"; do
		# Enter the test directory
		popd &> /dev/null || true
		pushd "${testdir}" &> /dev/null

		echo -n "${testdir%/}: "
		batfiles=( *.bat )

		# Confirm the batch-files end with an exit command
		batch_files_end_with_exit

		if [[ -f "${batfiles[0]:-}" ]]; then
			echo "running ${#batfiles[@]} tests"
			if ! generate_launch_commands | parallel --jobs "${jobs}" --delay 0.5 --halt "soon,fail=20%"; then
				# Parallel returns non-zero if any sub-job fails, which we track here
				rcode=1
			fi
		else
			echo "no batch-files, [skipped]"
		fi
	done
	return "${rcode}"
}

parse_args "$@"
check_vars
run_tests
