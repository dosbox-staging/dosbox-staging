#!env /bin/bash

# SPDX-FileCopyrightText:  2024-2024 The DOSBox Staging Team
# SPDX-License-Identifier: MIT

VCPKG=$VCPKG_ROOT/vcpkg

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <old-baseline-hash> <new-baseline-hash>"
  exit 1
fi

OLD="$1"
NEW="$2"

PATTERNS=$(jq -r '.dependencies[] | if type=="string" then . else .name end' vcpkg.json | sed 's/.*/^[[:space:]]*-[[:space:]]*&([[:space:]]|$)/')

$VCPKG portsdiff "$OLD" "$NEW" | grep -Ef <(echo "$PATTERNS")
