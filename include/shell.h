/*
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

#ifndef DOSBOX_SHELL_H
#define DOSBOX_SHELL_H

#include "dosbox.h"

#include <cctype>
#include <list>
#include <memory>
#include <string>

#ifndef DOSBOX_PROGRAMS_H
#include "programs.h"
#endif

#include "callback.h"
#include "help_util.h"

#define CMD_MAXLINE 4096
#define CMD_MAXCMDS 20
#define CMD_OLDSIZE 4096
extern callback_number_t call_shellstop;
class DOS_Shell;

/* first_shell is used to add and delete stuff from the shell env 
 * by "external" programs. (config) */
extern DOS_Shell * first_shell;


class BatchFile {
public:
	BatchFile(DOS_Shell * host,char const* const resolved_name,char const* const entered_name, char const * const cmd_line);
	BatchFile(const BatchFile&) = delete; // prevent copying
	BatchFile& operator=(const BatchFile&) = delete; // prevent assignment
	virtual ~BatchFile();
	virtual bool ReadLine(char * line);
	bool Goto(std::string_view label);
	void Shift();
	uint16_t file_handle = 0;
	uint32_t location = 0;
	bool echo = false;
	DOS_Shell *shell = nullptr;
	std::shared_ptr<BatchFile> prev = {}; // shared with Shell.bf
	std::unique_ptr<CommandLine> cmd = {};
	std::string filename{};

private:
	[[nodiscard]] std::string ExpandedBatchLine(std::string_view line) const;
	[[nodiscard]] std::string GetLine();
};

class AutoexecEditor;
class MoreOutputStrings;

struct SHELL_Cmd {
	void (DOS_Shell::*handler)(char *args) = nullptr; // Handler for this command
	const char *help = "";                       // String with command help
	HELP_Filter filter = HELP_Filter::Common;
	HELP_Category category = HELP_Category::Misc;
};

class DOS_Shell : public Program {
private:
	void PrintHelpForCommands(MoreOutputStrings &output, HELP_Filter req_filter);
	void AddShellCmdsToHelpList();
	bool WriteHelp(const std::string &command, char* args);

	friend class AutoexecEditor;
	std::list<std::string> l_history{};
	std::list<std::string> l_completion{};

	char *completion_start = nullptr;
	uint16_t completion_index = 0;
	bool exit_cmd_called = false;
	static inline bool help_list_populated = false;

public:

	DOS_Shell();
	~DOS_Shell() override;
	DOS_Shell(const DOS_Shell&) = delete; // prevent copy
	DOS_Shell& operator=(const DOS_Shell&) = delete; // prevent assignment
	void Run() override;
	void RunInternal(); // for command /C
	/* A load of subfunctions */
	void ParseLine(char * line);
	void GetRedirection(char *s, std::string &ifn, std::string &ofn, std::string &pipe, bool * append);
	void InputCommand(char * line);
	void ProcessCmdLineEnvVarStitution(char *line);
	void ShowPrompt();
	void DoCommand(char * cmd);
	bool Execute(char * name,char * args);
	/* Checks if it matches a hardware-property */
	bool CheckConfig(char* cmd_in,char*line);
	/* Internal utilities for testing */
	virtual bool execute_shell_cmd(char *name, char *arguments);

	/* Some internal used functions */
	const char *Which(const char *name) const;

	/* Commands */
	void CMD_HELP(char * args);
	void CMD_CLS(char * args);
	void CMD_COPY(char * args);
	void CMD_DATE(char * args);
	void CMD_TIME(char * args);
	void CMD_DIR(char * args);
	void CMD_DELETE(char * args);
	void CMD_ECHO(char * args);
	void CMD_EXIT(char * args);
	void CMD_MKDIR(char * args);
	void CMD_CHDIR(char * args);
	void CMD_RMDIR(char * args);
	void CMD_SET(char * args);
	void CMD_IF(char * args);
	void CMD_GOTO(char * args);
	void CMD_TYPE(char * args);
	void CMD_REM(char * args);
	void CMD_RENAME(char * args);
	void CMD_CALL(char * args);
	void SyntaxError();
	void CMD_PAUSE(char * args);
	void CMD_SUBST(char* args);
	void CMD_LOADHIGH(char* args);
	void CMD_CHOICE(char * args);
	void CMD_ATTRIB(char * args);
	void CMD_PATH(char * args);
	void CMD_SHIFT(char * args);
	void CMD_VER(char * args);
	void CMD_LS(char *args);

	/* The shell's variables */
	uint16_t input_handle = 0;
	std::shared_ptr<BatchFile> bf = {}; // shared with BatchFile.prev
	bool echo = false;
	bool call = false;
};

/* Object to manage lines in the autoexec.bat The lines get removed from
 * the file if the object gets destroyed. The environment is updated
 * as well if the line set a a variable */
class AutoexecObject{
private:
	bool installed = false;
	std::string buf = {};

public:
	AutoexecObject() = default;
	AutoexecObject(const std::string& line);
	~AutoexecObject();

	void Install(const std::string& in);
	void InstallBefore(const std::string& in);

private:
	void CreateAutoexec();
};

#endif
