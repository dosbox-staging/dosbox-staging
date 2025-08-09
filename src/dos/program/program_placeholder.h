// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_PLACEHOLDER_H
#define DOSBOX_PROGRAM_PLACEHOLDER_H

#include "dos/programs.h"

class PLACEHOLDER final : public Program {
public:
	PLACEHOLDER()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "PLACEHOLDER"};
	}
	void Run() override;
private:
	static void AddMessages();
};

#endif
