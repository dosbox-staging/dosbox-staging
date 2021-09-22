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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_placeholder.h"

#include "shell.h"

extern unsigned int result_errorcode;

void PLACEHOLDER::Run()
{
	const auto command = cmd->GetFileName();

	LOG_WARNING("%s: %s", command, MSG_Get("PROGRAM_PLACEHOLDER_SHORT_HELP"));
	LOG_WARNING("%s: %s", command, MSG_Get("VISIT_FOR_MORE_HELP"));
	LOG_WARNING("%s: %s/%s", command, MSG_Get("WIKI_URL"), "Add-Utilities");

	WriteOut(MSG_Get("PROGRAM_PLACEHOLDER_LONG_HELP"), command);
	WriteOut_NoParsing(MSG_Get("UTILITY_DRIVE_EXAMPLE_NO_TRANSLATE"));

	first_shell->CMD_PAUSE(nullptr);
	result_errorcode = dos.return_code;
}

void PLACEHOLDER_ProgramStart(Program **make)
{
	*make = new PLACEHOLDER;
}
