// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MODE_H
#define DOSBOX_PROGRAM_MODE_H

#include "dos/dos.h"
#include "dos/programs.h"

#include <string>

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
	bool HandleSetTypematicRate();

	void HandleSetDisplayMode();
	void SetDisplayMode(const std::string& mode_str);

	static void AddMessages();
};

#endif
