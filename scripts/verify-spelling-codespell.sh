#!/bin/bash

# Copyright (C) 2020  Feignint <feignint@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Utilises codespell https://github.com/codespell-project/codespell
# sudo apt-get install codespell

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
codepell_version ()
{
  >/dev/null type -p codespell ||
    {
      >&2 printf "%s\n" "codespell command not found"
      exit 127 # same as bash: command not found
    }
  >&2 printf "codespell version: %s\n" "$( codespell --version )"
}
main ()
{
  is_pull
  codepell_version
  fetch_files_json
  _github_PR
}
fetch_files_json ()
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
    # splits the patch string into patch_hunks array
    # i.e. each array element is a hunk
    jq '
        .[]|select(.patch != null)
           | {
               "filename"    : .filename,
               "patch_hunks" : [
                  .patch
                  | split("\n@@")|.[]
                  | sub("^ -";"@@ -")
               ]
             }
       ' <<<"${PR_FILES_JSON}"
  )"
}
hunk_ranges ()
{
  jq -r '
          select(.filename == "'"${1}"'")
            |.patch_hunks[]|match("\\+[0-9]+,[0-9]+")
            |.string|sub("\\+";"")
        ' <<<"${patch_hunks_json}"
}
_github_PR ()
{
  readarray -t files_changed < <( jq -r '.filename' <<<"$patch_hunks_json" )

  while IFS=":" read -r FN LN TYPO_FIX
  do
    # NOTE: TYPO_FIX has a leading space, not currently a problem.
    while IFS="," read -r S R
    do
      (( LN >= S && LN <= S + R )) &&
        printf "::%s file=%s,line=%d::%s:%s\n" \
               "${errlevels[$errlevel]}" "$FN" "$LN" "$FN" "$TYPO_FIX"
    done < <( hunk_ranges "${FN}" )
  done < <( codespell "${files_changed[@]}" )
}

# Annotations
errlevels=( debug warning error )
errlevel=1 # default, warning

main "${@}"

#TODO check binary
#TODO Pre push check, i.e. get files changed from diff, default to master ( parse .git/config for remote )
#     but let user pass any tree-ish, also --staged
#TODO Ignore list
