// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_HELP_H
#define DOSBOX_PROGRAM_HELP_H

#include "dos/programs.h"

#include <string>

class HELP final : public Program {
public:
	HELP()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "HELP"};
	}
	void Run() override;
};

#endif
