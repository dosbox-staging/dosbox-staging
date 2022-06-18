#!/bin/sh

set -e

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2022  kcgen <kcgen@users.noreply.github.com>

usage()
{
    printf "%s\n" "\
    Usage: ATTEMPTS COMMAND [[ARG1] ARG2 ...]
    Where:
        ATTEMPTS  : Retry the COMMAND up to this many times.
        COMMAND   : The executable or command to run.
        ARG(s)    : One or more arguments for the COMMAND.

    Note: Place COMMAND or ARG(s) inside single or double
          quotes if they contain spaces.
    "
}

# Setup the error handler
error_code=1
fail() {
  if [ "$error_code" -ne 0 ]; then
    echo "$1" >&2
  fi
  exit "$error_code"
}

# Check number of arguments
if [ "$#" -lt 2 ]; then
  usage
  fail "Please provide both the number of ATTEMPTS and COMMAND to run."
fi

# Setup our current and total number of attempts
attempt=1
attempts="$1"
shift
case "$attempts" in
    ''|*[!0-9]*) usage; fail "$attempts is invalid, please provide a number 1 or greater"  ;;
esac
if [ "$attempts" -lt 1 ]; then
  usage
  fail "$attempts cannot be a negative number, please provide a number 1 or greater"
fi

while true; do
  # reset the error code
  error_code=0

  # Launch and capture the error code on failure
  $* || error_code=$?

  # Quit if it passed
  if [ "$error_code" -eq 0 ]; then
    break
  fi

  # Inform the user
  echo -n "\"$*\" quit with error-code $error_code on attempt $attempt of $attempts, " >&2

  # Maybe loop again or bail out
  if [ "$attempt" -lt "$attempts" ]; then
    attempt=$((attempt+1))
    sleep 1
    echo "retrying ...\n" >&2
  else
    fail "quitting.\n"
  fi
done
