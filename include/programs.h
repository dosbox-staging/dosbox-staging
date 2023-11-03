/*
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_PROGRAMS_H
#define DOSBOX_PROGRAMS_H

#include "dosbox.h"

#include "std_filesystem.h"
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>

#include "dos_inc.h"
#include "help_util.h"

#define WIKI_URL "https://github.com/dosbox-staging/dosbox-staging/wiki"

#define WIKI_ADD_UTILITIES_ARTICLE WIKI_URL "/Add-Utilities"

class CommandLine {
public:
	CommandLine(int argc, const char* const argv[]);
	CommandLine(std::string_view name, std::string_view cmdline);

	std::string GetFileName() const
	{
		return file_name;
	}

	// Checks if any of the supplier arguments (like "/?) exists on the
	// command line argument list.
	bool FindExist(const std::string& arg);
	template <typename... T>
	bool FindExist(const std::string& first, const T... others)
	{
		return FindExist(first) || FindExist(others...);
	}

	// Checks if any of the supplier arguments (like "/?) exists on the
	// command line argument list.
	// All the matching arguments are removed from the list.
	bool FindRemoveExist(const std::string& arg);
	template <typename... T>
	bool FindRemoveExist(const std::string& first, const T... others)
	{
		// Make sure both are called
		const auto result1 = FindRemoveExist(first);
		const auto result2 = FindRemoveExist(others...);
		return result1 || result2;
	}

	// Checks if any of the supplier arguments (like "-n") exists on the
	// command line argument list, and that the next argument is a number.
	// Number is parsed and returned.
	bool FindInt(const std::string& arg, int& value);
	// As above, but removes both arguments.
	bool FindRemoveInt(const std::string& arg, int& value);

	// Checks if any of the supplier arguments (like "-n") exists on the
	// command line argument list, and that the next argument exists.
	// The next argument is returned as a string.
	bool FindString(const std::string& arg, std::string& value);
	// As above, but removes both arguments.
	bool FindRemoveString(const std::string& arg, std::string& value);

	bool FindCommand(const size_t, std::string& value) const;

	bool FindStringBegin(const std::string& arg, std::string& value);
	bool FindRemoveStringBegin(const std::string& arg, std::string& value);

	bool FindStringRemain(const std::string& arg, std::string& value);

	// Only used for parsing 'COMMAND.COM /C', allows '/C dir' and '/Cdir'.
	// Restores quotes back into the commands so than commands like
	// '/C mount d "/tmp/a b"'' work as intended.
	bool FindStringRemainBegin(const std::string& arg, std::string& value);

	bool GetStringRemain(std::string& value);

	int GetParameterFromList(const char* const params[],
	                         std::vector<std::string>& output);

	std::vector<std::string> GetArguments();

	bool HasDirectory() const;
	bool HasExecutableName() const;

	size_t GetCount() const;

	bool ExistsPriorTo(const std::list<std::string_view>& pre_args,
	                   const std::list<std::string_view>& post_args) const;

	void Shift(const size_t amount = 1);
	uint16_t Get_arglength();

	bool FindBoolArgument(const std::string& arg, const char short_letter = 0);
	bool FindRemoveBoolArgument(const std::string& name,
	                            const char short_letter = 0);

	std::string FindRemoveStringArgument(const std::string& name);

	std::vector<std::string> FindRemoveVectorArgument(const std::string& name);

	std::optional<std::vector<std::string>> FindRemoveOptionalArgument(
	        const std::string& arg);

	std::optional<int> FindRemoveIntArgument(const std::string& name);

private:
	using cmd_it = std::list<std::string>::iterator;

	std::list<std::string> cmds = {};
	std::string file_name       = "";

	bool FindEntry(const std::string& arg, cmd_it& it,
	               const bool needs_next = false);

	std::string FindRemoveSingleString(const std::string& arg);
};

class Program {
public:
	Program();

	Program(const Program&)            = delete; // prevent copy
	Program& operator=(const Program&) = delete; // prevent assignment

	virtual ~Program()
	{
		delete cmd;
		delete psp;
	}

	std::string temp_line = "";
	CommandLine* cmd      = nullptr;
	DOS_PSP* psp          = nullptr;

	virtual void Run(void) = 0;
	virtual void WriteOut(const char* format, const char* arguments);

	// printf to DOS stdout
	virtual void WriteOut(const char* format, ...);

	// Write string to DOS stdout
	void WriteOut_NoParsing(const char* str);

	// Prevent writing to DOS stdout
	bool SuppressWriteOut(const char* format);

	void InjectMissingNewline();
	void ChangeToLongCmd();
	bool HelpRequested();
	bool CheckAllSwitchesHandled(); // also prints DOS error

	static void ResetLastWrittenChar(char c);

	void AddToHelpList();

protected:
	HELP_Detail help_detail{};
};

using PROGRAMS_Creator = std::function<std::unique_ptr<Program>()>;

void PROGRAMS_Destroy([[maybe_unused]] Section* sec);
void PROGRAMS_MakeFile(const char* const name, PROGRAMS_Creator creator);

template <class P>
std::unique_ptr<Program> ProgramCreate()
{
	// Ensure that P is derived from Program
	static_assert(std::is_base_of_v<Program, P>, "class not derived from Program");
	return std::make_unique<P>();
}

#endif
