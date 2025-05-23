#!/bin/bash

CWD=$(dirname "$0")
COMMAND=$(basename "$0")

open -a DOSBox\ Staging --args --working-dir "$CWD"

(sleep 0.1 ; osascript <<END
tell application "Terminal"
	close (every window whose name contains "$COMMAND")
	if number of windows = 0 then quit
end tell
END
) &

