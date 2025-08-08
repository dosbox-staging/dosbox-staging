// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MORE_H
#define DOSBOX_PROGRAM_MORE_H

#include "programs.h"

#include "program_more_output.h"

class MORE final : public Program {
public:
	MORE()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "MORE"};
	}
	void Run() override;

private:
	bool ParseCommandLine(MoreOutputFiles &output);
	bool FindInputFiles(MoreOutputFiles& output);

	static void AddMessages();
};

#endif
