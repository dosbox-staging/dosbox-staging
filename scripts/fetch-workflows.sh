#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Kevin R. Croft <krcroft@gmail.com>

##
#  This script crawls the current repo's GitHub workflow content.
#  It fetches CI records for the provided branches, or by default
#  the latest CI runs for the main branch and currently-set branch.
#
#  The goal of this script is two fold:
#    - Provide a mechanized an automated way to fetch CI records.
#    - Provide a rapid way to diff bad CI runs against the main branch.
#
#  This script requires a GitHub account in order to generate an
#  auth-token. Simply run the script, it will provide instructions.
#

# Bash strict-mode
set -euo pipefail
shopt -s nullglob

# Fixed portion of GitHub's v3 REST API URL
declare -gr scheme="https://"
declare -gr authority="api.github.com"

# File purge thresholds
max_asset_age_days="2"
max_cache_age_minutes="5"

# Colors
declare -gr bold="\\e[1m"
declare -gr red="\\e[31m"
declare -gr green="\\e[32m"
declare -gr yellow="\\e[33m"
declare -gr cyan="\\e[36m"
declare -gr reset="\\e[0m"


##
#  Parse the command line arguments that will determine
#  which branches to pull and diff
#
function parse_args() {
	branches=()

	if [[ "$*" == *"help"* || "${#}" -gt "2" ]]; then
		echo ""
		echo "Usage: $0 [BRANCH_A] [BRANCH_B]"
		echo ""
		echo " - If only BRANCH_A is provided, then just download its records"
		echo " - If both BRANCH_A and B are provided, then fetch and diff them"
		echo " - If neither are provided, then fetch and diff the main branch vs current branch"
		echo " - 'current' can be used in-place of the repo's currently set branch name"
		echo " - Note: BRANCH_A and B can be the same; the tool will try to diff"
		echo "         the last *good* run versus latest run (which might differ)"
		echo ""
		exit 1
	elif [[ "${#}" == "1" ]]; then
		branches+=( "$(get_branch "$1")" )
	elif [[ "${#}" == "2" ]]; then
		branches+=( "$(get_branch "$1")" )
		branches+=( "$(get_branch "$2")" )
	else
		branches+=( "main" )
		branches+=( "$(get_branch current)" )
	fi
}

##
#  Returns the branch name either as-is or gets the current
#  actual branch name, if the keyword 'current' is provided
#
function get_branch() {
	local branch
	if [[ "$1" == "current" ]]; then
		init_local_branch
		branch="$local_branch"
	else
		branch="$1"
	fi
	echo "$branch"
}

##
#  Checks if the script's dependencies are available
#
function check_dependencies() {
	local missing=0
	for prog in git jq curl unzip chmod date stat dirname basename diff; do
		if ! command -v "$prog" &> /dev/null; then
			echo -e "The command-line tool \"${bold}${red}${prog}${reset}\" "\
			        "is needed but could not be run; please install it."
			(( missing++ ))
		fi
	done
	if [[ "$missing" -gt 0 ]]; then
		exit 1
	fi
}

##
#  Changes the working directory to that of the
#  repository's root.
#
function cd_repo_root() {
	if [[ "${in_root:-}" != "true" ]]; then
		local script_path
		script_path="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
		pushd "$script_path" > /dev/null
		pushd "$(git rev-parse --show-toplevel)" > /dev/null
		in_root=true
	fi
}

##
#  Determines the full GitHub repo name using the
#  remote origin set in the repository's configuration
#
function init_baseurl() {
	# Guard against subsequent calls
	if [[ -n "${baseurl:-}" ]]; then
		return 0
	fi
	cd_repo_root
	# Extract the full GitHub repo name from the origin
	repo="$(git config --get remote.origin.url | sed 's/.*://;s/\.git$//;s^//[^/]*/^^')"
	baseurl="$scheme/$authority/repos/$repo/actions"
	declare -gr repo
	declare -gr baseurl
}

##
#  Determines the local branch name
#
function init_local_branch() {
	# Guard against subsequent calls
	if [[ -n "${local_branch:-}" ]]; then
		return 0
	fi
	cd_repo_root
	local_branch="$(git rev-parse --abbrev-ref HEAD)"
	declare -gr local_branch
}

