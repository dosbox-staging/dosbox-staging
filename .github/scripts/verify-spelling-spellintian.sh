#!/bin/bash

# Copyright (C) 2020  Feignint <feignint@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Uses spellintian from Debian's lintian package to check pull requests
# for common spelling mistakes.
#
# Prerequisites:
#
#     lintian # sudo apt-get install lintian
#
# Also required but included in base (Linux) github runners:
#
#     bash (>= 4), jq, curl, git, grep, sed
#
# Usage:
#
#     .github/scripts/verify-spelling-spellintian.sh [--picky]
#
###############################################################################
#
# The script automatically operates in two modes: pre-build and post-build
#
# - pre-build  checks the lines added by commit(s) for spelling mistakes and
# reports: "filename:line no.:: misspelt -> correction" to github's API which
# Annotates "Files Changed" and "Job Summary" Page.
#
# - post-build If the script detects src/dosbox, spellintian checks the binary
# Any results from the scan are used to "Bump" the error level of pre-build
# detections, anything at level "error" flags the script to exit non-zero,
# effectivly failing the job run.
#
# As the current verify-bash.sh script stops the job run, it makes sense to run
# this spell check script prior to that, thus typos and shell scripting errors
# can be fixed at the same time.
#
# The post-build run only need be executed on one Linux build job, preferably
# the most recent Ubuntu version available thus newest lintian version.
#
# --picky is an optional mode which includes common capitalisation corrections
#
#     e.g. python -> Python , Gnome -> GNOME
#
# Since this is prone to being incorrect their error level is reduced by one
# meaning the script will not exit non-zero as a result even if in the binary.
# (The script will still exit non-zero if triggered by other spelling errors)
##
# Alternatives
# Since writing this script, codespell has come to my attention
#     https://github.com/codespell-project/codespell
# This does appear to have a superior word list and the output format is much
# better as it includes line number and optionally context. It does lack the
# ability to scan a binary out-of-the-box

set -e

is_pull ()
{
  # "." vs "/" . I've been using refs_pull_nnn_merge.json
  # locally for testing ;)
  [[ ${GITHUB_REF} =~ refs.pull.[0-9]+.merge ]] ||
    {
      >&2 printf "%s " "${0}:" "${GITHUB_REF}" "not a PR, skipped"
      >&2 printf "\n"
      exit 0
    }
}

