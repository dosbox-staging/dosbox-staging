#!/bin/bash
set -euo pipefail

# Setup QuickView if needed
if [[ ! -d qv ]]; then
	(	# subshell to avoid keeping track of CWD
		cd ../common
		./setup-QuickView.sh
	)
fi

# Default to the repo's build if not provided
dosbox="${1:-../../../src/dosbox}"
echo "Using DOSBox binary: $dosbox"
echo ""

for batfile in *.bat; do
	echo "----------------------"
	echo "Testing:"

	conf="${batfile%.*}.conf"
	# Print the SDL configuration under test
	head -6 "$conf" | grep = | sed 's/^/  /'
	echo ""

	# Launch
	set -x
	"$dosbox" -userconf -conf dosbox.conf -conf "$conf" "$batfile"
	set +x
	echo ""
	echo ""
done
