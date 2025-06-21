// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef HELP_UTIL_H
#define HELP_UTIL_H

#include <map>
#include <string>

enum class HELP_Filter : uint8_t { All, Common };

enum class HELP_Category : uint8_t { Misc, File, Dosbox, Batch };

enum class HELP_CmdType : uint8_t { Shell, Program };

struct HELP_Detail {
	HELP_Filter filter = HELP_Filter::All;
	HELP_Category category = HELP_Category::Misc;
	HELP_CmdType type = HELP_CmdType::Shell;
	std::string name = {};
};

void HELP_AddToHelpList(const std::string &cmd_name, const HELP_Detail &detail,
                        bool replace_existing = false);
const std::map<const std::string, HELP_Detail> &HELP_GetHelpList();
std::string HELP_GetShortHelp(const std::string &cmd_name);
const char *HELP_CategoryHeading(const HELP_Category category);
void HELP_AddMessages();

#endif // HELP_UTIL_H
