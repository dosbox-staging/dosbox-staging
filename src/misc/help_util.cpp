#include "dosbox.h"

#include <map>
#include <string>

#include "help_util.h"
#include "string_utils.h"
#include "support.h"

static std::map<const std::string, HELP_Detail> help_list = {};

void HELP_AddToHelpList(const std::string &cmd_name, const HELP_Detail &detail,
                        bool replace_existing)
{
	if (replace_existing || !contains(help_list, cmd_name)) {
		help_list[cmd_name] = detail;
	}
}

const std::map<const std::string, HELP_Detail> &HELP_GetHelpList()
{
	return help_list;
}

std::string HELP_GetShortHelp(const std::string &cmd_name)
{
	const std::string short_key = "SHELL_CMD_" + cmd_name + "_HELP";
	if (MSG_Exists(short_key.c_str())) {
		return MSG_Get(short_key.c_str());
	}
	const std::string long_key = "SHELL_CMD_" + cmd_name + "_HELP_LONG";
	if (MSG_Exists(long_key.c_str())) {
		const std::string str(MSG_Get(long_key.c_str()));
		const auto pos = str.find('\n');
		return str.substr(0, pos != std::string::npos ? pos + 1 : pos);
	}
	return "No help available\n";
}

const char *HELP_CategoryHeading(const HELP_Category category)
{
	switch (category) {
	case HELP_Category::Dosbox: return "DOSBox Commands";
	case HELP_Category::File: return "File/Directory Commands";
	case HELP_Category::Batch: return "Batch File Commands";
	case HELP_Category::Misc: return "Miscellaneous Commands";
	default: return "Unknown Commands";
	}
}
