// SPDX-FileCopyrightText: 2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMGMAKE_H
#define DOSBOX_IMGMAKE_H

#include "shell/shell.h"

class IMGMAKE final : public Program {
public:
	IMGMAKE()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "IMGMAKE"};
	}
	void Run() override;

	// Registers the messages used by the command with the message system
	static void AddMessages();
};

#endif // DOSBOX_IMGMAKE_H