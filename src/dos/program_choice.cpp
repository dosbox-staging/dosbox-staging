/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#include "program_choice.h"

#include <string>

#include "shell.h"
#include "string_utils.h"

extern unsigned int result_errorcode;

void CHOICE::Run()
{
	std::string tmp = "";
	cmd->GetStringRemain(tmp);
	char args[CMD_MAXLINE];
	safe_strcpy(args, tmp.c_str());
	first_shell->CMD_CHOICE(args);
	result_errorcode = dos.return_code;
}

void CHOICE_ProgramStart(Program **make)
{
	*make = new CHOICE;
}
