// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MOUSE_H
#define DOSBOX_PROGRAM_MOUSE_H

#include "programs.h"

class MOUSE final : public Program {
public:
	MOUSE()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "MOUSE"};
	}
	void Run() override;

private:
	static void AddMessages();
};

#endif
