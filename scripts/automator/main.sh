#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Automator is a generic automation tool that helps separate data
# from logical code.  It uses one or more "variables" files placed
# in a single sub-directory, that are sourced in logical succession,
# such as variables defined in earler files are either added-to or
# overridden in subsequent files.
#
# For a thorough example, see scripts/build.sh and its data files
# in scripts/automator/build.
#
# TODO: This script manages variables using eval meta-programming.
#       Although the syntax is quite readable under bash 4.x,
#       we have limitted our syntax to the uglier and heavier bash 3.x
#       because Apple is still only shipping bash 3.x, even on their
#       latest operating system (Nov-2019).
#
#       Future work might involve switching this to python and using YAML
#       syntax in the variables files. Python's dictionaries can be
#       merged or have their values overridden, and key names can be readily
#       manipulated.
#
set -euo pipefail

function underline() {
	echo ""
	echo "$1"
	echo "${1//?/${2:--}}"
}

function lower() {
	echo "${1}" | tr '[:upper:]' '[:lower:]'
}

function upper() {
	echo "${1}" | tr '[:lower:]' '[:upper:]'
}

function arg_error() {
	local PURPLE='\033[0;34m'
	local BLACK='\033[0;30m'
	local ON_WHITE='\033[47m'
	local NO_COLOR='\033[0m'
	>&2 echo -e "Please specify the ${BLACK}${ON_WHITE}${1}${NO_COLOR} argument with one of: ${PURPLE}${2}${NO_COLOR}"
	exit 1
}

function import() { # arguments: category instance
	category=$(lower "${1}")
	instance=$(lower "${2}")
	if [[ -z "$instance" ]]; then
		instances=( "$(find "${data_dir}" -name "${category}-*" -a ! -name '*defaults' | sed 's/.*-//' | xargs)" )
		arg_error "--${category}" "${instances[*]}"
	else for instance in defaults "${instance}"; do
		varfile="${data_dir}/${category}-${instance}"
		# shellcheck disable=SC1090,SC1091
		if [[ -f "$varfile" ]]; then source "$varfile"
		else >&2 echo "${varfile} does not exist, skipping"; fi; done
	fi
}

function construct_environment() {
	# underline "Environment"
	for base_var in "${VARIABLES[@]}"; do
		# First check if we have a build-specific variable
		# shellcheck disable=SC2154
		if [[ -n "$(eval 'echo ${'"${base_var}_${selected_type}"'[*]:-}')" ]]; then
			var_name="${base_var}_${selected_type}"
		# Otherwise fallback to the default variable
		else var_name="${base_var}"; fi

		# Aggregate any modifiers, which are stand-alone arguments
		mod_values=""
		# shellcheck disable=SC2154
		for mod_upper in "${modifiers[@]}"; do
			mod=$(lower "${mod_upper}")
			# Mods can only add options to existing VARs, so we check for those
			if [[ -n "$(eval 'echo ${'"${base_var}_${mod}"'[*]:-}')" ]]; then
				var_with_mod="${base_var}_${mod}"
				# shellcheck disable=SC2034
				mod_values=$(eval 'echo ${mod_values} ${'"${var_with_mod}"'[*]}')
			fi
		done

		# Expand our variable array (or scalar) and combine it with our aggregated
		# modification string. Echo takes care of spacing out our variables without
		# explicit padding, because it either drops empty variables or adds spaces
		# between them if they exist.
		# shellcheck disable=SC2034
		combined=$(eval 'echo ${'"${var_name}"'[*]} ${mod_values}')
		env_var=$(upper "${base_var}")
		eval 'export '"${env_var}"'="${combined}"'
		# echo "${env_var}=\"$(eval echo \$"${env_var}")\""
	done
}

function perform_steps() {
	# underline "Launching"
	for step in "${STEPS[@]}"; do
		if type -t "$step" | grep -q function; then "$step"
		else
			# eval 'echo ${'"${step}"'[*]}'
			eval '${'"${step}"'[*]}'
		fi
	done
}

function main() {
	cd "$(dirname "$0")"/..
	export repo_root
	repo_root="$PWD"
	cd scripts/automator
	if [[ -z "${data_dir:-}" ]]; then data_dir="$(basename "$0" '.sh')"; fi
	parse_args "$@"
	construct_environment
	perform_steps
}

main "$@"