##
#  Returns the path to Git's configuration file
#  specifically holding the user's credentials.
#
function get_credential_file() {
	init_baseurl
	git config --show-origin "credential.$baseurl.username" \
	| sed 's/.*://;s/\t.*$//'
}

##
#  Gets or sets credentials, depending on the action
#  Action can be "global" or "get", and value can be empty.
#
function manage_credential() {
	init_baseurl
	local action="$1"
	local key="$2"
	local value="${3:-}"
	git config --"${action}" "credential.$baseurl.$key" "$value"
}

##
#  Test if the credentials work with GitHub
#
function test_credentials() {
	if pull workflows | grep -q "Bad credentials"; then
		local test_url="https://api.github.com/repos/octo-org/octo-repo/actions/workflows"
		echo "The provided credentials failed; please test them with this command:"
		echo ""
		echo "  curl -l -u \"EMAIL:TOKEN\" \"$test_url\""
		echo ""
		exit 1
	fi
}

##
#  Initializes credentials for GitHub's v3 API server.
#
#  We use Git's recommended credential mechanism described here:
#  https://git-scm.com/docs/gitcredentials, along with protecting
#  their Git configuration file.
#
#  We store the credentially globally for two reasons:
#  - auth-tokens are not repo-specific, and can be used github-wide
#  - auto-tokens are not recoverable from the website,
#    so storing globally ensure that if the user clones the
#    repo again, their token will still be useable (and not lost).
#
function init_credentials() {
	# Attempt to fetch previously setup credentials
	if username="$(manage_credential get username)" && \
	   token="$(manage_credential get token)"; then
	   echo "Credentials loaded from: $(get_credential_file)"
	   return 0
	fi

	# Otherwise ask the user for their email and token;
	# One-time setup.

	# Check if we can pre-populate the username field with
	# their existing email address, but only if it's valid:
	username="$(git config --get user.email)"
	if [[ -z "$username" || "$username" == *"noreply"* ]]; then
		username=""
	fi

	# Prompt the user for their account email address:
	echo ""
	echo "1. Enter your GitHub account email address, example: jcousteau@scuba.fr"
	echo "   Note, this is your real signup address, not GitHub's no-reply address."
	echo ""
	read -r -e -i "$username" -p "GitHub account email: " username

	# Help the user generate and enter a minimal-access token:
	echo ""
	echo "2. Login to GitHub and visit https://github.com/settings/tokens"
	echo ""
	echo "3. Click 'Generate new token' and set the 'public_repo' permission:"
	echo ""
	echo "    [ ]  repo                  Full control of private repos"
	echo "        [ ] repo:status        Access commit status"
	echo "        [ ] repo_deployment    Access deployment status"
	echo "        [X] public_repo        Access public repositories"
	echo "        [ ] repo:invite        Access repository invitations"
	echo ""
	echo "   Type a name for the token then scroll down and click 'Generate Token'."
	echo ""
	echo "4. Copy & paste your token, for example: f7e6b2344bd2c1487597b61d77703527a692a072"
	echo ""
	# Note: We deliberately echo the token so the user can verify its correctness
	read -r -p "Personal Access Token: " token
	echo ""

	# Add the credentials per Git's recommended mechanism
	if [[ -n "${username:-}" && -n "${token:-}" ]]; then
		test_credentials
		manage_credential global username "$username"
		manage_credential global token "$token"
		local credential_file
		credential_file="$(get_credential_file)"
		echo "Added your credentials to $credential_file"

		# If we made it here, then the above commands succeeded and the credentials
		# are added. It's now our responsibility to lock-down the file:
		chmod 600 "$credential_file"
		echo "Applied user-access-only RW (600) permissions to $credential_file"
	else
		echo "Failed to setup credentials or some of the credentials were empty!"
		exit 1
	fi
}

