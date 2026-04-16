// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "manual.h"

#include <filesystem>

#include "misc/support.h"
#include "more_output.h"
#include "utils/checks.h"

#include <SDL_misc.h>

CHECK_NARROWING();

void MANUAL::Run(void)
{
	// Print usage
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MANUAL_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("PROGRAM_MANUAL_HELP_LONG"));
		output.Display();
		return;
	}

	const auto path = get_resource_path("docs/manual/about-this-manual.html");

	if (std::filesystem::exists(path)) {
		const auto url = std::string{"file://"} + path.string();
		SDL_OpenURL(url.c_str());

	} else {
		SDL_OpenURL("https://www.dosbox-staging.org/manual/");
	}
}

void MANUAL::AddMessages()
{
	MSG_Add("PROGRAM_MANUAL_HELP",
	        "Open the DOSBox Staging user manual in the default browser.\n");

	MSG_Add("PROGRAM_MANUAL_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]manual[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  - This will open a local offline copy of the user manual guide bundled with\n"
	        "    your DOSBox Staging installation; you don't need an internet connection to\n"
	        "    read the bundled manual.\n"
	        "\n"
	        "  - The offline documentation is located in the 'docs' subfolder of your\n"
	        "    DOSBox Staging 'resources' folder.\n"
	        "\n"
	        "  - If the offline documentation cannot be found, the command will open the\n"
	        "    guide on the project website (requires internet connection).");
}
