// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_SUBST_H
#define DOSBOX_PROGRAM_SUBST_H

#include "programs.h"

#include <string>

class SUBST final : public Program {
public:
	SUBST()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "SUBST"};
	}
	void Run() override;
};

#endif