##
#  Makes strings suitable for directory and filenames
#   - spaces      => underscores
#   - upper-case  => lower-case
#   - slashes     => dashes
#   - parenthesis => stripped
#   - equals      => dashes
#
function sanitize_name() {
	local name="$1"
	echo "$name" | sed 's/\(.*\)/\L\1/;s/ /_/g;s^/^-^g;s/[()]//g;s/=/-/g'
}

##
#  Return how old a file or directory is, respectively
#
function seconds_old() { echo $(( "$(date +%s)" - "$(stat -L --format %Y "$1")" )); }
function minutes_old() { echo $(( "$(seconds_old "$1")" / 60 )); }
function days_old() { echo $(( "$(seconds_old "$1")" / 86400 )); }

##
#  Creates a storage area for all content fetched by the script.
#  This include cached JSON output (valid for 5 minutes), along
#  with zip assets, log files, and diffs.
#
declare -g storage_dir # used by the trap
function init_dirs() {
	local repo_postfix
	repo_postfix="$(basename "$repo")"
	storage_dir="/tmp/$repo_postfix-workflows-$USER"
	cache_dir="$storage_dir/cache"
	declare -gr cache_dir
	echo "Initializing storage area: $storage_dir"

	# Cleanup run directories, if they exist
	for run_dir in "$storage_dir/"*-*-*; do
		if [[ -f "$run_dir/interrupted"
		   || "$(days_old "$run_dir")" -gt "$max_asset_age_days" ]]; then
			rm -rf "$run_dir"
		fi
	done
	unset run_dir

	# Clean up the cache directory and content
	if [[ -f "$cache_dir/interrupted" ]]; then
		rm -rf "$cache_dir"
	fi
	if [[ -d "$cache_dir" ]]; then
		for filename in "$cache_dir"/*; do
			if [[ "$(minutes_old "$filename")" -gt "$max_cache_age_minutes" ]]; then
				rm -f "$filename"
			fi
		done
	else
		mkdir -p "$cache_dir"
	fi
	trap 'interrupt' INT
}

##
#  Perform post-exit actions if the user Ctrl-C'd the job.
#  Some files might be partially written, so drop a breadcrumb
#  to clean up next run.
#
function interrupt() {
	if [[ -n "${run_dir:-}" ]]; then
		touch "$run_dir/interrupted"
	fi
	touch "$cache_dir/interrupted"
	echo " <== OK, stopping."
	echo ""
	echo -e "Partial logs available in: ${bold}${storage_dir}${bold}${reset}"
	echo "They will be purged next run."
	echo ""
}

##
#  Downloads a file if we otherwise don't have it.
#  (Note that the script on launch cleans up files older than
#  5 minutes, so most of the time we'll be downloading.)
#
function download() {
	local url="$1"
	local outfile="$2"
	if [[ ! -f "$outfile" ]]; then
		curl -u "$username:$token" \
		     --silent              \
		     --location            \
		     --output "$outfile"   \
		     "$url"
	fi
}

##
#  Unzips files inside their containing directory.
#  Clobbers existing files.
#
function unpack() {
	local zipfile="$1"
	local zipdir
	zipdir="$(dirname "$zipfile")"
	pushd "$zipdir" > /dev/null
	unzip -qq -o "$zipfile"
	rm -f "$zipfile"
	popd > /dev/null
}

##
#  Constructs and fetches REST urls using our personal access
#  token. Files are additionally hashed based on the REST URL
#  and cached.  This allows for rapid-rerunning without needing
#  to hit GitHub's API again (for duplicate requests). This
#  avoid us exceeding our repo limit on API calls/day.
#
function pull() {
	# Buildup the REST URL by appending arguments
	local url="$baseurl"
	for element in "$@"; do
		url="$url/$element"
	done
	local url_hash
	url_hash="$(echo "$url" | md5sum | cut -f1 -d' ')"
	local outfile="${cache_dir}/${url_hash}.json"
	if [[ ! -f "$outfile" ]]; then
		download "$url" "$outfile"
	fi
	cat "$outfile"
}

##
#  Gets one or more keys from all records
#
function get_all() {
	local container="$1"
	local return_keys="$2"
	jq -r '.'"$container"'[] | '"${return_keys}"
}

##
#  Gets one or more return_key(s) from records that have
#  matching search_key and search_value hits
#
function query() {
	local container="$1"
	local search_key="$2"
	local search_value="$3"
	local return_keys="$4"
	jq -r --arg value "$search_value"\
	'.'"${container}"'[] | if .'"${search_key}"' == $value then '"${return_keys}"' else empty end'
}

##
#  Pulls the subset of active workflows from GitHub having
#  path values that match the local repos filenames inside
#  .github/workflows (otherwise there are 30+ workflows).
#
#  The workflow numeric ID and textual name are stored
#  in an associated array, respectively.
#
#  API References:
#   - https://developer.github.com/v3/actions/workflows/
#   - GET /repos/:owner/:repo/actions/workflows
#
function fetch_workflows() {
	unset workflows
	declare -gA workflows
	for workflow_path in ".github/workflows/"*.yml; do

		# Guard: skip our Config heavy and Coverity analysis workflows
		if [[ "$workflow_path" == *"config.yml"*
		   || "$workflow_path" == *"coverity.yml"* ]]; then
			continue
		fi

		local result
		result="$(pull workflows \
	            | query workflows path "$workflow_path" '.id,.name')"
		local id
		id="${result%$'\n'*}"
		local name
		name="${result#*$'\n'}"

		# Guard: skip any workflows that result in empty values
		if [[ -z "${id:-}" || -z "${name:-}" ]]; then
			continue
		fi
		workflows["$id"]="$(sanitize_name "$name")"
	done
}

##
#  Fetches the first run identifier for a given workflow ID
#  and branch name. The run ID is stored in the run_id variable.
#
#  API References:
#   - https://developer.github.com/v3/actions/workflow_runs
#   - GET /repos/:owner/:repo/actions/runs/:run_id
#
function fetch_workflow_run() {
	declare -g run_id
	local workflow_id="$1"
	local branch="$2"
	# GET /repos/:owner/:repo/actions/workflows/:workflow_id/runs
	run_id="$(pull workflows "$workflow_id" runs \
	       | query workflow_runs head_branch "$branch" '.id' \
	       | head -1)"
}

##
#  Fetches artifact names and download URLs for a given run ID,
#  and stored them in an assiciative array, respectively.
#
#  API References:
#   - https://developer.github.com/v3/actions/artifacts
#   - GET /repos/:owner/:repo/actions/runs/:run_id/artifacts
#
function fetch_run_artifacts() {
	unset artifacts
	declare -gA artifacts
	while read -r name; do
		read -r url
		sanitized_name="$(sanitize_name "$name")"
		artifacts["$sanitized_name"]="$url"
	done < <(pull runs "$run_id" artifacts \
	         | get_all artifacts '.name,.archive_download_url')
}

##
#  Fetches the job IDs and job names for a given run ID.
#  The job IDs and names are stored in an associative array,
#  respectively.
#
#  API References:
#   - https://developer.github.com/v3/actions/workflow_jobs
#   - GET /repos/:owner/:repo/actions/runs/:run_id/jobs
#
function fetch_run_jobs() {
	unset jobs_array
	declare -gA jobs_array
	local conclusion="$1" # success or failure
	while read -r id; do
		read -r name
		jobs_array["$id"]="$(sanitize_name "$name")"
	done < <(pull runs "$run_id" jobs \
	         | query jobs conclusion "$conclusion" '.id,.name')
}

##
#  Fetches a job's log, and saves it in the provided output
#  filename. The logs prefix time-stamps are filtered for easier
#  text processing.
#
#  API References:
#   - https://developer.github.com/v3/actions/workflow_jobs
#   - GET /repos/:owner/:repo/actions/jobs/:job_id/logs
#
function fetch_job_log() {
	local jid="$1"
	local outfile="$2"
	pull jobs "$jid" logs \
	| sed 's/^.*Z[ \t]*//;s/:[[:digit:]]*:[[:digit:]]*://;s/\[/./g;s/\]/./g' \
	> "$outfile"
}

