// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MOVE_H
#define DOSBOX_PROGRAM_MOVE_H

#include "dos/programs.h"

#include <string>

class MOVE final : public Program {
public:
	MOVE()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "MOVE"};
	}
	void Run() override;
};

#endif
