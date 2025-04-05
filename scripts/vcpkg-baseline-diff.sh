#!/bin/bash
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <old-baseline> <new-baseline>"
  exit 1
fi
old="$1"
new="$2"
patterns=$(jq -r '.dependencies[] | if type=="string" then . else .name end' vcpkg.json | sed 's/.*/^[[:space:]]*-[[:space:]]*&([[:space:]]|$)/')
vcpkg portsdiff "$old" "$new" | grep -Ef <(echo "$patterns")
