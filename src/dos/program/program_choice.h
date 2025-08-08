// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_CHOICE_H
#define DOSBOX_PROGRAM_CHOICE_H

#include "programs.h"

#include <string>

class CHOICE final : public Program {
public:
	CHOICE()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::Batch,
		               HELP_CmdType::Program,
		               "CHOICE"};
	}
	void Run() override;
};

#endif
