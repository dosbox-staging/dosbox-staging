/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#include "control.h"
#include "logging.h"

// This global variable should be setup/torn down per test.
Config *control = nullptr;

// During testing we never want to log to stdout/stderr, as it could
// negatively affect test harness.
void GFX_ShowMsg(const char *, ...) {}
void DEBUG_ShowMsg(const char *, ...) {}

void DEBUG_HeavyWriteLogInstruction(){}
void MSG_Add(const char *, const char *){}
const char *MSG_Get(char const *)
{
	return nullptr;
}

#if C_DEBUG
void LOG::operator()(MAYBE_UNUSED char const *buf, ...)
{
	(void)d_type;     // Deliberately unused.
	(void)d_severity; // Deliberately unused.
}
#endif
