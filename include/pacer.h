// SPDX-FileCopyrightText:  2020-2021  Kirk Klobe <kklobe@gmail.com>
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PACER_H
#define DOSBOX_PACER_H

#include "dosbox.h"

#include <string>

#include "timer.h"

/*
Pacer Class
~~~~~~~~~~~
Pacer allows a task to run provided it completes within a specified timeout. If
the task takes longer than the permitted time, then it skips its next turn to
run.

Usage:
 1. Construct using the task name and a timeout (microseconds) within which the
task should run. For example: Pacer render_pacer("Render", 1000);
 2. Check if the task can be run using CanRun(), which returns a bool.
 3. Immediately after the task ran (or didn't), Checkpoint() the results to
    prepare for the next pass.

Use the Reset() call after performing tasks that shouldn't be counted againsts
the pacer's timing. This is especially important for tasks that are long running
or depend on host behavior, such as changing a video mode or altering the
SDL window.
*/

class Pacer {
public:
	enum class LogLevel {
		NOTHING,
		CHECKPOINTS,
		TIMEOUTS,
	};
	Pacer(const std::string &name, const int timeout, const LogLevel level);
	Pacer() = delete;

	bool CanRun();
	void Checkpoint();
	void SetLogLevel(const LogLevel level);
	void SetTimeout(const int timeout);
	void Reset();

private:
	const std::string pacer_name{};
	int64_t iteration_start = 0;
	LogLevel log_level = LogLevel::NOTHING;
	int skip_timeout = 0;
	bool can_run = true;
	bool was_reset = false;
};

#endif
