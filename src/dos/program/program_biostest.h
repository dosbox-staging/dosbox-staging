// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_BIOSTEST_H
#define DOSBOX_PROGRAM_BIOSTEST_H

#include "dosbox.h"
#include "dos/programs.h"

class BIOSTEST final : public Program {
    public:
	    BIOSTEST()
	    {
		    help_detail = {HELP_Filter::All,
		                   HELP_Category::Misc,
		                   HELP_CmdType::Program,
		                   "BIOSTEST"};
	    }
	void Run(void) override;
};

#endif // DOSBOX_PROGRAM_BIOSTEST_H
