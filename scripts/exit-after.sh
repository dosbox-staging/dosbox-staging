#!/bin/bash

# Copyright (c) 2019-2020 Kevin R Croft <krcroft@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Runs and stops a command after an amount of time.
#
# Args: RUNNING-AFTER RUNTIME GRACETIME LOGFILE NAME COMMAND [ARGS [...]]
#
# Termination is performed by strapping a debugger to the process and
# calling exit(0) from within the process.  This is necessary to
# trigger the flushing of instrumentation records, such as line coverage
# or profiling times, to disk.  If the process is exited using any
# other means (such as via SIGNAL), the records will not be flushed.
#
# Pre-requisite: GDB needs to be allowed to attach to processes:
# echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
#
# The RUNNING-AFTER value ("true" or "false") indicates if COMMAND
# is expected to running after RUNTIME seconds have elapsed.
#
# If RUNNING-AFTER is "true", then COMMAND is expected to still be
# running *after* RUNTIME seconds have elapsed, at which point this script
# stops the test and declares success.
# If the program unexpectedly quits on its own before RUNTIME have elapsed,
# then an error-code (1) is returned to the shell.
#
# If RUNNING-AFTER is "false", then COMMAND is expected to quit on its own
# before RUNTIME seconds have elapsed.  In this scenario, this script simply
# acts as a safety net to force the COMMAND to stop after RUNTIME seconds
# in the event it hasn't already quit.
# If the program is still running after RUNTIME, then an error-code (1) is
# returned to the shell.
#
set -euo pipefail

# Check arguments
if [[ "$#" -lt "6" ]]; then
	echo "Usage: $0 RUNNING-AFTER RUNTIME GRACETIME LOGFILE NAME COMMAND [ARGS [...]]"
	echo "Where:"
	echo "  RUNNING-AFTER (true or false) indicates if COMMAND is expected to"
	echo "  still be running after RUNTIME seconds."
	echo ""
	echo "  COMMAND is run for RUNTIME seconds before being forced to exit(0),"
	echo "  after  which it's signalled to QUIT and then KILLed after GRACETIME"
	echo "  seconds."
	echo ""
	echo "  Writes stdin and stdout to LOGFILE, can be - to send it to the terminal"
	echo ""
	echo "  Prints '  - running NAME '  followed by [passed] or [failed]"
	echo ""
	echo "  ARGS will be passed to COMMAND, if any are provided."
	exit 1
fi

# Check GDB permissions
if ! grep -q 0 /proc/sys/kernel/yama/ptrace_scope; then
	echo "Allow GDB to attach to processes without root:"
	echo "echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope"
	exit 1
fi

# Define a couple helper functions
function is_running() {
	ps -p "${1}" &> /dev/null
}

function gdb_exit() {
	rcode="none"
	if is_running "${prog_pid}"; then
		# shellcheck disable=SC2016
		if gdb -p "${prog_pid}" -ex 'call exit(0)' -ex 'print $_exitcode' -batch &> "${logfile}"; then
			rcode="${late_exit_code}"
		elif [[ -f "${logfile}" ]]; then
			cat "${logfile}"
			# gdb failed, so fall through to the kill steps instead
		fi
	else
		rcode="${early_exit_code}"
	fi
	# If we need to exit here, print result then exit
	if [[ "${rcode}" != "none" ]]; then
		[[ "${rcode}" == 0 ]] && echo "[passed]" || echo "[failed]"
		exit "${rcode}"
	fi
}

function send_signal() {
	if is_running "${prog_pid}"; then
		echo "Sending ${1} signal to PID ${prog_pid}"
		kill -s "${1}" "${prog_pid}" &> "${logfile}"
		sleep "${gracetime}"
	fi
}

# Process user arguments
late_exit_code="$([[ "${1}" == "true" ]] && echo 0 || echo 1)"
early_exit_code="$([[ "${1}" == "false" ]] && echo 0 || echo 1)"
runtime="${2}"
gracetime="${3}"
logfile="${4}"
name="${5}"
prog="${6}"
shift 6

if [[ "${logfile}" == "-" ]]; then
	logfile="/dev/stdout"
fi

# Start the program
echo -n "  - running $name "
"${prog}" "$@" &> "${logfile}" &
prog_pid="$!"

# Start the timer
sleep "${runtime}" &
timer_pid="$!"

# Wait for either our program to quit or the timer to expire ...
while is_running "${prog_pid}" && is_running "${timer_pid}"; do
	sleep 1
done

# Incrementally attempt to stop the process
gdb_exit # exits if successful
send_signal QUIT
send_signal KILL

# If we made it here then our program has become a zombie
echo "[failed]"
exit 1
