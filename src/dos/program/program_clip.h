// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_CLIP_H
#define DOSBOX_PROGRAM_CLIP_H

#include "dos/programs.h"

class CLIP final : public Program {
public:
	CLIP()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "CLIP"};
	}
	void Run() override;

private:
	static void AddMessages();
};

#endif
