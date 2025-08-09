// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_INTRO_H
#define DOSBOX_PROGRAM_INTRO_H

#include "dos/programs.h"

class INTRO final : public Program {
public:
	INTRO()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "INTRO"};
	}
	void DisplayMount(void);
	void Run(void) override;

private:
        static void AddMessages();
        void WriteOutProgramIntroSpecial();
};

#endif // DOSBOX_PROGRAM_INTRO_H
