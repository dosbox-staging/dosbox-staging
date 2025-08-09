// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAMS_H
#define DOSBOX_PROGRAMS_H

#include "dosbox.h"

#include <memory>
#include <string>

#include "shell/command_line.h"
#include "misc/console.h"
#include "dos_inc.h"
#include "misc/help_util.h"

#define WIKI_URL "https://github.com/dosbox-staging/dosbox-staging/wiki"

#define WIKI_ADD_UTILITIES_ARTICLE WIKI_URL "/Adding-utilities"

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

	// printf to DOS stdout
	template <typename... Args>
	void WriteOut(const std::string& format, const Args&... args)
	{
		if (SuppressWriteOut(format)) {
			return;
		}

		const auto str = format_str(format, args...);
		CONSOLE_Write(str);
	}

	// TODO Only used by the unit tests, try to get rid of it later
	virtual void WriteOut(const std::string& format, const char* arguments);

	// Write string to DOS stdout
	void WriteOut_NoParsing(const std::string& str);

	// Prevent writing to DOS stdout
	bool SuppressWriteOut(const std::string& format) const;

	void InjectMissingNewline();
	void ChangeToLongCmd();
	bool HelpRequested();

	static void ResetLastWrittenChar(char c);

	void AddToHelpList();

protected:
	HELP_Detail help_detail{};
};

void PROGRAMS_AddMessages();
void PROGRAMS_Init(Section* sec);

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
