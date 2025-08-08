// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_KEYB_H
#define DOSBOX_PROGRAM_KEYB_H

#include "programs.h"

#include "dos/dos_keyboard_layout.h"

class KEYB final : public Program {
public:
	KEYB()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "KEYB"};
	}

	void Run() override;

private:
	void ListKeyboardLayouts();

	void WriteOutFailure(const KeyboardLayoutResult error_code,
	                     const std::string& layout,
	                     const uint16_t requested_code_page,
	                     const uint16_t tried_code_page);
	void WriteOutSuccess();

	static void AddMessages();
};

#endif // DOSBOX_PROGRAM_KEYB_H
