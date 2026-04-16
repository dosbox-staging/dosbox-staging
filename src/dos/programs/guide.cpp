// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "guide.h"

#include <filesystem>

#include "misc/support.h"
#include "more_output.h"
#include "utils/checks.h"

#include <SDL3/SDL.h>

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

	const auto path = get_resource_path("docs/getting-started/introduction.html");

	if (std::filesystem::exists(path)) {
		const auto url = std::string{"file://"} + path.string();
		SDL_OpenURL(url.c_str());

	} else {
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
	        "  - This will open a local offline copy of the Getting Started guide bundled\n"
	        "    with your DOSBox Staging installation; you don't need an internet connection\n"
	        "    to read the bundled guide.\n"
	        "\n"
	        "  - The offline documentation is located in the 'docs' subfolder of your\n"
	        "    DOSBox Staging 'resources' folder.\n"
	        "\n"
	        "  - If the offline documentation cannot be found, the command will open the\n"
	        "    guide on the project website (requires internet connection).");
}
