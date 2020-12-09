#!/bin/bash

# Copyright (C) 2020  Feignint <feignint@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

# files to check by default
# Note: if src/dosbox exists it will also be checked
check_files=( "*.ac" "*.md" "*.MD" "README*" "dosbox.1" "INSTALL" )

# The following patterns should be ignored
  filter="README: esc esc"
filter+="|README: tcl -> Tcl"
filter+="|stb_vorbis.h: finalY"
filter+="|duplicate word"
#TODO improve filter maintenance

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
        check_files+=( "*.h" "*.cpp" "*.c" )
        # mostly seems to pick-up typos in comments
        # important typos will be picked up in the src/dosbox
        # no real need for this in github_CI
        ;;
      --all)
        check_files+=( "." )
        # will check everything
        # well, nearly everything,
        # contrib/translations and .gitignore have been 'hardcode' excluded
        ;;
      --dosbox|--binary)
        skip_list_files="true"
        ;;
      *)
        : # do nothing, for now
        ;;
    esac
  done

  repo_root="$( git -C "${0%/*}" rev-parse --show-toplevel )"

  readarray -t -O 0 file_list < <( list_files )

  # If no files to check, --dosbox was passed and either compile failed or
  # this script executed before compiling, exit with error code.
  [[ ${#file_list[@]} == 0 ]] && exit 42

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
  git -C "${repo_root}" grep -h -n "${TYPO_FIX% ->*}" \
      $( [[ ${MIMEtype#*: }  =~ ${BinaryMIMEtypes}$ ]] ||
           printf "%s" "-- ${FILENAME%:}" )
}

_github_output ()
{
  echo "::group::Spellintian"
  while read -r FILENAME TYPO_FIX
  do
    FILENAME="${FILENAME#${repo_root}/}"
    TYPO_FIX="${TYPO_FIX//\"}"

    [[ "${FILENAME} ${TYPO_FIX}" =~ (${filter}) ]] && continue

    [[ ${FILENAME%:} == src/dosbox ]] &&
      warning=error || warning=warning

    while IFS=: read -r LN LINE
    do
      # get column, bash indexes from 0, prefix with _ for a cheap +1
      COL="_${LINE%%${TYPO_FIX% ->*}*}"
      # WIP This Creates Annotations
      echo "::$warning file=${FILENAME%:},line=${LN},col=${#COL}::${TYPO_FIX}"
      # FIXME I think I have to do some magic to get the correct line number
      # It appears to work fine if the file is not touched by commit
      # so I think the line number needs to be relative to the diff
      ###
      # I think this, a plain old echo will, I hope, show up in the log
      # thing that never expands
      echo "$warning ${TYPO_FIX} file=${FILENAME%:} line=${LN} col=${#COL}"

      if [[ ${warning} == error ]]
      then
        Canary=dead
      fi
    done < <( Get_context )
  done < <( check_spelling )
  echo "::endgroup::"
}


Canary=alive

main "${@}"

[[ $Canary == alive ]]
