// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
