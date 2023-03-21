#!/bin/bash

CWD=$(dirname "$0")
COMMAND=$(basename "$0")

NUM_COLUMNS=120
NUM_ROWS=45
FONT_SIZE=14

quit_terminal() {
  kill $(jobs -p)
  (sleep 0.1 ; osascript <<END
tell application "Terminal"
  close (every window whose name contains "$COMMAND")
  if number of windows = 0 then quit
end tell
END
  ) &
}

trap quit_terminal EXIT

osascript <<END
tell application "Terminal"
  set number of columns of (every window whose name contains "$COMMAND") to $NUM_COLUMNS
  set number of rows    of (every window whose name contains "$COMMAND") to $NUM_ROWS
  set font size         of (every window whose name contains "$COMMAND") to $FONT_SIZE
  set background color  of (every window whose name contains "$COMMAND") to {5000,5000,5000,0}
  set normal text color of (every window whose name contains "$COMMAND") to {40000,40000,40000,0}
end tell
END

/Applications/DOSBox\ Staging.app/Contents/MacOS/dosbox --working-dir "$CWD"

