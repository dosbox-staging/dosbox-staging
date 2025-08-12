// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_SERIAL_H
#define DOSBOX_PROGRAM_SERIAL_H

#include "dosbox.h"

#include "dos/programs.h"

class SERIAL final : public Program {
public:
	SERIAL()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "SERIAL"};
	}
	void Run() override;

private:
	static void AddMessages();
	void showPort(int port);
};

#endif // DOSBOX_PROGRAM_SERIAL_H
