// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_RESCAN_H
#define DOSBOX_PROGRAM_RESCAN_H

#include "programs.h"

class RESCAN final : public Program {
public:
	RESCAN()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "RESCAN"};
	}
	void Run(void) override;
private:
	static void AddMessages();
};

#endif // DOSBOX_PROGRAM_RESCAN_H
