// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xcopy.h"

#include "misc/support.h"
#include "more_output.h"

extern unsigned int result_errorcode;

void XCOPY::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_XCOPY_HELP_LONG"));
		output.Display();
		return;
	}

	WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PARAM_NUM"));
	WriteOut(MSG_Get("PROGRAM_XCOPY_FILES_COPIED"), 0);
	result_errorcode = enum_val(XcopyExitCode::InitializationError);
}

void XCOPY::AddMessages()
{
	MSG_Add("PROGRAM_XCOPY_INVALID_PARAM_NUM", "Invalid number of parameters\n");

	MSG_Add("PROGRAM_XCOPY_FILES_COPIED", "%9d File(s) copied\n");

	MSG_Add("PROGRAM_XCOPY_HELP", "Copy files and directory trees.\n");

	MSG_Add("PROGRAM_XCOPY_HELP_LONG",
	        "Copies files (except hidden and system files) and directory trees.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]xcopy[reset] [color=white]SOURCE[reset] [[color=light-cyan]DESTINATION[reset]] [/a|/m] [/d:[color=light-cyan]DATE[reset]] [/p] [/s|/e] [/v] [/w] [/y|/-y]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]SOURCE[reset]       the file or files to copy\n"
	        "  [color=light-cyan]DESTINATION[reset]  the location and/or name of the new files; current working \n"
	        "               directory if omitted\n"
	        "  /a           copy only files with the archive attribute set\n"
	        "  /m           same as /a, but clear the archive attribute in the source files\n"
	        "  /d:[color=light-cyan]DATE[reset]      copy only files changed on or after [color=light-cyan]DATE[reset]\n"
	        "  /p           prompt before creating each destination file\n"
	        "  /s           copy entire source tree, except empty directories\n"
	        "  /e           copy entire source tree, including empty directories\n"
	        "  /v           verify each new file as it is written\n"
	        "  /w           prompt to press a key before copying starts\n"
	        "  /y           overwrite existing destination files without prompting for\n"
	        "               confirmation\n"
	        "  /-y          prompt for confirmation before overwriting an existing\n"
	        "               destination file\n"
	        "\n"
	        "Notes:\n"
	        "  - The COPYCMD environment variable can be preset to /y or /-y. Either can\n"
	        "    be overridden by giving the opposite switch on the command line.\n"
	        "\n"
	        "  - [color=light-cyan]DATE[reset] takes a two- or four-digit year, with the day and month in the\n"
	        "    order your country setting uses. Two-digit years 80 to 99 mean 1980 to\n"
	        "    1999 and 00 to 79 mean 2000 to 2079; allowed four-digit years are in the\n"
	        "    range from 1980 to 2099.\n"
	        "\n"
	        "  - Copies are always created with the archive attribute set. The read-only,\n"
	        "    hidden and system attributes are not carried over.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]xcopy[reset] [color=white]a:[reset] [color=light-cyan]b:[reset] /e\n"
	        "  [color=light-green]xcopy[reset] [color=white]..\\*.*[reset] /d:[color=light-cyan]01-01-93[reset]");
}