main ()
{
  is_pull

  for opt in "${@}"
  do
    case ${opt} in
      --picky|--strict|--pedantic)
        # --picky is.. well picky
        picky="--picky"
        # prone to false positives
        ;;
      *)
        : # do nothing, for now
        ;;
    esac
  done

  # report spellintian version
  spellintian --version >&2

  repo_root="$( git -C "${0%/*}" rev-parse --show-toplevel )"
  # really script should bail here if no git

  # Load ignore list
  readarray -t  < <( grep -v "^#" <"${repo_root}/.spellintian-ignore" )
  printf -v filter "%s" "${MAPFILE[@]/%/|}" ; unset MAPFILE
  filter+="\(duplicate word\)"

  if [[ -e "src/dosbox" ]]
  then
      check_binary "src/dosbox"
  else
      :
  fi

  _github_PR
}

check_patchset_in_json ()
{
  # find suspect words
  spellintian $picky < <(
    jq -r '.hunk_line|sub("\\+";"")
          ' <<<"$patch_additions_json"
  )
  # outputs:
  #     misspelt -> correction
}

Get_filename_range ()
{
  # find the typo, and get which filename it belongs to along with range
  # so we don't end up highlighting every occurrence in the file
  # We only want to guard against new typos
  jq -r '
          .|select(.hunk_line|test("\\b'"${TYPO//\"}"'\\b"))
           |.filename + ":" + .range|sub(",";",+")
        ' <<<"$patch_additions_json"
  # outputs:
  #     filename:nnn,+n
}

_github_PR ()
{
  PULL_NUMBER="${GITHUB_REF//[^0-9]}"

  local PULLS="${GITHUB_API_URL}/repos/${GITHUB_REPOSITORY}/pulls/"

  [[ -z $PULL_NUMBER ]] && return ||
  PR_FILES_JSON="$(
      if [[ -f "$GITHUB_REF" ]]
      then
        # craft your own json, to test stuff
        cat "$GITHUB_REF"
      else
        # easier to get details of the PR from github's API than messing about
        # with git, which I'm guessing will be a shallow checkout.
        curl -s -H "Accept: application/vnd.github.v3+json" \
             "${PULLS}${PULL_NUMBER}/files"
        # FIXME do something if curl fails
      fi
  )"

  patch_hunks_json="$(
    jq '
        .[]|select(.patch != null)
           | {
               "filename"   : .filename,
               "patch_hunk" : .patch
                            | split("\n@@")
                            | .[]
                            | split("\n")
                            | group_by(test("@@"))
             }
       ' <<<"${PR_FILES_JSON}"

   # The splits are not very clean
   # but I'm only interested in the "+" lines, so doesn't matter
  )"
  patch_additions_json="$(
    jq '
        .| {
             "filename"   : .filename,
             "hunk_line"  : .patch_hunk[0]|.[]|select(startswith("+")),
             "range"      : .patch_hunk[1]|tostring|gsub(".+\\+| @@.+";"")
           }
       ' <<<"$patch_hunks_json"
  )"

  while read -r
  do
    local TYPO="${REPLY% ->*}"
    local FIX="${REPLY#*-> }"
    while IFS="${IFS/#/:}" read -r FN RANGE
    do
      # Reconstruct "filename: typo -> fix" and see if we should ignore it
      [[ "${FN}: ${REPLY}" =~ ^(${filter})$ ]] && continue

      # Get line number.
      LN="$(
             set -x
             git -C "${repo_root}" grep -h -n "" -- "${FN}" \
             | sed -E -n ''"${RANGE}"'{s/^([0-9]+):.*\b'"${TYPO}"'\b.*/\1/p}'
           )"
      # That looks like cheating but:
      # Consider a patchset with a !fixup that corrects spelling, a prior patch
      # would have the typo that is fixed by the later fixup patch.
      # Without grep|sed one would have to dance to find the offending line's
      # offset from the diff hunk's start line, which is more complicated than
      # it should be ( in jq need to define a for loop to get index of array
      # element containing the typo ) and would still have the !fixup problem.
      # tl;dr *must* double check final file regardless, so use the easy route.

      [[ -z $LN ]] && continue # If we didn't get a line number, assume typo
                               # was fixed by a later commit, e.g. a !fixup

      (
         # subshell, so we can change errlevel without effecting default

         # Lower level if capitalisation correction suggested.
         [[ "${TYPO,,}" == "${FIX,,}" ]] && (( errlevel -- ))

         # Bump warning level if typo was in binary
         [[ ${REPLY} =~ ^(${in_binary})$ ]] && (( errlevel ++ ))

         printf "::%s file=%s,line=%d::%s: %s\n" \
                "${errlevels[$errlevel]}" "${FN}" "${LN}" "${FN}" "${REPLY}"
      )
    done < <( Get_filename_range )
  done < <( check_patchset_in_json )
}

check_binary ()
{
  while read -r
  do
    typo_in_binary+=( "${REPLY#*: }" )
    printf "::error::%s\n" "${REPLY}"
  done < <( spellintian $picky "${1:-src/dosbox}" )
  local IFS="|"
  printf -v in_binary "%s" "${typo_in_binary[*]}"
  # If typo was in binary, kill the bird.
  [[ -z ${in_binary} ]] || Canary=dead
  # NOTE: The ignore list is not used here
  # This hasn't been a problem thus far
}

Canary=alive
# Annotations
errlevels=( debug warning error )
errlevel=1 # default, warning

main "${@}"

[[ $Canary == alive ]]
exit 1
