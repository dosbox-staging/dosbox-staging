/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2020-2021  Kirk Klobe <kklobe@gmail.com>
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

#include "pacer.h"

Pacer::Pacer(const std::string &name, const int timeout)
        : pacer_name(name),
          iteration_start(GetTicksUs())
{
	assert(pacer_name.size() > 0);
	SetTimeout(timeout);
}

bool Pacer::CanRun()
{
	if (can_run)
		iteration_start = GetTicksUs();
	return can_run;
}

static constexpr bool log_checkpoints = false;

void Pacer::Checkpoint()
{
	if (can_run) {
		const auto iteration_took = GetTicksUsSince(iteration_start);
		can_run = iteration_took < skip_timeout;

		if (log_checkpoints)
			LOG_MSG("%s took %5dus, can_run = %s", pacer_name.c_str(),
			        iteration_took, can_run ? "true" : "false");
	} else {
		can_run = true;
	}
}

void Pacer::SetTimeout(const int timeout)
{
	assert(timeout > 0);
	skip_timeout = timeout;
}
