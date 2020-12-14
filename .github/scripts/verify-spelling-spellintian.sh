#!/bin/bash

# Copyright (C) 2020  Feignint <feignint@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

##
# TODO Usage / Summary
#
##

set -e

main ()
{
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

  repo_root="$( git -C "${0%/*}" rev-parse --show-toplevel )"

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
}

PR_comment ()
{
  : # TODO
}

Canary=alive
# Annotations
errlevels=( debug warning error )
errlevel=1 # default, warning

main "${@}"

[[ $Canary == alive ]]
