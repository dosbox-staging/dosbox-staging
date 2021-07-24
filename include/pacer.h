/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  Kirk Klobe <kklobe@gmail.com>
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
*/

class Pacer {
public:
	Pacer(const std::string &name, const int timeout);
	Pacer() = delete;

	bool CanRun();
	void Checkpoint();
	void SetTimeout(const int timeout);

private:
	const std::string pacer_name{};
	int64_t iteration_start = 0;
	int skip_timeout = 0;
	bool can_run = true;
};

#endif
