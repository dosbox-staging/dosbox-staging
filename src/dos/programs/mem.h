// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MEM_H
#define DOSBOX_PROGRAM_MEM_H

#include "dos/programs.h"

class MEM final : public Program {
public:
	MEM()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "MEM"};
	}
	void Run(void) override;
private:
        static void AddMessages();
};

#endif // DOSBOX_PROGRAM_MEM_H
