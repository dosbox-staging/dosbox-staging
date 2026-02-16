// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_COMMAND_LINE_H
#define DOSBOX_COMMAND_LINE_H

#include "dosbox.h"

#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class CommandLine {
public:
	CommandLine(int argc, const char* const argv[]);
	CommandLine(std::string_view name, std::string_view cmdline);

	const char* GetFileName() const
	{
		return file_name.c_str();
	}

	bool FindExist(const std::string& name, bool remove = false);

	bool FindExistCaseSensitive(const std::string& name, bool remove = false);

	bool FindExistRemoveAll(const std::string& name);

	// Removes all the supplied arguments from the list, returns 'true' if
	// at least one of them was found. Use it if argument has aliases, for
	// example if '/a' and '/all' request the same action.
	template <typename... Names>
	bool FindExistRemoveAll(const std::string& name, Names... names)
	{
		const auto found_1 = FindExistRemoveAll(name);
		const auto found_2 = FindExistRemoveAll(names...);
		return found_1 || found_2;
	}

	bool FindInt(const std::string& name, int& value, bool remove = false);

	bool FindString(const std::string& name, std::string& value,
	                bool remove = false);

	bool FindCommand(unsigned int which, std::string& value) const;

	bool FindStringBeginCaseSensitive(const std::string& begin,
	                                  std::string& value, bool remove = false);

	bool FindStringBegin(const std::string& begin, std::string& value,
	                     bool remove = false);

	bool FindStringRemain(const std::string& name, std::string& value);

	bool FindStringRemainBegin(const std::string& name, std::string& value);

	bool GetStringRemain(std::string& value);

	int GetParameterFromList(const char* const params[],
	                         std::vector<std::string>& output);

	std::vector<std::string> GetArguments();

	bool HasDirectory() const;
	bool HasExecutableName() const;

	unsigned int GetCount(void);

	bool ExistsPriorTo(const std::list<std::string_view>& pre_args,
	                   const std::list<std::string_view>& post_args) const;

	void Shift(unsigned int amount = 1);
	int GetNumArguments();

	bool FindRemoveBoolArgument(const std::string& name, char short_letter = 0);

	std::string FindRemoveStringArgument(const std::string& name);

	std::vector<std::string> FindRemoveVectorArgument(const std::string& name);

	std::optional<std::vector<std::string>> FindRemoveOptionalArgument(
	        const std::string& name);

	std::optional<int> FindRemoveIntArgument(const std::string& name);

private:
	using cmd_it = std::list<std::string>::iterator;

	std::list<std::string> cmds = {};
	std::string file_name       = "";

	bool FindEntry(const std::string& name, cmd_it& it, bool neednext = false, const bool case_sensitive = false);

	std::string FindRemoveSingleString(const char* name);

	bool FindBoolArgument(const std::string& name, bool remove,
	                      char short_letter = 0);
};

#endif // DOSBOX_COMMAND_LINE_H
