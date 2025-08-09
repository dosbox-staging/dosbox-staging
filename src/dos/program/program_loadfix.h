// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_LOADFIX_H
#define DOSBOX_PROGRAM_LOADFIX_H

#include "dos/programs.h"

class LOADFIX final : public Program {
public:
	LOADFIX()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "LOADFIX"};
	}
	void Run(void) override;
private:
    static void AddMessages();
};

#endif // DOSBOX_PROGRAM_LOADFIX_H
