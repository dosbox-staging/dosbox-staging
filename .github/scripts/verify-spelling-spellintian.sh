#!/bin/bash

# Copyright (C) 2020  Feignint <feignint@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

# files to check by default
# Note: if src/dosbox exists it will also be checked
check_files=( "*.ac" "*.md" "*.MD" "README*" "dosbox.1" "INSTALL" )

# Annotations
errlevels=( debug warning error )
# default is warning
# Anything in src/dosbox is error
# however: Capitalisation errors reduce level by one

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
      --source|--more)
        extrafiles=yes
        check_files+=( "*.h" "*.cpp" "*.c" )
        # mostly seems to pick-up typos in comments
        # important typos will be picked up in the src/dosbox
        # no real need for this in github_CI
        ;;
      --all)
        extrafiles=yes
        check_files+=( "." )
        # will check everything
        # well, nearly everything,
        # contrib/translations and .gitignore have been 'hardcode' excluded
        ;;
      --dosbox|--binary)
        skip_list_files="true"
        ;;
      --github-PR)
        github_PR=yes
        # will only check files changed by the PR
        # This is recomended mode for github_CI
        ;;
      --docs)
        extrafiles=yes
        ;;
      *)
        : # do nothing, for now
        ;;
    esac
  done

  repo_root="$( git -C "${0%/*}" rev-parse --show-toplevel )"

  [[ -z $github_PR ]] || _github_PR
  readarray -t -O 0 file_list < <( list_files )

  # If no files to check
  [[ ${#file_list[@]} -gt 0 ]] || exit 0

  # Load ignore list
  readarray -t  < <( grep -v "^#" <"${repo_root}/.spellintian-ignore" )
  printf -v filter "%s" "${MAPFILE[@]/%/|}" ; unset MAPFILE
  filter+="\(duplicate word\)"

  _github_output
}

list_files ()
{
  [[ ${skip_list_files} ]] ||
    git -C "${repo_root}" ls-files -- \
      ':!contrib/translations' \
      ':!.gitignore' \
      "${check_files[@]}"

  [[ -e ${repo_root}/src/dosbox ]] &&
    printf "%s\n" "src/dosbox"
}

check_spelling ()
{
  # shellcheck disable=SC2086
  spellintian $picky "${file_list[@]/#/${repo_root}\/}"
}

Get_context ()
{
  # get "offending" lines to offer context
  local MIMEtype
  MIMEtype="$( file --brief --mime-type "${repo_root}/${FILENAME%:}" )"
  BinaryMIMEtypes="application/(x-pie-executable|x-sharedlib)"

  # shellcheck disable=2046
  git -C "${repo_root}" grep -n -P "\b${TYPO}\b" \
      $( [[ ${MIMEtype#*: }  =~ ${BinaryMIMEtypes}$ ]] ||
           printf "%s" "-- ${FILENAME%:}" )
}

_github_output ()
{
  # I have not managed to get ::group:: to work
  #echo "::group::Spellcheck"
  while read -r FILENAME TYPO_FIX
  do
    FILENAME="${FILENAME#${repo_root}/}"
    TYPO_FIX="${TYPO_FIX//\"}"
        TYPO="${TYPO_FIX% ->*}"
         FIX="${TYPO_FIX#*-> }"

    [[ "${FILENAME} ${TYPO_FIX}" =~ ^(${filter})$ ]] && continue

    [[ ${FILENAME%:} == src/dosbox ]] && errlevel=2 || errlevel=1

    # Reduce error level by one if Capitalisation
    [[ "${TYPO,,}" != "${FIX,,}" ]] || (( errlevel -- ))

    # shellcheck disable=2034
    while IFS=: read -r FN LN LINE
    do
      printf  "::%s file=%s,line=%d::%s: %s\n" \
              "${errlevels[$errlevel]}"  \
              "${FN}" "${LN}" "${FN}" "${TYPO_FIX}"

      if [[ ${errlevels[${errlevel}]} == error ]]
      then
        Canary=dead
      fi
    done < <( Get_context )
  done < <( check_spelling )
  # I have not managed to get ::group:: to work
  #echo "::endgroup::"
}

_github_PR ()
{
  # These will be set by the github runner
  #local GITHUB_REF="refs/pull/763/merge"
  #local GITHUB_API_URL="https://api.github.com"
  #local GITHUB_REPOSITORY="dosbox-staging/dosbox-staging"
  ## uncomment if testing this function locally
  # or download the json to local file, e.g.
  # local GITHUB_REF="pulls_763_files.json"

  PULL_NUMBER="${GITHUB_REF//[^0-9]}"

  local PULLS="${GITHUB_API_URL}/repos/${GITHUB_REPOSITORY}/pulls/"

  [[ -z $PULL_NUMBER ]] && return ||
  PR_FILES_JSON="$(
      if [[ -f "$GITHUB_REF" ]]
      then
        cat "$GITHUB_REF"
      else
        curl -s -H "Accept: application/vnd.github.v3+json" \
             "${PULLS}${PULL_NUMBER}/files"
      # FIXME do something if curl fails
      fi
  )"

  [[ $extrafiles ]] || unset check_files

  < <( <<<"${PR_FILES_JSON}" jq '.[]|.patch' ) spellintian $picky \
    | grep -q . || return
  readarray -t -O "${#check_files[@]}" check_files < <(
    jq -r '.[]|.filename' <<<"$PR_FILES_JSON"
  )
}

Canary=alive

main "${@}"