##
#  Ensures all pre-requisites are setup and have passed
#  before we start making REST queries and writing files.
#
function init() {
	parse_args "$@"
	check_dependencies
	init_baseurl
	init_dirs
	init_credentials
}

##
#  Crawl workflows, runs, and jobs for provided branches.
#  While crawling, download assets and logs, and if a run failed, diff
#  that log against the other branch's.
#
#  TODO - Refactor into smaller functions and trying to flatten the loop depth.
#  TODO - Improve the log differ to something that can lift out just the
#         gcc/clang/vistual-studio warnings and errors, and diff them.
#
function main() {
	# Setup all pre-requisites
	init "$@"
	echo "Operating on branches: ${branches[*]}"
	echo ""

	# Fetch the workflows to be used throughout the run
	fetch_workflows

	# Step through each workflow
	for workflow_id in "${!workflows[@]}"; do
		workflow_name="${workflows[$workflow_id]}"
		echo -e "${bold}${workflow_name}${reset} workflow [$workflow_id]"

		# Create state-tracking variables
		first="true"
		prior_run_dir=""
		prior_branch_name=""

		# Within the workflows, we're interested in finding the newest subset of
		# runs that match the given branch
		for branch in "${branches[@]}"; do
			branch_name="$(sanitize_name "$branch")"

			if [[ "$first" == "true" && "${#branches[@]}" == "2" ]]; then
				run_joiner="|-"
				job_joiner="|"
				first="false"
			else
				run_joiner="\`-"
				job_joiner=" "
			fi

			# Get the run identifier for the given workflow ID and branch name
			echo -ne "  $run_joiner ${bold}${branch}${reset} "
			fetch_workflow_run "$workflow_id" "$branch"
			if [[ -z "${run_id:-}" ]]; then
				echo "no runs found [skipping]"
				continue
			fi

			# Create the branch's run directory, if needed
			run_dir="$storage_dir/$branch_name-$workflow_name-$run_id"
			if [[ ! -d "$run_dir" ]]; then
				mkdir -p "$run_dir"
				echo "run [$run_id, fetching]"
			else
				echo "run [$run_id, already fetched]"
				prior_branch_name="$branch_name"
				prior_run_dir="$run_dir"
				continue
			fi

			# Download the artifacts produced during the selected run
			fetch_run_artifacts
			for artifact_name in "${!artifacts[@]}"; do
				artifact_url="${artifacts[$artifact_name]}"
				asset_file="$run_dir/$artifact_name.zip"
				download "$artifact_url" "$asset_file"
				unpack "$asset_file"
				echo -e "  $job_joiner     - unpacking ${cyan}${artifact_name}${reset} asset"
			done

			# Download the logs for the jobs within the selected run
			for conclusion in failure success; do
				if ! fetch_run_jobs "$conclusion"; then
					echo "      \`- skipped $job_id $conclusion"
					continue
				fi
				[[ "$conclusion" == "success" ]] && color="${green}" || color="${red}"
				for job_id in "${!jobs_array[@]}"; do
					job_name="${jobs_array[$job_id]}"
					echo -e "  $job_joiner     - fetching  ${color}${job_name}${reset} ${conclusion} log"
					log_file="$run_dir/$job_name-$conclusion.txt"
					successful_prior_log="$prior_run_dir/$job_name-success.txt"
					fetch_job_log "$job_id" "$log_file"

					# In the event we've found a failed job, try to diff it against a prior
					# successful main job of the equivalent workflow and job-type.
					if [[ "$conclusion" == "failure"
					&& -f "$log_file"
					&& -f "$successful_prior_log" ]]; then
						diff_file="$run_dir/$job_name-$branch_name-vs-$prior_branch_name.txt"
						diff "$log_file" "$successful_prior_log" > "$diff_file" || true
						echo -e "        - diffed    ${yellow}$diff_file${reset}"
					fi
				done # jobs_array loop
			done # conclusion loop
			echo "  $job_joiner"
			prior_branch_name="$branch_name"
			prior_run_dir="$run_dir"
		done # branch loop
		echo ""
	done # workflow loop
	echo -e "Copy of logs in: ${bold}${storage_dir}${bold}"
	echo ""
}

main "$@"
