/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2022  The DOSBox Staging Team
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

extern unsigned int result_errorcode;

#include "program_more_output.h"

void PLACEHOLDER::Run()
{
	const auto command = cmd->GetFileName();

	LOG_WARNING("%s: %s", command, MSG_Get("PROGRAM_PLACEHOLDER_HELP"));
	LOG_WARNING("%s: %s", command, MSG_Get("VISIT_FOR_MORE_HELP"));
	LOG_WARNING("%s: %s/%s", command, MSG_Get("WIKI_URL"), "Add-Utilities");

	MoreOutputStrings output(*this);
	output.AddString(MSG_Get("PROGRAM_PLACEHOLDER_HELP_LONG"), command);
	output.Display();

	WriteOut_NoParsing(MSG_Get("UTILITY_DRIVE_EXAMPLE_NO_TRANSLATE"));

	result_errorcode = dos.return_code;
}

void PLACEHOLDER::AddMessages() {
	MSG_Add("PROGRAM_PLACEHOLDER_HELP", "This program is a placeholder");
	MSG_Add("PROGRAM_PLACEHOLDER_HELP_LONG",
	        "%s is only a placeholder.\n"
	        "\nInstall a 3rd-party and give its PATH precedence.\n"
	        "\nFor example:");
			
	MSG_Add("UTILITY_DRIVE_EXAMPLE_NO_TRANSLATE",
	        "\n   [autoexec]\n"
#if defined(WIN32)
	        "   mount u C:\\Users\\username\\dos\\utils\n"
#else
	        "   mount u ~/dos/utils\n"
#endif
	        "   set PATH=u:\\;%PATH%\n\n");
	
	MSG_Add("VISIT_FOR_MORE_HELP", "Visit the following for more help:");
}
