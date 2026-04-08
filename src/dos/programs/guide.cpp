// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "guide.h"

#include "misc/support.h"
#include "more_output.h"
#include "utils/checks.h"

#include <SDL_misc.h>

CHECK_NARROWING();

void GUIDE::Run(void)
{
	// Print usage
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_GUIDE_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("PROGRAM_GUIDE_HELP_LONG"));
		output.Display();
		return;
	}

	const auto url =
	        std::string{"file://"} +
	        get_resource_path("docs/getting-started/introduction.html").string();

	if (SDL_OpenURL(url.c_str()) == -1) {
		SDL_OpenURL("https://www.dosbox-staging.org/getting-started/");
	}
}

void GUIDE::AddMessages()
{
	MSG_Add("PROGRAM_GUIDE_HELP",
	        "Open the DOSBox Staging Getting Started guide in the default browser.\n");

	MSG_Add("PROGRAM_GUIDE_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]guide[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  - This will open an offline copy of the Getting Started guide bundled with\n"
	        "    your DOSBox Staging installation; you don't need an internet connection to\n"
	        "    read the guide.\n");
}
