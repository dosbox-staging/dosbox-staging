/*
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#include <memory>
#include <optional>
#include <stack>
#include <string>

#include "callback.h"
#include "programs.h"

class DOS_Shell;

constexpr auto CMD_MAXLINE = 4096;

extern callback_number_t call_shellstop;

/* first_shell is used to add and delete stuff from the shell env
 * by "external" programs. (config) */
extern DOS_Shell* first_shell;

class LineReader {
public:
	virtual void Reset()       = 0;
	virtual std::optional<std::string> Read() = 0;

	virtual ~LineReader() = default;
};

class BatchFile {
public:
	BatchFile(const Environment& host, std::unique_ptr<LineReader> input_reader,
	          std::string_view entered_name, std::string_view cmd_line,
	          bool echo_on);
	BatchFile(const BatchFile&)            = delete;
	BatchFile& operator=(const BatchFile&) = delete;
	BatchFile(BatchFile&&)                 = delete;
	BatchFile& operator=(BatchFile&&)      = delete;
	~BatchFile()                           = default;

	bool ReadLine(char* line);
	bool Goto(std::string_view label);
	void Shift();
	void SetEcho(bool echo_on);
	[[nodiscard]] bool Echo() const;

private:
	[[nodiscard]] std::string ExpandedBatchLine(std::string_view line) const;
	[[nodiscard]] std::optional<std::string> GetLine();

	const Environment& shell;
	CommandLine cmd;
	std::unique_ptr<LineReader> reader;
	bool echo;
};

class AutoexecEditor;
class MoreOutputStrings;

struct SHELL_Cmd {
	void (DOS_Shell::*handler)(char* args) = nullptr; // Handler for this
	                                                  // command
	const char* help       = ""; // String with command help
	HELP_Filter filter     = HELP_Filter::Common;
	HELP_Category category = HELP_Category::Misc;
};

class ShellHistory {
public:
	std::vector<std::string> GetCommands(uint16_t code_page) const;
	void Append(std::string command, uint16_t code_page);

	ShellHistory();
	~ShellHistory();
	ShellHistory(const ShellHistory&)            = delete;
	ShellHistory& operator=(const ShellHistory&) = delete;
	ShellHistory(ShellHistory&&)                 = delete;
	ShellHistory& operator=(ShellHistory&&)      = delete;

private:
	std::vector<std::string> commands{};
	std_fs::path path;
};

class DOS_Shell : public Program {
private:
	void PrintHelpForCommands(MoreOutputStrings& output, HELP_Filter req_filter);
	void AddShellCmdsToHelpList();
	bool WriteHelp(const std::string& command, char* args);
	[[nodiscard]] std::string ReadCommand();
	[[nodiscard]] std::string SubstituteEnvironmentVariables(std::string_view command);
	[[nodiscard]] std::string ResolvePath(std::string_view name) const;

	bool ExecuteConfigChange(const char* const cmd_in, const std::string args);

	friend class AutoexecEditor;

	std::shared_ptr<ShellHistory> history  = {};
	std::stack<BatchFile> batchfiles       = {};
	uint16_t input_handle                  = STDIN;
	bool call                              = false;
	bool exit_cmd_called                   = false;
	static inline bool help_list_populated = false;

public:
	DOS_Shell();
	~DOS_Shell() override                  = default;
	DOS_Shell(const DOS_Shell&)            = delete; // prevent copy
	DOS_Shell& operator=(const DOS_Shell&) = delete; // prevent assignment
	void Run() override;
	void RunBatchFile();

	/* A load of subfunctions */
	void ParseLine(char* line);
	void InputCommand(char* line);
	void ShowPrompt();
	void DoCommand(char* cmd);

	// Returned by the GetRedirection(...) member function
	struct RedirectionResults {
		std::string processed_line = {};
		std::string in_file        = {};
		std::string out_file       = {};
		std::string pipe_target    = {};
		bool is_appending          = false;
	};
	static std::optional<RedirectionResults> GetRedirection(const std::string_view line);

	// Execute external shell command / program / configuration change
	// 'virtual' needed for unit tests
	virtual bool ExecuteShellCommand(const char* const name, char* arguments);
	bool ExecuteProgram(std::string_view name, std::string_view args);

	// HACK: Don't use in new code
	// TODO: Remove the call to this function from autoexec
	bool SetEnv(std::string_view entry, std::string_view new_string);

	/* Commands */
	void CMD_HELP(char* args);
	void CMD_CLS(char* args);
	void CMD_COPY(char* args);
	void CMD_DATE(char* args);
	void CMD_TIME(char* args);
	void CMD_DIR(char* args);
	void CMD_DELETE(char* args);
	void CMD_ECHO(char* args);
	void CMD_EXIT(char* args);
	void CMD_FOR(char* args);
	void CMD_MKDIR(char* args);
	void CMD_CHDIR(char* args);
	void CMD_RMDIR(char* args);
	void CMD_SET(char* args);
	void CMD_IF(char* args);
	void CMD_GOTO(char* args);
	void CMD_TYPE(char* args);
	void CMD_REM(char* args);
	void CMD_RENAME(char* args);
	void CMD_CALL(char* args);
	void SyntaxError();
	void CMD_PAUSE(char* args);
	void CMD_SUBST(char* args);
	void CMD_LOADHIGH(char* args);
	void CMD_CHOICE(char* args);
	void CMD_ATTRIB(char* args);
	void CMD_PATH(char* args);
	void CMD_SHIFT(char* args);
	void CMD_VER(char* args);
	void CMD_LS(char* args);
	void CMD_VOL(char* args);
	void CMD_MOVE(char* args);

	bool echo = true;
};

std::tuple<std::string, std::string, std::string, bool> parse_drive_conf(
        std::string drive_letter, const std_fs::path& conf_path);

std::string to_search_pattern(const char* arg);

// Localized output

char* format_date(const uint16_t year, const uint8_t month, const uint8_t day);
char* format_time(const uint8_t hour, const uint8_t min, const uint8_t sec,
                  const uint8_t msec, const bool full = false);
std::string format_number(const size_t num);

std::string shorten_path(const std::string& path, const size_t max_len);

#endif
