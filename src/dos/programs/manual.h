// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MANUAL_H
#define DOSBOX_PROGRAM_MANUAL_H

#include <optional>

#include "dos/programs.h"
#include "misc/help_util.h"

class MANUAL final : public Program {
public:
	MANUAL()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "MANUAL"};
	}

	void Run(void) override;

private:
	static void AddMessages();
};

#endif // DOSBOX_PROGRAM_MANUAL_H
