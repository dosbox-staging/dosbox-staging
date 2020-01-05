#!/bin/bash
set -euo pipefail
id="${1}"
dir="${2}"
if [[ ! -d "${dir}" ]]; then
	gdown -q --id "${id}" -O - | tar -I zstd -x
fi
