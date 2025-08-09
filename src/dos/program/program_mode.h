// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MODE_H
#define DOSBOX_PROGRAM_MODE_H

#include "dos/dos_inc.h"
#include "programs.h"

#include <string>


enum class FontWidth;

class MODE final : public Program {
public:
	MODE()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "MODE"};
	}
	void Run() override;

private:
	void HandleSetDisplayMode(const std::string& command);
	bool HandleSetTypematicRate();

	static void AddMessages();
};

#endif
