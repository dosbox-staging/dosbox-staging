/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#include "dos_system.h"
#include "shell.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <map>
#include <regex>
#include <stack>
#include <string>
#include <vector>

#include "../dos/program_more_output.h"
#include "../ints/int10.h"
#include "ansi_code_markup.h"
#include "bios.h"
#include "callback.h"
#include "control.h"
#include "cross.h"
#include "dos_inc.h"
#include "drives.h"
#include "paging.h"
#include "regs.h"
#include "string_utils.h"
#include "support.h"
#include "timer.h"

// clang-format off
static const std::map<std::string, SHELL_Cmd> shell_cmds = {
	{ "CALL",     {&DOS_Shell::CMD_CALL,     "CALL",     HELP_Filter::All,    HELP_Category::Batch } },
	{ "CD",       {&DOS_Shell::CMD_CHDIR,    "CHDIR",    HELP_Filter::Common, HELP_Category::File } },
	{ "CHDIR",    {&DOS_Shell::CMD_CHDIR,    "CHDIR",    HELP_Filter::All,    HELP_Category::File } },
	{ "CLS",      {&DOS_Shell::CMD_CLS,      "CLS",      HELP_Filter::Common, HELP_Category::Misc} },
	{ "COPY",     {&DOS_Shell::CMD_COPY,     "COPY",     HELP_Filter::Common, HELP_Category::File} },
	{ "DATE",     {&DOS_Shell::CMD_DATE,     "DATE",     HELP_Filter::All,    HELP_Category::Misc } },
	{ "DEL",      {&DOS_Shell::CMD_DELETE,   "DELETE",   HELP_Filter::Common, HELP_Category::File } },
	{ "DELETE",   {&DOS_Shell::CMD_DELETE,   "DELETE",   HELP_Filter::All,    HELP_Category::File } },
	{ "DIR",      {&DOS_Shell::CMD_DIR,      "DIR",      HELP_Filter::Common, HELP_Category::File } },
	{ "ECHO",     {&DOS_Shell::CMD_ECHO,     "ECHO",     HELP_Filter::All,    HELP_Category::Batch } },
	{ "ERASE",    {&DOS_Shell::CMD_DELETE,   "DELETE",   HELP_Filter::All,    HELP_Category::File } },
	{ "EXIT",     {&DOS_Shell::CMD_EXIT,     "EXIT",     HELP_Filter::Common, HELP_Category::Misc } },
	{ "FOR",      {&DOS_Shell::CMD_FOR,      "FOR",      HELP_Filter::All,    HELP_Category::Batch } },
	{ "GOTO",     {&DOS_Shell::CMD_GOTO,     "GOTO",     HELP_Filter::All,    HELP_Category::Batch } },
	{ "IF",       {&DOS_Shell::CMD_IF,       "IF",       HELP_Filter::All,    HELP_Category::Batch } },
	{ "LH",       {&DOS_Shell::CMD_LOADHIGH, "LOADHIGH", HELP_Filter::All,    HELP_Category::Misc } },
	{ "LOADHIGH", {&DOS_Shell::CMD_LOADHIGH, "LOADHIGH", HELP_Filter::All,    HELP_Category::Misc } },
	{ "MD",       {&DOS_Shell::CMD_MKDIR,    "MKDIR",    HELP_Filter::Common, HELP_Category::File } },
	{ "MKDIR",    {&DOS_Shell::CMD_MKDIR,    "MKDIR",    HELP_Filter::All,    HELP_Category::File } },
	{ "PATH",     {&DOS_Shell::CMD_PATH,     "PATH",     HELP_Filter::All,    HELP_Category::Misc} },
	{ "PAUSE",    {&DOS_Shell::CMD_PAUSE,    "PAUSE",    HELP_Filter::All,    HELP_Category::Batch } },
	{ "RD",       {&DOS_Shell::CMD_RMDIR,    "RMDIR",    HELP_Filter::Common, HELP_Category::File } },
	{ "REM",      {&DOS_Shell::CMD_REM,      "REM",      HELP_Filter::All,    HELP_Category::Batch } },
	{ "REN",      {&DOS_Shell::CMD_RENAME,   "RENAME",   HELP_Filter::Common, HELP_Category::File } },
	{ "RENAME",   {&DOS_Shell::CMD_RENAME,   "RENAME",   HELP_Filter::All,    HELP_Category::File } },
	{ "RMDIR",    {&DOS_Shell::CMD_RMDIR,    "RMDIR",    HELP_Filter::All,    HELP_Category::File } },
	{ "SET",      {&DOS_Shell::CMD_SET,      "SET",      HELP_Filter::All,    HELP_Category::Misc} },
	{ "SHIFT",    {&DOS_Shell::CMD_SHIFT,    "SHIFT",    HELP_Filter::All,    HELP_Category::Batch } },
	{ "TIME",     {&DOS_Shell::CMD_TIME,     "TIME",     HELP_Filter::All,    HELP_Category::Misc } },
	{ "TYPE",     {&DOS_Shell::CMD_TYPE,     "TYPE",     HELP_Filter::Common, HELP_Category::Misc } },
	{ "VER",      {&DOS_Shell::CMD_VER,      "VER",      HELP_Filter::All,    HELP_Category::Misc } },
	{ "VOL",      {&DOS_Shell::CMD_VOL,      "VOL",      HELP_Filter::All,    HELP_Category::Misc } }
};
// clang-format on

// support functions

static void StripSpaces(char*& args)
{
	while (args && *args && isspace(*reinterpret_cast<unsigned char*>(args)))
		args++;
}

static void StripSpaces(char*&args,char also) {
	while (args && *args && (isspace(*reinterpret_cast<unsigned char*>(args)) || (*args == also)))
		args++;
}

static char *ExpandDot(const char *args, char *buffer, size_t bufsize)
{
	if (*args == '.') {
		if (*(args+1) == 0){
			safe_strncpy(buffer, "*.*", bufsize);
			return buffer;
		}
		if ( (*(args+1) != '.') && (*(args+1) != '\\') ) {
			buffer[0] = '*';
			buffer[1] = 0;
			if (bufsize > 2) strncat(buffer,args,bufsize - 1 /*used buffer portion*/ - 1 /*trailing zero*/  );
			return buffer;
		} else
			safe_strncpy (buffer, args, bufsize);
	}
	else safe_strncpy(buffer,args, bufsize);
	return buffer;
}

bool lookup_shell_cmd(std::string name, SHELL_Cmd &shell_cmd)
{
	for (auto &c : name)
		c = toupper(c);

	const auto result = shell_cmds.find(name);
	if (result == shell_cmds.end())
		return false; // name isn't a shell command!

	shell_cmd = result->second;
	return true;
}

bool DOS_Shell::ExecuteConfigChange(const char* const cmd_in, const char* const line)
{
	assert(control);
	const auto section_dosbox = static_cast<Section_prop*>(
	        control->GetSection("dosbox"));
	assert(section_dosbox);
	if (!section_dosbox->Get_bool("shell_config_shortcuts")) {
		return false;
	}

	Section* test = control->GetSectionFromProperty(cmd_in);
	if (!test) {
		return false;
	}

	if (line && !line[0]) {
		std::string val = test->GetPropValue(cmd_in);
		if (val != NO_SUCH_PROPERTY) {
			WriteOut("%s\n", val.c_str());
		}
		return true;
	}

	char newcom[1024];
	safe_sprintf(newcom, "z:\\config -set %s %s%s", test->GetName(), cmd_in,
	             line ? line : "");
	DoCommand(newcom);

	return true;
}

bool DOS_Shell::ExecuteShellCommand(const char* const name, char* arguments)
{
	SHELL_Cmd shell_cmd = {};
	if (!lookup_shell_cmd(name, shell_cmd))
		return false; // name isn't a shell command!
	(this->*(shell_cmd.handler))(arguments);
	return true;
}

void DOS_Shell::DoCommand(char* line)
{
	// First split the line into command and arguments
	line = trim(line);
	char cmd_buffer[CMD_MAXLINE];
	char* cmd_write = cmd_buffer;

	auto is_cli_delimiter = [](const char c) {
		constexpr std::array<char, 7> Delimiters = {'\0', ' ', '/', '\t', '=', '"'};
		// Note: ':' is also a delimiter, but handling it here breaks
		//       drive switching as that is handled at a later stage.
		return contains(Delimiters, c);
	};

	// Scan forward until we hit the first delimiter
	while (!is_cli_delimiter(line[0])) {
		// Handle squashed . and \ syntax like real MS-DOS:
		//   C:\> cd\keen
		//   C:\KEEN> cd..
		//   C:\> dir.exe
		if ((*line == '.') || (*line == '\\')) {
			*cmd_write = 0;
			if (ExecuteShellCommand(cmd_buffer, line)) {
				return;
			}
		}
		*cmd_write++ = *line++;
	}
	*cmd_write = 0;
	if (is_empty(cmd_buffer)) {
		return;
	}

	// First try to execute the line as internal shell command
	if (ExecuteShellCommand(cmd_buffer, line)) {
		return;
	}
	// Try to execute the line as external program
	if (ExecuteProgram(cmd_buffer, line)) {
		return;
	}
	// Last resort - try to handle the line as configuration change request
	if (ExecuteConfigChange(cmd_buffer, line)) {
		return;
	}

	WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"), cmd_buffer);
}

bool DOS_Shell::WriteHelp(const std::string &command, char *args) {
	if (!args || !scan_and_remove_cmdline_switch(args, "?"))
		return false;

	MoreOutputStrings output(*this);
	std::string short_key("SHELL_CMD_" + command + "_HELP");
	output.AddString("%s\n", MSG_Get(short_key.c_str()));
	std::string long_key("SHELL_CMD_" + command + "_HELP_LONG");
	if (MSG_Exists(long_key.c_str()))
		output.AddString("%s", MSG_Get(long_key.c_str()));
	else
		output.AddString("%s\n", command.c_str());
	output.Display();

	return true;
}

#define HELP(command) if (WriteHelp((command), args)) return

void DOS_Shell::CMD_CLS(char *args)
{
	HELP("CLS");

	// Thank you to FreeDOS
	// Logic for this command taken from:
	// https://github.com/FDOS/freecom/blob/master/cmd/cls.c

	// Scroll screen
	reg_ah = 0x06;

	// Color (0x07 is light gray text on black)
	// https://en.wikipedia.org/wiki/BIOS_color_attributes
	// NOTE: FreeDOS sets this to 0x00 for certain video modes.
	// They seem to be non-text modes and I was unable to find a way to get the console into them.
	// Probably this isn't needed for our purposes so I've excluded the check for simplicity
	reg_bh = 0x07;

	// Copy 0 lines
	reg_al = 0;

	// Top left of the fill (0, 0)
	reg_cx = 0;

	// Fill to max
	// Gets capped to screen dimensions in INT10_ScrollWindow
	reg_dx = 0xffff;

	CALLBACK_RunRealInt(0x10);

	// Get video mode
	// Sets reg_bh = page
	reg_ah = 0x0f;
	CALLBACK_RunRealInt(0x10);

	// Set cursor position
	reg_ah = 0x02;

	// reg_bh = current video page
	// Set by above interrupt

	// Set to (0, 0)
	reg_dx = 0;

	CALLBACK_RunRealInt(0x10);
}

void DOS_Shell::CMD_DELETE(char * args) {
	HELP("DELETE");

	char * rem=scan_remaining_cmdline_switch(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	/* If delete accept switches mind the space infront of them. See the dir /p code */

	char full[DOS_PATHLENGTH];
	char buffer[CROSS_LEN];
	args = ExpandDot(args,buffer, CROSS_LEN);
	StripSpaces(args);
	if (!DOS_Canonicalize(args,full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));return; }

	/* Command uses dta so set it to our internal dta */
	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);

	// TODO Maybe support confirmation for *.* like dos does.
	bool res = DOS_FindFirst(args, FatAttributeFlags::NotVolume);
	if (!res) {
		WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"), args);
		dos.dta(save_dta);
		return;
	}
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	char * end=strrchr(full,'\\')+1;*end=0;

	DOS_DTA::Result search_result = {};

	DOS_DTA dta(dos.dta());

	while (res) {
		search_result = {};
		dta.GetResult(search_result);
		strcpy(end, search_result.name.c_str());

		if (search_result.IsReadOnly()) {
			WriteOut(MSG_Get("SHELL_ACCESS_DENIED"), full);

		} else if (!search_result.IsDirectory()) {
			if (!DOS_UnlinkFile(full)) {
				WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"), full);
			}
		}
		res = DOS_FindNext();
	}
	dos.dta(save_dta);
}

void DOS_Shell::PrintHelpForCommands(MoreOutputStrings &output, HELP_Filter req_filter)
{
	static const auto format_header_str  = convert_ansi_markup("[color=light-blue]%s[reset]\n");
	static const auto format_command_str = convert_ansi_markup("  [color=light-green]%-8s[reset] %s");
	static const auto format_header      = format_header_str.c_str();
	static const auto format_command     = format_command_str.c_str();

	for (const auto &cat : {HELP_Category::Dosbox, HELP_Category::File, HELP_Category::Batch, HELP_Category::Misc}) {
		bool category_started = false;
		for (const auto &s : HELP_GetHelpList()) {
			if (req_filter == HELP_Filter::Common &&
				s.second.filter != HELP_Filter::Common)
				continue;
			if (s.second.category != cat)
				continue;
			if (!category_started) {
				// Only add a newline to the first category heading when
				// displaying "common" help
				if (cat != HELP_Category::Dosbox || req_filter == HELP_Filter::Common) {
					output.AddString("\n");
				}
				output.AddString(format_header, HELP_CategoryHeading(cat));
				category_started = true;
			}
			std::string name(s.first);
			lowcase(name);
			output.AddString(format_command, name.c_str(),
			                 HELP_GetShortHelp(s.second.name).c_str());
		}
	}
}

void DOS_Shell::AddShellCmdsToHelpList() {
	// Setup Help
	if (DOS_Shell::help_list_populated) {
		return;
	}
	for (const auto &c : shell_cmds) {
		HELP_AddToHelpList(c.first,
		                   HELP_Detail{c.second.filter,
		                               c.second.category,
		                               HELP_CmdType::Shell,
		                               c.second.help},
		                   true);
	}
	DOS_Shell::help_list_populated = true;
}

void DOS_Shell::CMD_HELP(char * args){
	HELP("HELP");

	upcase(args);
	SHELL_Cmd shell_cmd = {};
	char help_arg[] = "/?";
	const auto& hl = HELP_GetHelpList();
	if (contains(hl, args) && hl.at(args).type == HELP_CmdType::Program) {
		ExecuteProgram(args, help_arg);
	} else if (lookup_shell_cmd(args, shell_cmd)) {
		// Print help for the provided command by
		// calling it with the '/?' arg
		(this->*(shell_cmd.handler))(help_arg);
	} else if (scan_and_remove_cmdline_switch(args, "A") || scan_and_remove_cmdline_switch(args, "ALL")) {
		// Print help for all the commands
		MoreOutputStrings output(*this);
		PrintHelpForCommands(output, HELP_Filter::All);
		output.Display();
	} else {
		// Print help for just the common commands
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_HELP"));
		PrintHelpForCommands(output, HELP_Filter::Common);
		output.Display();
	}
}

void DOS_Shell::CMD_ECHO(char * args){
	if (!*args) {
		const auto echo_enabled = batchfiles.empty()
		                                ? echo
		                                : batchfiles.top().Echo();
		if (echo_enabled) {
			WriteOut(MSG_Get("SHELL_CMD_ECHO_ON"));
		} else {
			WriteOut(MSG_Get("SHELL_CMD_ECHO_OFF"));
		}
		return;
	}
	char buffer[512];
	char* pbuffer = buffer;
	safe_strcpy(buffer, args);
	StripSpaces(pbuffer);
	if (strcasecmp(pbuffer, "OFF") == 0) {
		if (batchfiles.empty()) {
			echo = false;
		} else {
			batchfiles.top().SetEcho(false);
		}
		return;
	}
	if (strcasecmp(pbuffer, "ON") == 0) {
		if (batchfiles.empty()) {
			echo = true;
		} else {
			batchfiles.top().SetEcho(true);
		}
		return;
	}
	if (strcasecmp(pbuffer,"/?") == 0) { HELP("ECHO"); }

	args++;//skip first character. either a slash or dot or space
	size_t len = strlen(args); //TODO check input of else ook nodig is.
	if (len && args[len - 1] == '\r') {
		LOG(LOG_MISC,LOG_WARN)("Hu ? carriage return already present. Is this possible?");
		WriteOut("%s\n",args);
	} else WriteOut("%s\r\n",args);
}

void DOS_Shell::CMD_EXIT(char *args)
{
	HELP("EXIT");

	const bool wants_force_exit = control->arguments.exit;
	const bool is_normal_launch = control->GetStartupVerbosity() !=
	                              Verbosity::InstantLaunch;

	// Check if this is an early-exit situation, in which case we avoid
	// exiting because the user might have a configuration problem and we
	// should let them see any errors in their console.
	constexpr auto early_exit_seconds = 1.5;
	const auto exiting_after_seconds  = DOSBOX_GetUptime();

	const auto not_early_exit = exiting_after_seconds > early_exit_seconds;

	if (wants_force_exit || is_normal_launch || not_early_exit) {
		exit_cmd_called = true;
		return;
	}

	WriteOut(MSG_Get("SHELL_CMD_EXIT_TOO_SOON"));
	LOG_WARNING("SHELL: Exit blocked because program quit after only %.1f seconds",
	            exiting_after_seconds);
}

void DOS_Shell::CMD_CHDIR(char * args) {
	HELP("CHDIR");
	StripSpaces(args);
	uint8_t drive = DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	if (!*args) {
		DOS_GetCurrentDir(0,dir);
		WriteOut("%c:\\%s\n",drive,dir);
	} else if (strlen(args) == 2 && args[1] == ':') {
		uint8_t targetdrive = (args[0] | 0x20) - 'a' + 1;
		unsigned char targetdisplay = *reinterpret_cast<unsigned char*>(&args[0]);
		if (!DOS_GetCurrentDir(targetdrive,dir)) {
			if (drive == 'Z') {
				WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(targetdisplay));
			} else {
				WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			}
			return;
		}
		WriteOut("%c:\\%s\n",toupper(targetdisplay),dir);
		if (drive == 'Z')
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT"),toupper(targetdisplay));
	} else 	if (!DOS_ChangeDir(args)) {
		/* Changedir failed. Check if the filename is longer then 8 and/or contains spaces */

		std::string temps(args),slashpart;
		std::string::size_type separator = temps.find_first_of("\\/");
		if (!separator) {
			slashpart = temps.substr(0,1);
			temps.erase(0,1);
		}
		separator = temps.find_first_of("\\/");
		if (separator != std::string::npos) temps.erase(separator);
		separator = temps.rfind('.');
		if (separator != std::string::npos) temps.erase(separator);
		separator = temps.find(' ');
		if (separator != std::string::npos) {/* Contains spaces */
			temps.erase(separator);
			if (temps.size() >6) temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else if (temps.size()>8) {
			temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else {
			if (drive == 'Z') {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_3"));
			} else {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_ERROR"),args);
			}
		}
	}
}

void DOS_Shell::CMD_MKDIR(char * args) {
	HELP("MKDIR");
	StripSpaces(args);
	char * rem=scan_remaining_cmdline_switch(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_MakeDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_MKDIR_ERROR"),args);
	}
}

void DOS_Shell::CMD_RMDIR(char * args) {
	HELP("RMDIR");
	StripSpaces(args);
	char * rem=scan_remaining_cmdline_switch(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
}

std::string format_number(const size_t num)
{
	if (num < 1) {
		return "0";
	}

	std::string buffer = {};

	std::string separator = " ";
	separator[0] = DOS_GetLocaleThousandsSeparator();

	auto tmp = num;
	while (tmp) {
		const auto current_num = tmp % 1000;
		const auto current_str = std::to_string(current_num);

		tmp /= 1000;

		if (tmp && current_num < 10) {
			buffer = separator + "00" + current_str + buffer;
		} else if (tmp && current_num < 100) {
			buffer = separator + "0" + current_str + buffer;
		} else if (tmp) {
			buffer = separator + current_str + buffer;
		} else {
			buffer = current_str + buffer;
		}
	}

	return buffer;
}

std::string shorten_path(const std::string& path, const size_t max_len)
{
	// NOTE: This routine tries to shorten the path, but in case of extreme
	// limit requested, it might be unable to shorten the path that much.
	//
	// max_len 20 or more should always work

	// Nothing to do if file name matches the constraint

	if (path.length() <= max_len) {
		return path;
	}

	// Extract parts of the path which can't be shortened

	std::string path_prefix = {}; // part which has to stay untouched
	std::string path_middle = path;
	std::string path_suffix = {}; // part which has to stay untouched

	if (!path_middle.empty() && path_middle.back() == '\\') {
		// Input ends with backslash
		path_suffix = "\\";
		path_middle.pop_back();
	}

	if (path_middle.size() >= 2 && path_middle[1] == ':' &&
	    ((path_middle[0] >= 'a' && path_middle[0] <= 'z') ||
	     (path_middle[0] >= 'A' && path_middle[0] <= 'Z'))) {
		// Input starts with DRIVE:
		if (path_middle.size() >= 3 && path_middle[2] == '\\') {
			path_prefix = path_middle.substr(0, 3);
			path_middle = path_middle.substr(3);
		} else {
			path_prefix = path_middle.substr(0, 2);
			path_middle = path_middle.substr(2);
		}
	}

	// Calculate length limit for path_middle part, do not allow for
	// any extreme limit
	// - 3 dots (ellipsis)
	// - 1 path separator
	// - 8 characters of name
	// - 1 dot
	// - 3 characters of extension
	// gives 16 characters, the code should always be prepared to display
	// something this long.

	constexpr size_t min_limit   = 16;
	const auto prefix_suffix_len = path_prefix.size() + path_suffix.size();
	const size_t limit = std::max(min_limit, max_len - prefix_suffix_len);

	// Try to strip path levels, one at a time

	static const std::string ellipsis = "...";
	while (path_middle.length() > limit &&
	       std::count(path_middle.begin(), path_middle.end(), '\\') > 1) {
		const auto pos = path_middle.find('\\', path_middle.find('\\') + 1);
		path_middle = ellipsis + path_middle.substr(pos);
	}

	// If still too long, just cut away the beginning

	const auto len = path_middle.length();
	if (len > limit) {
		path_middle = ellipsis + path_middle.substr(len - limit + 3);
	}

	return path_prefix + path_middle + path_suffix;
}

std::string to_search_pattern(const char* arg)
{
	std::string pattern = arg;
	trim(pattern);

	const char last_char = (pattern.length() > 0 ? pattern.back() : '\0');
	switch (last_char) {
	case '\0': // No arguments, search for all.
		pattern = "*.*";
		break;
	case '\\': // Handle \, C:\, etc.
	case ':':  // Handle C:, etc.
		pattern += "*.*";
		break;
	default: break;
	}

	// Handle patterns starting with a dot.
	char buffer[CROSS_LEN];
	pattern = ExpandDot(pattern.c_str(), buffer, sizeof(buffer));

	// When there's no wildcard and target is a directory then search files
	// inside the directory.
	const char *p = pattern.c_str();
	if (!strrchr(p, '*') && !strrchr(p, '?')) {
		FatAttributeFlags attr = {};
		if (DOS_GetFileAttr(p, &attr) && attr.directory) {
			pattern += "\\*.*";
		}
	}

	// If no extension, list all files.
	// This makes patterns like foo* work.
	if (!strrchr(pattern.c_str(), '.'))
		pattern += ".*";

	return pattern;
}

char *format_date(const uint16_t year, const uint8_t month, const uint8_t day)
{
	char format_string[6];
	static char return_date_buffer[15] = {0};
	const auto date_format    = DOS_GetLocaleDateFormat();
	const char date_separator = DOS_GetLocaleDateSeparator();
	int result;
	switch (date_format) {
	case DosDateFormat::DayMonthYear:
		result = sprintf(format_string, "D%cM%cY", date_separator,
		                 date_separator);
		break;
	case DosDateFormat::YearMonthDay:
		result = sprintf(format_string, "Y%cM%cD", date_separator,
		                 date_separator);
		break;
	default: // DosDateFormat::MonthDayYear
		result = sprintf(format_string, "M%cD%cY", date_separator,
		                 date_separator);
	}
	if (result < 0)
		return return_date_buffer;
	size_t index = 0;
	for (int i = 0; i < 5; i++) {
		if (i == 1 || i == 3) {
			return_date_buffer[index] = format_string[i];
			index++;
		} else {
			if (format_string[i] == 'M') {
				result = sprintf(return_date_buffer + index,
				                 "%02u", month);
				if (result >= 0)
					index += result;
			}
			if (format_string[i] == 'D') {
				result = sprintf(return_date_buffer + index,
				                 "%02u", day);
				if (result >= 0)
					index += result;
			}
			if (format_string[i] == 'Y') {
				result = sprintf(return_date_buffer + index,
				                 "%04u", year);
				if (result >= 0)
					index += result;
			}
		}
	}
	return return_date_buffer;
}

char* format_time(const uint8_t hour, const uint8_t min, const uint8_t sec,
                  const uint8_t msec, const bool full)
{
	uint8_t fhour = hour;
	static char return_time_buffer[19] = {0};
	char ampm[3] = "";
	if (DOS_GetLocaleTimeFormat() == DosTimeFormat::Time12H) {
		if (fhour != 12)
			fhour %= 12;
		strcpy(ampm, hour != 12 && hour == fhour ? "am" : "pm");
		if (!full)
			*(ampm + 1) = 0; // "a" or "p" in short time format
	}
	const char time_separator    = DOS_GetLocaleTimeSeparator();
	const char decimal_separator = DOS_GetLocaleDecimalSeparator();
	if (full) // Example full time format: 1:02:03.04am
		safe_sprintf(return_time_buffer, "%u%c%02u%c%02u%c%02u%s",
		             (unsigned int)fhour, time_separator,
		             (unsigned int)min, time_separator, (unsigned int)sec,
		             decimal_separator, (unsigned int)msec, ampm);
	else // Example short time format: 1:02p
		safe_sprintf(return_time_buffer, "%2u%c%02u%s", (unsigned int)fhour,
		             time_separator, (unsigned int)min, ampm);
	return return_time_buffer;
}

void DOS_Shell::CMD_DIR(char* args)
{
	HELP("DIR");

	std::string line = {};
	if (const auto envvar = psp->GetEnvironmentValue("DIRCMD")){
		line = std::string(args) + " " + *envvar;
		args=const_cast<char*>(line.c_str());
	}

	bool has_option_wide = scan_and_remove_cmdline_switch(args, "W");

	(void)scan_and_remove_cmdline_switch(args, "S");

	bool has_option_paging = scan_and_remove_cmdline_switch(args, "P");
	if (scan_and_remove_cmdline_switch(args,"WP") || scan_and_remove_cmdline_switch(args,"PW")) {
		has_option_paging = true;
		has_option_wide   = true;
	}

	bool has_option_bare = scan_and_remove_cmdline_switch(args, "B");

	bool has_option_all_dirs  = scan_and_remove_cmdline_switch(args, "AD");
	bool has_option_all_files = scan_and_remove_cmdline_switch(args, "A-D");

	// Sorting flags
	bool option_reverse          = false;
	ResultSorting option_sorting = ResultSorting::None;
	if (scan_and_remove_cmdline_switch(args, "ON")) {
		option_sorting = ResultSorting::ByName;
		option_reverse = false;
	}
	if (scan_and_remove_cmdline_switch(args, "O-N")) {
		option_sorting = ResultSorting::ByName;
		option_reverse = true;
	}
	if (scan_and_remove_cmdline_switch(args, "OD")) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = false;
	}
	if (scan_and_remove_cmdline_switch(args,"O-D")) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = true;
	}
	if (scan_and_remove_cmdline_switch(args, "OE")) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = false;
	}
	if (scan_and_remove_cmdline_switch(args,"O-E")) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = true;
	}
	if (scan_and_remove_cmdline_switch(args, "OS")) {
		option_sorting = ResultSorting::BySize;
		option_reverse = false;
	}
	if (scan_and_remove_cmdline_switch(args,"O-S")) {
		option_sorting = ResultSorting::BySize;
		option_reverse = true;
	}

	const char* rem = scan_remaining_cmdline_switch(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
	}

	const std::string pattern = to_search_pattern(args);

	// Make a full path in the args
	char path[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(pattern.c_str(), path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}

	// Prepare display engine
	MoreOutputStrings output(*this);
	output.SetOptionNoPaging(!has_option_paging);

	// DIR cmd in DOS and cmd.exe format 'Directory of <path>'
	// accordingly:
	// - only directory part of pattern passed as an argument
	// - do not append '\' to the directory name
	// - for root directories/drives: append '\' to the name
	char *last_dir_sep = strrchr(path, '\\');
	if (last_dir_sep == path + 2)
		*(last_dir_sep + 1) = '\0';
	else
		*last_dir_sep = '\0';

	const char drive_letter = path[0];
	const auto drive_idx = drive_index(drive_letter);
	const bool print_label  = (drive_letter >= 'A') && Drives.at(drive_idx);

	if (!has_option_bare) {
		if (print_label) {
			const auto label = To_Label(Drives.at(drive_idx)->GetLabel());
			output.AddString(MSG_Get("SHELL_CMD_DIR_VOLUME"),
			                 drive_letter,
			                 label.c_str());
		}
		// TODO: display volume serial number in DIR and TREE commands
		output.AddString(MSG_Get("SHELL_CMD_DIR_INTRO"), path);
		output.AddString("\n");
	}

	const bool is_root = strnlen(path, sizeof(path)) == 3;

	/* Command uses dta so set it to our internal dta */
	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	bool ret = DOS_FindFirst(pattern.c_str(), FatAttributeFlags::NotVolume);
	if (!ret) {
		if (!has_option_bare)
			output.AddString(MSG_Get("SHELL_FILE_NOT_FOUND"),
			                 pattern.c_str());
		dos.dta(save_dta);
		output.Display();
		return;
	}

	std::vector<DOS_DTA::Result> dir_contents;

	do { // File name and extension
		DOS_DTA::Result result = {};
		dta.GetResult(result);

		// Skip non-directories if option AD is present,
		// or skip dirs in case of A-D
		if (has_option_all_dirs && !result.IsDirectory()) {
			continue;
		} else if (has_option_all_files && result.IsDirectory()) {
			continue;
		}

		dir_contents.emplace_back(result);

	} while (DOS_FindNext());

	DOS_Sort(dir_contents, option_sorting, option_reverse);

	size_t byte_count   = 0;
	uint32_t file_count = 0;
	uint32_t dir_count  = 0;
	size_t wide_count   = 0;

	for (const auto& entry : dir_contents) {
		// Skip listing . and .. from toplevel directory, to simulate
		// DIR output correctly.
		// Bare format never lists .. nor . as directories.
		if (is_root || has_option_bare) {
			if (entry.IsDummyDirectory()) {
				continue;
			}
		}

		if (entry.IsDirectory()) {
			dir_count += 1;
		} else {
			file_count += 1;
			byte_count += entry.size;
		}

		// 'Bare' format: just the name, one per line, nothing else
		//
		if (has_option_bare) {
			output.AddString("%s\n", entry.name.c_str());
			continue;
		}

		// 'Wide list' format: using several columns
		//
		if (has_option_wide) {
			if (entry.IsDirectory()) {
				const int length = static_cast<int>(entry.name.length());
				output.AddString("[%s]%*s",
				                 entry.name.c_str(),
				                 (14 - length),
				                 "");
			} else {
				output.AddString("%-16s", entry.name.c_str());
			}
			wide_count += 1;
			if (!(wide_count % 5)) {
				// TODO: should auto-adapt to screen width
				output.AddString("\n");
			}
			continue;
		}

		// default format: one detailed entry per line
		//
		const auto year   = static_cast<uint16_t>((entry.date >> 9) + 1980);
		const auto month  = static_cast<uint8_t>((entry.date >> 5) & 0x000f);
		const auto day    = static_cast<uint8_t>(entry.date & 0x001f);
		const auto hour   = static_cast<uint8_t>((entry.time >> 5) >> 6);
		const auto minute = static_cast<uint8_t>((entry.time >> 5) & 0x003f);

		output.AddString("%-8s %-3s   %21s %s %s\n",
		                 entry.GetBareName().c_str(),
		                 entry.GetExtension().c_str(),
		                 entry.IsDirectory()
		                         ? "<DIR>"
		                         : format_number(entry.size).c_str(),
		                 format_date(year, month, day),
		                 format_time(hour, minute, 0, 0));
	}

	// Additional newline in case last line in 'Wide list` format was not
	// wrapped automatically
	if (has_option_wide && (wide_count % 5)) {
		// TODO: should auto-adapt to screen width
		output.AddString("\n");
	}

	// Show the summary of results
	if (!has_option_bare) {
		output.AddString(MSG_Get("SHELL_CMD_DIR_BYTES_USED"),
		                 file_count,
		                 format_number(byte_count).c_str());

		uint8_t drive = dta.GetSearchDrive();
		size_t free_space = 1024 * 1024 * 100;
		if (Drives.at(drive)) {
			uint16_t bytes_sector;
			uint8_t  sectors_cluster;
			uint16_t total_clusters;
			uint16_t free_clusters;
			Drives.at(drive)->AllocationInfo(&bytes_sector,
			                                 &sectors_cluster,
			                                 &total_clusters,
			                                 &free_clusters);
			free_space = bytes_sector;
			free_space *= sectors_cluster;
			free_space *= free_clusters;
		}

		output.AddString(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"),
		                 dir_count,
		                 format_number(free_space).c_str());
	}
	dos.dta(save_dta);
	output.Display();
}

struct copysource {
	std::string filename = "";
	bool concat = false;

	copysource() = default;

	copysource(const std::string &file, bool cat)
	        : filename(file),
	          concat(cat)
	{}
};

void DOS_Shell::CMD_COPY(char* args)
{
	HELP("COPY");
	static char defaulttarget[] = ".";
	StripSpaces(args);
	/* Command uses dta so set it to our internal dta */
	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	DOS_DTA::Result search_result = {};
	std::vector<copysource> sources;
	// ignore /b and /t switches: always copy binary
	while (scan_and_remove_cmdline_switch(args,"B")) ;
	while (scan_and_remove_cmdline_switch(args,"T")) ; //Shouldn't this be A ?
	while (scan_and_remove_cmdline_switch(args,"A")) ;

	(void)scan_and_remove_cmdline_switch(args, "Y");
	(void)scan_and_remove_cmdline_switch(args, "-Y");
	(void)scan_and_remove_cmdline_switch(args, "V");

	char* rem = scan_remaining_cmdline_switch(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		dos.dta(save_dta);
		return;
	}
	// Gather all sources (extension to copy more then 1 file specified at command line)
	// Concatenating files go as follows: All parts except for the last bear the concat flag.
	// This construction allows them to be counted (only the non concat set)
	char* source_p = nullptr;
	char source_x[DOS_PATHLENGTH+CROSS_LEN];
	while ( (source_p = strip_word(args)) && *source_p ) {
		do {
			char* plus = strchr(source_p,'+');
			// If strip_word() previously cut at a space before a plus then
			// set concatenate flag on last source and remove leading plus.
			if (plus == source_p && sources.size()) {
				sources[sources.size() - 1].concat = true;
				// If spaces also followed plus then item is only a plus.
				if (is_empty(++source_p))
					break;
				plus = strchr(source_p,'+');
			}
			if (plus) *plus++ = 0;
			safe_strcpy(source_x, source_p);
			bool has_drive_spec = false;
			size_t source_x_len = safe_strlen(source_x);
			if (source_x_len>0) {
				if (source_x[source_x_len-1] == ':') has_drive_spec = true;
			}
			if (!has_drive_spec  && !strpbrk(source_p,"*?") ) { //doubt that fu*\*.* is valid
				if (DOS_FindFirst(source_p,
				                  FatAttributeFlags::NotVolume)) {
					dta.GetResult(search_result);
					if (search_result.IsDirectory()) {
						strcat(source_x, "\\*.*");
					}
				}
			}
			sources.emplace_back(copysource(source_x,(plus)?true:false));
			source_p = plus;
		} while (source_p && *source_p);
	}
	// At least one source has to be there
	if (!sources.size() || !sources[0].filename.size()) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		dos.dta(save_dta);
		return;
	};

	copysource target;
	// If more then one object exists and last target is not part of a
	// concat sequence then make it the target.
	if (sources.size() > 1 && !sources[sources.size() - 2].concat){
		target = sources.back();
		sources.pop_back();
	}
	//If no target => default target with concat flag true to detect a+b+c
	if (target.filename.size() == 0) target = copysource(defaulttarget,true);

	copysource oldsource;
	copysource source;
	uint32_t count = 0;
	while (sources.size()) {
		/* Get next source item and keep track of old source for concat start end */
		oldsource = source;
		source = sources[0];
		sources.erase(sources.begin());

		//Skip first file if doing a+b+c. Set target to first file
		if (!oldsource.concat && source.concat && target.concat) {
			target = source;
			continue;
		}

		/* Make a full path in the args */
		char pathSource[DOS_PATHLENGTH];
		char pathTarget[DOS_PATHLENGTH];

		if (!DOS_Canonicalize(source.filename.c_str(), pathSource)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		// cut search pattern
		char* pos = strrchr(pathSource,'\\');
		if (pos) *(pos+1) = 0;

		if (!DOS_Canonicalize(target.filename.c_str(), pathTarget)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		char* temp = strstr(pathTarget,"*.*");
		if (temp) *temp = 0;//strip off *.* from target

		// add '\\' if target is a directory
		bool target_is_file = true;
		const auto target_path_length = strlen(pathTarget);
		if (target_path_length > 0 && pathTarget[target_path_length - 1] != '\\') {
			if (DOS_FindFirst(pathTarget, FatAttributeFlags::NotVolume)) {
				dta.GetResult(search_result);
				if (search_result.IsDirectory()) {
					strcat(pathTarget, "\\");
					target_is_file = false;
				}
			}
		} else
			target_is_file = false;

		//Find first sourcefile
		bool ret = DOS_FindFirst(source.filename.c_str(),
		                         FatAttributeFlags::NotVolume);
		if (!ret) {
			WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"),
			         source.filename.c_str());
			dos.dta(save_dta);
			return;
		}

		uint16_t sourceHandle = 0;
		uint16_t targetHandle = 0;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH];

		bool second_file_of_current_source = false;
		while (ret) {
			dta.GetResult(search_result);

			if (!search_result.IsDirectory()) {
				safe_strcpy(nameSource, pathSource);
				safe_strcat(nameSource, search_result.name.c_str());
				// Open Source
				if (DOS_OpenFile(nameSource,0,&sourceHandle)) {
					// Create Target or open it if in concat mode
					safe_strcpy(nameTarget, pathTarget);
					const auto name_length = strlen(nameTarget);
					if (name_length > 0 && nameTarget[name_length - 1] == '\\')
						strcat(nameTarget,
						       search_result.name.c_str());

					//Special variable to ensure that copy * a_file, where a_file is not a directory concats.
					bool special = second_file_of_current_source && target_is_file;
					second_file_of_current_source = true;
					if (special) oldsource.concat = true;
					//Don't create a new file when in concat mode
					if (oldsource.concat || DOS_CreateFile(nameTarget,0,&targetHandle)) {
						uint32_t dummy=0;
						//In concat mode. Open the target and seek to the eof
						if (!oldsource.concat || (DOS_OpenFile(nameTarget,OPEN_READWRITE,&targetHandle) &&
					        	                  DOS_SeekFile(targetHandle,&dummy,DOS_SEEK_END))) {
							// Copy
							static uint8_t buffer[0x8000]; // static, otherwise stack overflow possible.
							uint16_t toread = 0x8000;
							do {
								DOS_ReadFile(sourceHandle, buffer, &toread);
								DOS_WriteFile(targetHandle, buffer, &toread);
							} while (toread == 0x8000);
							if (!oldsource.concat) {
								DOS_GetFileDate(
								        sourceHandle,
								        &search_result
								                 .time,
								        &search_result
								                 .date);
								DOS_SetFileDate(
								        targetHandle,
								        search_result
								                .time,
								        search_result
								                .date);
							}
							DOS_CloseFile(sourceHandle);
							DOS_CloseFile(targetHandle);
							WriteOut(" %s\n",
							         search_result
							                 .name.c_str());
							if (!source.concat && !special) count++; //Only count concat files once
						} else {
							DOS_CloseFile(sourceHandle);
							WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),
							         target.filename.c_str());
						}
					} else {
						DOS_CloseFile(sourceHandle);
						WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),
						         target.filename.c_str());
					}
				} else {
					WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),
					         source.filename.c_str());
				}
			};
			//On to the next file if the previous one wasn't a device
			ret = search_result.IsDevice() ? false : DOS_FindNext();
		};
	}

	WriteOut(MSG_Get("SHELL_CMD_COPY_SUCCESS"),count);
	dos.dta(save_dta);
}

static std::vector<std::string> all_dirs;
struct attributes {
	bool add_a = false;
	bool add_s = false;
	bool add_h = false;
	bool add_r = false;
	bool min_a = false;
	bool min_s = false;
	bool min_h = false;
	bool min_r = false;
};

static void show_attributes(DOS_Shell* shell, const FatAttributeFlags fattr,
                            const char* name)
{
	shell->WriteOut("  %c  %c%c%c	%s\n",
	                fattr.archive   ? 'A' : ' ',
	                fattr.hidden    ? 'H' : ' ',
	                fattr.system    ? 'S' : ' ',
	                fattr.read_only ? 'R' : ' ',
	                name);
}

char *get_filename(char *args)
{
	static char *fname = strrchr(args, '\\');
	if (fname != nullptr)
		fname++;
	else {
		fname = strrchr(args, ':');
		if (fname != nullptr)
			fname++;
		else
			fname = args;
	}
	return fname;
}

static bool attrib_recursive(DOS_Shell *shell,
                             char *args,
                             const DOS_DTA &dta,
                             const bool optS,
                             attributes attribs)
{
	char path[DOS_PATHLENGTH + 4], full[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(args, full) || strrchr(full, '\\') == nullptr) {
		shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return false;
	}
	bool found = false, res = DOS_FindFirst(args, FatAttributeFlags::NotVolume);
	if (!res && !optS)
		return false;
	char *end = strrchr(full, '\\');
	if (!end)
		return false;
	end++;
	*end = 0;
	strcpy(path, full);

	DOS_DTA::Result search_result = {};
	FatAttributeFlags fattr       = {};
	while (res) {
		dta.GetResult(search_result);
		const auto& name = search_result.name.c_str();
		if (!((!strcmp(name, ".") || !strcmp(name, "..") ||
		       strchr(args, '*') != nullptr || strchr(args, '?') != nullptr) &&
		      search_result.IsDirectory())) {
			found = true;
			strcpy(end, name);
			if (!*full || !DOS_GetFileAttr(full, &fattr)) {
				shell->WriteOut(MSG_Get("SHELL_CMD_ATTRIB_GET_ERROR"),
				                full);
			} else if (attribs.add_a || attribs.add_s || attribs.add_h ||
			           attribs.add_r || attribs.min_a || attribs.min_s ||
			           attribs.min_h || attribs.min_r) {
				if (attribs.min_a) {
					fattr.archive = false;
				} else if (attribs.add_a) {
					fattr.archive = true;
				}
				if (attribs.min_s) {
					fattr.system = false;
				} else if (attribs.add_s) {
					fattr.system = true;
				}
				if (attribs.min_h) {
					fattr.hidden = false;
				} else if (attribs.add_h) {
					fattr.hidden = true;
				}
				if (attribs.min_r) {
					fattr.read_only = false;
				} else if (attribs.add_r) {
					fattr.read_only = true;
				}

				if (DOS_SetFileAttr(full, fattr) &&
				    DOS_GetFileAttr(full, &fattr)) {
					show_attributes(shell, fattr, full);
				} else {
					shell->WriteOut(MSG_Get("SHELL_CMD_ATTRIB_SET_ERROR"),
					                full);
				}
			} else {
				show_attributes(shell, fattr, full);
			}
		}
		res = DOS_FindNext();
	}
	if (optS) {
		size_t len = strlen(path);
		strcat(path, "*.*");
		bool ret = DOS_FindFirst(path, FatAttributeFlags::NotVolume);
		*(path + len) = 0;
		if (ret) {
			std::vector<std::string> found_dirs;
			found_dirs.clear();
			do { /* File name and extension */
				DOS_DTA::Result result;
				dta.GetResult(result);
				if (result.IsDirectory() &&
				    !result.IsDummyDirectory()) {
					std::string fullname = result.name +
					                       std::string(1, '\\') +
					                       get_filename(args);
					found_dirs.push_back(fullname);
					safe_strcpy(path, fullname.c_str());
				}
			} while (DOS_FindNext());
			all_dirs.insert(all_dirs.begin() + 1,
			                found_dirs.begin(), found_dirs.end());
		}
	}
	return found;
}

void DOS_Shell::CMD_ATTRIB(char *args)
{
	HELP("ATTRIB");
	StripSpaces(args);

	bool optS = scan_and_remove_cmdline_switch(args, "S");
	char *rem = scan_remaining_cmdline_switch(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
	}
	bool add_attr_a = false, add_attr_s = false, add_attr_h = false,
	     add_attr_r = false, min_attr_a = false, min_attr_s = false,
	     min_attr_h = false, min_attr_r = false;
	char sfull[DOS_PATHLENGTH + 2];
	char *arg1;
	strcpy(sfull, "*.*");
	do {
		arg1 = strip_word(args);
		if (!strcasecmp(arg1, "+A"))
			add_attr_a = true;
		else if (!strcasecmp(arg1, "+S"))
			add_attr_s = true;
		else if (!strcasecmp(arg1, "+H"))
			add_attr_h = true;
		else if (!strcasecmp(arg1, "+R"))
			add_attr_r = true;
		else if (!strcasecmp(arg1, "-A"))
			min_attr_a = true;
		else if (!strcasecmp(arg1, "-S"))
			min_attr_s = true;
		else if (!strcasecmp(arg1, "-H"))
			min_attr_h = true;
		else if (!strcasecmp(arg1, "-R"))
			min_attr_r = true;
		else if (*arg1)
			safe_strcpy(sfull, arg1);
	} while (*args);

	char buffer[CROSS_LEN];
	args = ExpandDot(sfull, buffer, CROSS_LEN);
	StripSpaces(args);
	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	all_dirs.clear();
	all_dirs.emplace_back(std::string(args));
	bool found = false;
	while (!all_dirs.empty()) {
		attributes attribs = {add_attr_a, add_attr_s, add_attr_h,
		                      add_attr_r, min_attr_a, min_attr_s,
		                      min_attr_h, min_attr_r};
		if (attrib_recursive(this, (char *)all_dirs.begin()->c_str(),
		                     dta, optS, attribs))
			found = true;
		all_dirs.erase(all_dirs.begin());
	}
	if (!found)
		WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"), args);
	dos.dta(save_dta);
}

void DOS_Shell::CMD_SET(char * args) {
	HELP("SET");
	StripSpaces(args);
	if (!*args) {
		/* No command line show all environment lines */
		for (const auto& entry : psp->GetAllRawEnvironmentStrings()) {
			WriteOut("%s\n", entry.c_str());
		}
		return;
	}
	//There are args:
	char * pcheck = args;

	while (*pcheck && (*pcheck == ' ' || *pcheck == '\t')) {
		pcheck++;
	}
	if (*pcheck && strlen(pcheck) > 3 && (strncasecmp(pcheck, "/p ", 3) == 0)) {
		WriteOut(MSG_Get("SHELL_CMD_SET_OPTION_P_UNSUPPORTED"));
		return;
	}

	char* p = strpbrk(args, "=");
	if (!p) {
		auto variable = std::string(args);
		for (auto& c : variable) {
			c = std::toupper(c);
		}
		if (const auto value = psp->GetEnvironmentValue(args)) {
			WriteOut("%s\n",
			         std::string(variable + '=' + *value).c_str());
		} else {
			WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"), args);
		}
		return;
	} else {
		*p++=0;
		/* parse p for envirionment variables */
		char parsed[CMD_MAXLINE];
		char* p_parsed = parsed;
		while (*p) {
			if (*p != '%') *p_parsed++ = *p++; //Just add it (most likely path)
			else if ( *(p+1) == '%') {
				*p_parsed++ = '%'; p += 2; //%% => %
			} else {
				char * second = strchr(++p,'%');
				if (!second) continue;
				*second++ = 0;

				if (const auto envvar = psp->GetEnvironmentValue(p)) {
					const auto& temp = *envvar;
					const uintptr_t remaining_len = std::min(
					        sizeof(parsed) - static_cast<uintptr_t>(p_parsed - parsed),
					        sizeof(parsed));
					safe_strncpy(p_parsed,
					             temp.c_str(),
					             remaining_len);
					p_parsed += strlen(p_parsed);
				}
				p = second;
			}
		}
		*p_parsed = 0;
		/* Try setting the variable */
		if (!SetEnv(args,parsed)) {
			WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));
		}
	}
}

void DOS_Shell::CMD_IF(char* args)
{
	HELP("IF");
	StripSpaces(args,'=');
	bool has_not=false;

	while (strncasecmp(args, "NOT", 3) == 0) {
		if (!isspace(*reinterpret_cast<unsigned char*>(&args[3])) && (args[3] != '=')) break;
		args += 3;	//skip text
		//skip more spaces
		StripSpaces(args,'=');
		has_not = !has_not;
	}

	if (strncasecmp(args,"ERRORLEVEL",10) == 0) {
		args += 10;	//skip text
		//Strip spaces and ==
		StripSpaces(args,'=');
		char* word = strip_word(args);
		if (!isdigit(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER"));
			return;
		}

		uint8_t n = 0;
		do n = n * 10 + (*word - '0');
		while (isdigit(*++word));
		if (*word && !isspace(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER"));
			return;
		}
		/* Read the error code from DOS */
		if ((dos.return_code>=n) ==(!has_not)) DoCommand(args);
		return;
	}

	if (strncasecmp(args,"EXIST ",6) == 0) {
		args += 6; //Skip text
		StripSpaces(args);
		char* word = strip_word(args);
		if (!*word) {
			WriteOut(MSG_Get("SHELL_CMD_IF_EXIST_MISSING_FILENAME"));
			return;
		}

		{	/* DOS_FindFirst uses dta so set it to our internal dta */
			const RealPt save_dta=dos.dta();
			dos.dta(dos.tables.tempdta);
			bool ret = DOS_FindFirst(word, FatAttributeFlags::NotVolume);
			dos.dta(save_dta);
			if (ret == (!has_not)) DoCommand(args);
		}
		return;
	}

	/* Normal if string compare */

	char* word1 = args;
	// first word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;
	char* end_word1 = args;

	// scan for =
	while (*args && (*args != '='))
		args++;
	// check for ==
	if ((*args == 0) || (args[1] != '=')) {
		SyntaxError();
		return;
	}
	args += 2;
	StripSpaces(args,'=');

	char* word2 = args;
	// second word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;

	if (*args) {
		*end_word1 = 0;		// mark end of first word
		*args++ = 0;		// mark end of second word
		StripSpaces(args,'=');

		if ((strcmp(word1,word2) == 0) == (!has_not)) DoCommand(args);
	}
}

void DOS_Shell::CMD_GOTO(char * args) {
	HELP("GOTO");
	StripSpaces(args);
	if (batchfiles.empty()) return;
	if (*args &&(*args == ':')) args++;
	//label ends at the first space
	char* non_space = args;
	while (*non_space) {
		if ((*non_space == ' ') || (*non_space == '\t'))
			*non_space = 0;
		else non_space++;
	}
	if (!*args) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_MISSING_LABEL"));
		return;
	}
	assert(!batchfiles.empty());
	if (!batchfiles.top().Goto(args)) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_LABEL_NOT_FOUND"),args);
		batchfiles.pop();
		return;
	}
}

void DOS_Shell::CMD_SHIFT(char * args ) {
	HELP("SHIFT");
	if (!batchfiles.empty()) batchfiles.top().Shift();
}

void DOS_Shell::CMD_TYPE(char * args) {
	HELP("TYPE");
	StripSpaces(args);
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
		return;
	}
	uint16_t handle;
	char * word;
nextfile:
	word=strip_word(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"),word);
		return;
	}
	uint16_t n;uint8_t c;
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		if (c == 0x1a) break; // stop at EOF
		DOS_WriteFile(STDOUT,&c,&n);
	} while (n);
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}

void DOS_Shell::CMD_REM(char * args) {
	HELP("REM");
}

void DOS_Shell::CMD_PAUSE(char *args) {
	HELP("PAUSE");
	WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
	uint8_t c;
	uint16_t n = 1;
	DOS_ReadFile(STDIN, &c, &n);
	if (c == 0)
		DOS_ReadFile(STDIN, &c, &n); // read extended key
	WriteOut_NoParsing("\n");
}

void DOS_Shell::CMD_CALL(char * args){
	HELP("CALL");
	this->call=true; /* else the old batchfile will be closed first */
	this->ParseLine(args);
	this->call=false;
}

void DOS_Shell::CMD_DATE(char *args)
{
	const auto date_format    = DOS_GetLocaleDateFormat();
	const char date_separator = DOS_GetLocaleDateSeparator();
	char format[11];
	int result;
	switch (date_format) {
	case DosDateFormat::DayMonthYear:
		result = safe_sprintf(format, "DD%cMM%cYYYY", date_separator,
		                      date_separator);
		break;
	case DosDateFormat::YearMonthDay:
		result = safe_sprintf(format, "YYYY%cMM%cDD", date_separator,
		                      date_separator);
		break;
	default: // DosDateFormat::MonthDayYear
		result = safe_sprintf(format, "MM%cDD%cYYYY", date_separator,
		                      date_separator);
	}
	if (result < 0) {
		LOG_WARNING("SHELL: Incorrect date format");
		return;
	}
	if (scan_and_remove_cmdline_switch(args, "?")) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_DATE_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("SHELL_CMD_DATE_HELP_LONG"),
		                 format,
		                 format_date(2012, 10, 11));
		output.Display();
		return;
	}
	if (scan_and_remove_cmdline_switch(args, "H")) {
		// synchronize date with host
		const time_t curtime = time(nullptr);
		struct tm datetime;
		cross::localtime_r(&curtime, &datetime);
		reg_ah = 0x2b; // set system date
		reg_cx = static_cast<uint16_t>(datetime.tm_year + 1900);
		reg_dh = static_cast<uint8_t>(datetime.tm_mon + 1);
		reg_dl = static_cast<uint8_t>(datetime.tm_mday);
		CALLBACK_RunRealInt(0x21);
		return;
	}
	// check if a date was passed in command line
	uint32_t newday, newmonth, newyear;
	char date_separator_placeholder_1, date_separator_placeholder_2;
	int n;
	switch (date_format) {
	case DosDateFormat::DayMonthYear:
		n = sscanf(args, "%u%c%u%c%u", &newday, &date_separator_placeholder_1,
		           &newmonth, &date_separator_placeholder_2, &newyear);
		break;
	case DosDateFormat::YearMonthDay:
		n = sscanf(args, "%u%c%u%c%u", &newyear, &date_separator_placeholder_1,
		           &newmonth, &date_separator_placeholder_2, &newday);
		break;
	default: // DosDateFormat::MonthDayYear
		n = sscanf(args, "%u%c%u%c%u", &newmonth,
		           &date_separator_placeholder_1, &newday,
		           &date_separator_placeholder_2, &newyear);
	}
	if (n == 5 && date_separator_placeholder_1 == date_separator &&
	    date_separator_placeholder_2 == date_separator) {
		if (!is_date_valid(newyear, newmonth, newday))
			WriteOut(MSG_Get("SHELL_CMD_DATE_ERROR"));
		else {
			reg_cx = static_cast<uint16_t>(newyear);
			reg_dh = static_cast<uint8_t>(newmonth);
			reg_dl = static_cast<uint8_t>(newday);

			reg_ah = 0x2b; // set system date
			CALLBACK_RunRealInt(0x21);
			if (reg_al == 0xff) {
				WriteOut(MSG_Get("SHELL_CMD_DATE_ERROR"));
			}
		}
		return;
	}
	// display the current date
	reg_ah = 0x2a; // get system date
	CALLBACK_RunRealInt(0x21);

	const char *datestring = MSG_Get("SHELL_CMD_DATE_DAYS");
	uint32_t length;
	char day[6] = {0};
	if (sscanf(datestring, "%u", &length) && (length < 5) &&
	    (strlen(datestring) == (length * 7 + 1))) {
		// date string appears valid
		for (uint32_t i = 0; i < length; i++)
			day[i] = datestring[reg_al * length + 1 + i];
	}
	bool dateonly = scan_and_remove_cmdline_switch(args, "T");
	if (!dateonly) {
		WriteOut(MSG_Get("SHELL_CMD_DATE_NOW"));
		WriteOut("%s ", day);
	}
	WriteOut("%s\n",
	         format_date((uint16_t)reg_cx, (uint8_t)reg_dh, (uint8_t)reg_dl));
	if (!dateonly) {
		WriteOut(MSG_Get("SHELL_CMD_DATE_SETHLP"), format);
	}
}

void DOS_Shell::CMD_TIME(char * args) {
	char format[9], example[9];
	const char time_separator = DOS_GetLocaleTimeSeparator();
	sprintf(format, "hh%cmm%css", time_separator, time_separator);
	sprintf(example, "13%c14%c15", time_separator, time_separator);
	if (scan_and_remove_cmdline_switch(args, "?")) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_TIME_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("SHELL_CMD_TIME_HELP_LONG"), format, example);
		output.Display();
		return;
	}
	if (scan_and_remove_cmdline_switch(args, "H")) {
		// synchronize time with host
		const time_t curtime = time(nullptr);
		struct tm datetime;
		cross::localtime_r(&curtime, &datetime);
		reg_ah = 0x2d; // set system time
		reg_ch = static_cast<uint8_t>(datetime.tm_hour);
		reg_cl = static_cast<uint8_t>(datetime.tm_min);
		reg_dh = static_cast<uint8_t>(datetime.tm_sec);
		CALLBACK_RunRealInt(0x21);
		return;
	}
	uint32_t newhour, newminute, newsecond;
	char time_separator_placeholder_1, time_separator_placeholder_2;
	if (sscanf(args, "%u%c%u%c%u", &newhour, &time_separator_placeholder_1,
	           &newminute, &time_separator_placeholder_2, &newsecond) == 5 &&
	    time_separator_placeholder_1 == time_separator &&
	    time_separator_placeholder_2 == time_separator) {
		if (!is_time_valid(newhour, newminute, newsecond))
			WriteOut(MSG_Get("SHELL_CMD_TIME_ERROR"));
		else {
			reg_ch = static_cast<uint8_t>(newhour);
			reg_cl = static_cast<uint8_t>(newminute);
			reg_dh = static_cast<uint8_t>(newsecond);

			reg_ah = 0x2d; // set system time
			CALLBACK_RunRealInt(0x21);
			if (reg_al == 0xff) {
				WriteOut(MSG_Get("SHELL_CMD_TIME_ERROR"));
			}
		}
		return;
	}
	bool timeonly = scan_and_remove_cmdline_switch(args, "T");

	reg_ah = 0x2c; // get system time
	CALLBACK_RunRealInt(0x21);
	/*
	                reg_dl= // 1/100 seconds
	                reg_dh= // seconds
	                reg_cl= // minutes
	                reg_ch= // hours
	*/
	if (timeonly) {
		WriteOut("%u%c%02u%c%02u\n", reg_ch, time_separator, reg_cl,
		         time_separator, reg_dh);
	} else {
		WriteOut(MSG_Get("SHELL_CMD_TIME_NOW"));
		WriteOut("%s\n", format_time(reg_ch, reg_cl, reg_dh, reg_dl, true));
		WriteOut(MSG_Get("SHELL_CMD_TIME_SETHLP"), format);
	}
}

void DOS_Shell::CMD_SUBST (char * args) {
/* If more that one type can be substed think of something else
 * E.g. make basedir member dos_drive instead of localdrive
 */
	HELP("SUBST");
	char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
	char temp_str[2] = { 0,0 };
	try {
		safe_strcpy(mountstring, "MOUNT ");
		StripSpaces(args);
		std::string arg;
		CommandLine command("", args);

		// Expecting two arguments
		if (command.GetCount() != 2)
			throw 0;

		// Found first
		if (!command.FindCommand(1, arg))
			throw 0;
		if ((arg.size() > 1) && arg[1] != ':')
			throw(0);

		temp_str[0]=(char)toupper(args[0]);

		// Found second
		if (!command.FindCommand(2, arg))
			throw 0;

		const auto drive_idx = drive_index(temp_str[0]);
		if ((arg == "/D") || (arg == "/d")) {
			if (!Drives.at(drive_idx)) {
				throw 1; // targetdrive not in use
			}
			strcat(mountstring, "-u ");
			strcat(mountstring, temp_str);
			this->ParseLine(mountstring);
			return;
		}
		if (Drives.at(drive_idx)) {
			throw 0; // targetdrive in use
		}
		strcat(mountstring, temp_str);
		strcat(mountstring, " ");

   		uint8_t drive;char fulldir[DOS_PATHLENGTH];
		if (!DOS_MakeName(arg.c_str(), fulldir, &drive)) {
			throw 0;
		}

		const auto ldp = std::dynamic_pointer_cast<localDrive>(
		        Drives.at(drive));
		if (!ldp) {
			throw 0;
		}
		char newname[CROSS_LEN];
		safe_strcpy(newname, ldp->GetBasedir());
		strcat(newname,fulldir);
		CROSS_FILENAME(newname);
		ldp->dirCache.ExpandNameAndNormaliseCase(newname);
		strcat(mountstring,"\"");
		strcat(mountstring, newname);
		strcat(mountstring,"\"");
		this->ParseLine(mountstring);
	}
	catch(int a){
		if (a == 0) {
			WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		} else {
		       	WriteOut(MSG_Get("SHELL_CMD_SUBST_NO_REMOVE"));
		}
		return;
	}
	catch(...) {		//dynamic cast failed =>so no localdrive
		WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		return;
	}

	return;
}

void DOS_Shell::CMD_LOADHIGH(char *args){
	HELP("LOADHIGH");
	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	uint8_t umb_flag=dos_infoblock.GetUMBChainState();
	uint8_t old_memstrat=(uint8_t)(DOS_GetMemAllocStrategy()&0xff);
	if (umb_start == 0x9fff) {
		if ((umb_flag&1) == 0) DOS_LinkUMBsToMemChain(1);
		DOS_SetMemAllocStrategy(0x80);	// search in UMBs first
		this->ParseLine(args);
		uint8_t current_umb_flag=dos_infoblock.GetUMBChainState();
		if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy
	} else this->ParseLine(args);
}

void MAPPER_AutoType(std::vector<std::string> &sequence,
                     const uint32_t wait_ms,
                     const uint32_t pacing_ms);
void MAPPER_StopAutoTyping();
void DOS_21Handler();

void DOS_Shell::CMD_CHOICE(char * args){
	HELP("CHOICE");

	// Parse "/n"; does the user want to show choices or not?
	const bool should_show_choices = !scan_and_remove_cmdline_switch(args, "N");

	// Parse "/s"; does the user want choices to be case-sensitive?
	const bool always_capitalize = !scan_and_remove_cmdline_switch(args, "S");

	// Prepare the command line for use with regular expressions
	assert(args);
	std::string cmdline = args;
	std::smatch match; // will contain the last valid match
	std::stack<std::smatch> matches = {};

	// helper to snip the stack of regex matches from the cmdline
	auto snip_matches_from_cmdline = [&]() {
		while (!matches.empty()) {
			const auto &m = matches.top();
			const auto start = static_cast<size_t>(m.position());
			const auto length = static_cast<size_t>(m.length());
			assert(start + length <= cmdline.size());
			cmdline.erase(start, length);
			matches.pop();
		}
	};
	// helper to search the cmdline for the last regex match
	auto search_cmdline_for = [&](const std::regex &r) -> bool {
		matches = {};
		auto it = std::sregex_iterator(cmdline.begin(), cmdline.end(), r);
		while (it != std::sregex_iterator())
			matches.emplace(*it++);
		match = matches.size() ? matches.top() : std::smatch();
		return match.ready();
	};

	// Parse /c[:]abc ... has the user provided custom choices?
	static const std::regex re_choices("/[cC]:?([0-9a-zA-Z]+)");
	const auto has_choices = search_cmdline_for(re_choices);
	auto choices = has_choices ? match[1].str() : std::string("yn");
	if (always_capitalize)
		upcase(choices);
	remove_duplicates(choices);
	snip_matches_from_cmdline();

	// Parse /t[:]c,nn ... was a default choice and timeout provided?
	static const std::regex re_timeout(R"(/[tT]:?([0-9a-zA-Z]),(\d+))");
	auto has_default = search_cmdline_for(re_timeout);
	const auto default_wait_s = has_default ? std::stoi(match[2]) : 0;
	char default_choice = has_default ? match[1].str()[0] : '\0';
	if (always_capitalize)
		default_choice = check_cast<char>(toupper(default_choice));
	snip_matches_from_cmdline();

	// Parse and print any text message(s) witout whitespace and quotes
	static const std::regex re_trim(R"(^["\s]+|["\s]+$)");
	const auto messages = split(std::regex_replace(cmdline, re_trim, ""));
	for (const auto &message : messages)
		WriteOut("%s ", message.c_str());

	// Show question prompt of the form [a,b]? where a b are the choice values
	if (should_show_choices) {
		WriteOut_NoParsing("[");
		assert(choices.size() > 0);
		for (size_t i = 0; i < choices.size() - 1; i++)
			WriteOut("%c,", choices[i]);
		WriteOut("%c]?", choices.back());
	}

	// If a default was given, is it in the choices and is the wait valid?
	const auto using_auto_type = has_default &&
	                             contains(choices, default_choice) &&
	                             default_wait_s > 0;
	if (using_auto_type) {
		std::vector<std::string> sequence{std::string{default_choice}};
		const auto start_after_ms = static_cast<uint32_t>(default_wait_s * 1000);
		MAPPER_AutoType(sequence, start_after_ms, 500);
	}

	// Begin waiting for input, but maybe break on some conditions
	constexpr char ctrl_c = 3;
	char choice = '\0';
	uint16_t bytes_read = 1;
	while (!contains(choices, choice)) {
		DOS_ReadFile(STDIN, reinterpret_cast<uint8_t *>(&choice), &bytes_read);
		if (!bytes_read) {
			WriteOut_NoParsing(MSG_Get("SHELL_CMD_CHOICE_EOF"));
			dos.return_code = 255;
			LOG_ERR("CHOICE: Failed, returing errorlevel %u",
			        dos.return_code);
			return;
		}
		if (always_capitalize)
			choice = static_cast<char>(toupper(choice));
		if (using_auto_type)
			MAPPER_StopAutoTyping();
		if (shutdown_requested)
			break;
		if (choice == ctrl_c)
			break;
	}

	// Print the choice and return the index (or zero if aborted)
	const auto num_choices = static_cast<int>(choices.size());
	if (contains(choices, choice)) {
		WriteOut("%c\n", choice);
		const auto nth_choice = choices.find(choice) % 254 + 1;
		dos.return_code = check_cast<uint8_t>(nth_choice);
		LOG_MSG("CHOICE: '%c' (#%u of %d choices)", choice,
		        dos.return_code, num_choices);
	} else {
		WriteOut_NoParsing(MSG_Get("SHELL_CMD_CHOICE_ABORTED"));
		dos.return_code = 0;
		LOG_WARNING("CHOICE: Aborted, returning errorlevel %u (from %d choices)",
		            dos.return_code,
		            num_choices);
	}
}

void DOS_Shell::CMD_PATH(char *args){
	HELP("PATH");
	if (args && strlen(args)) {
		char set_path[DOS_PATHLENGTH + CROSS_LEN + 20] = {0};
		while (args && *args && (*args == '='|| *args == ' '))
			args++;
		if (strlen(args) == 1 && *args == ';')
			*args = 0;
		safe_sprintf(set_path, "set PATH=%s", args);
		this->ParseLine(set_path);
		return;
	} else {
		if (const auto envvar = psp->GetEnvironmentValue("PATH"))
			WriteOut("%s\n", envvar->c_str());
		else
			WriteOut("PATH=(null)\n");
	}
}

void DOS_Shell::CMD_VER(char *args)
{
	HELP("VER");
	if (args && strlen(args)) {
		char *word = strip_word(args);
		if (strcasecmp(word, "set")) {
			WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
			return;
		}

		// Despite declared as deprecated, we should probably never
		// remove it, for compatibility with original DOSBox
		LOG_WARNING("SHELL: Command 'ver set VERSION' is deprecated");

		word = strip_word(args);
		const auto new_version = DOS_ParseVersion(word, args);
		if (new_version.major || new_version.minor) {
			dos.version.major = new_version.major;
			dos.version.minor = new_version.minor;
		} else
			WriteOut(MSG_Get("SHELL_CMD_VER_INVALID"));
	} else {
		WriteOut(MSG_Get("SHELL_CMD_VER_VER"), DOSBOX_GetDetailedVersion(),
		         dos.version.major, dos.version.minor);
	}
}

void DOS_Shell::CMD_VOL(char* args)
{
	HELP("VOL");

	char drive;
	size_t len = 0;
	if (args) {
		StripSpaces(args);
		len = strlen(args);
	}
	if (len > 0) {
		drive = toupper(args[0]);
		if (len < 2 || args[1] != ':' || drive < 'A' || drive > 'Z') {
			SyntaxError();
			return;
		}
	} else {
		drive = drive_letter(DOS_GetDefaultDrive());
	}

	// DOS IOCTL
	reg_ah = 0x44;

	// Generic block device request
	reg_al = 0x0D;

	// Drive (0 = default drive, A = 1, B = 2, etc.)  DOSBox drives start A
	// = 0, so add 1.
	reg_bl = drive_index(drive) + 1;

	// Device type (0x08 for block device)
	reg_ch = 0x08;

	// Get volume information (returns serial number, volume label, and
	// filesystem type)
	reg_cl = 0x66;

	// Offset pointer for return information (DS:DX). Set to 0 for beginning
	// of data segment (hopefully this doesn't step on anyone else's memory?)
	reg_dx        = 0;
	dos.errorcode = 0;
	CALLBACK_RunRealInt(0x21);
	if (dos.errorcode) {
		WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"), drive);
		return;
	}
	const auto serial = mem_readd(Segs.phys[ds] + 2);

	// Read in non-null terminated string with spaces at the end.
	char label[DOS_NAMELENGTH] = {};
	MEM_BlockRead(Segs.phys[ds] + 6, label, ARRAY_LEN(label) - 1);

	const auto high = check_cast<uint16_t>((serial >> 16) & 0xFFFF);
	const auto low  = check_cast<uint16_t>(serial & 0xFFFF);
	const auto trimmed_label = trim(label);
	WriteOut(MSG_Get("SHELL_CMD_VOL_OUTPUT"), drive, trimmed_label, high, low);
}

static std::vector<std::string> cmd_move_parse_sources(const char*& args)
{
	bool parsing             = true;
	bool inside_quote        = false;
	bool stop_next_letter    = false;
	bool continue_after_stop = false;
	std::string source;
	std::vector<std::string> sources;

	// Sources are separated by commas and terminated by a trailing space
	while (parsing) {
		const auto ch = *args;
		switch (ch) {
		case '"':
			inside_quote = !inside_quote;
			++args;
			break;
		case ' ':
			if (inside_quote) {
				source.push_back(ch);
				++args;
			} else {
				if (!source.empty()) {
					sources.emplace_back(std::move(source));
					source              = {};
					stop_next_letter    = true;
					continue_after_stop = false;
				}
				++args;
			}
			break;
		case ',':
			stop_next_letter    = true;
			continue_after_stop = true;
			if (!source.empty()) {
				sources.emplace_back(std::move(source));
				source = {};
			}
			++args;
			break;
		case 0:
			if (!source.empty()) {
				sources.emplace_back(std::move(source));
			}
			parsing = false;
			break;
		default:
			if (stop_next_letter) {
				parsing             = continue_after_stop;
				continue_after_stop = false;
				stop_next_letter    = false;
			} else {
				source.push_back(ch);
				++args;
			}
		}
	}
	return sources;
}

static std::string cmd_move_parse_destination(const char*& args)
{
	std::string destination;
	bool parsing      = true;
	bool inside_quote = false;

	while (parsing) {
		const auto ch = *args;
		switch (ch) {
		case '"':
			inside_quote = !inside_quote;
			++args;
			break;
		case ' ':
			if (inside_quote) {
				destination.push_back(ch);
				++args;
			} else {
				++args;
				parsing = false;
			}
			break;
		case 0: parsing = false; break;
		default: destination.push_back(ch); ++args;
		}
	}
	return destination;
}

struct cmd_move_arguments {
	std::vector<std::string> sources = {};
	std::string destination          = {};
	const char* error                = nullptr;
};

cmd_move_arguments cmd_move_parse_arguments(const char* args)
{
	cmd_move_arguments ret;
	// args pointer gets advanced by the two helper functions
	ret.sources = cmd_move_parse_sources(args);
	if (ret.sources.empty()) {
		ret.error = "SHELL_MISSING_PARAMETER";
		return ret;
	}
	ret.destination = cmd_move_parse_destination(args);
	if (ret.destination.empty()) {
		ret.error = "SHELL_MISSING_PARAMETER";
		return ret;
	}
	// Check for extra arguments
	while (*args) {
		if (*args++ != ' ') {
			ret.error = "SHELL_TOO_MANY_PARAMETERS";
			return ret;
		}
	}
	return ret;
}

void DOS_Shell::CMD_MOVE(char* args)
{
	HELP("MOVE");

	// TODO: Add support for these flags along with the COPYCMD environment
	// variable (overwrite prompts) Also currently ignored by CMD_COPY
	(void)scan_and_remove_cmdline_switch(args, "Y");
	(void)scan_and_remove_cmdline_switch(args, "-Y");

	char* rem = scan_remaining_cmdline_switch(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), rem);
		return;
	}

	cmd_move_arguments parsed = cmd_move_parse_arguments(args);
	if (parsed.error) {
		WriteOut(MSG_Get(parsed.error));
		return;
	}

	char temp[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(parsed.destination.c_str(), temp)) {
		WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"),
		         parsed.destination.c_str());
		return;
	}
	std::string canonical_destination = temp;

	// Command uses dta so set it to our internal dta
	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	// Each raw source can be a wildcard (*.*) containing multiple real
	// sources.
	std::vector<std::string> final_sources;
	for (const std::string& source : parsed.sources) {
		FatAttributeFlags attr = {0xff};
		attr.volume.flip();
		if (!DOS_FindFirst(source.c_str(), attr._data)) {
			WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"), source.c_str());
			continue;
		}
		if (!DOS_Canonicalize(source.c_str(), temp)) {
			WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"), source.c_str());
			continue;
		}
		std::string source_dir = temp;
		source_dir = source_dir.substr(0, source_dir.rfind(DosSeparator) + 1);
		do {
			DOS_DTA::Result result = {};
			dta.GetResult(result);
			if (result.name != CurrentDirectory &&
			    result.name != ParentDirectory) {
				final_sources.emplace_back(source_dir + result.name);
			}
		} while (DOS_FindNext());
	}

	// Done with DTA. Restore it.
	dos.dta(save_dta);

	FatAttributeFlags destination_attr = {};
	bool destination_exists = DOS_GetFileAttr(canonical_destination.c_str(),
	                                          &destination_attr);
	bool dest_is_dir        = false;

	if (destination_exists) {
		if (destination_attr.directory) {
			dest_is_dir = true;
		} else if (final_sources.size() > 1) {
			WriteOut(MSG_Get("SHELL_CMD_MOVE_MULTIPLE_TO_SINGLE"));
			return;
		}
	} else if (final_sources.size() > 1) {
		dest_is_dir = true;
		if (!DOS_MakeDir(canonical_destination.c_str())) {
			WriteOut(MSG_Get("SHELL_CMD_MKDIR_ERROR"),
			         canonical_destination.c_str());
			return;
		}
	}

	if (dest_is_dir && canonical_destination.back() != DosSeparator) {
		canonical_destination.push_back(DosSeparator);
	}

	for (const std::string& source : final_sources) {
		std::string final_destination = canonical_destination;
		if (dest_is_dir) {
			final_destination += source.substr(
			        source.rfind(DosSeparator) + 1);
		}
		if (source == final_destination) {
			WriteOut(MSG_Get("SHELL_FILE_EXISTS"),
			         final_destination.c_str());
			continue;
		}
		// Overwrite existing file
		if (destination_exists && !dest_is_dir) {
			// Condition should be checked for previously
			assert(final_sources.size() == 1);
			if (!DOS_UnlinkFile(final_destination.c_str())) {
				WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),
				         final_destination.c_str());
				return;
			}
		}
		// If same drive, do a rename. Otherwise it needs a copy + delete.
		const auto source_drive_letter = source[0];
		const auto dest_drive_letter   = final_destination[0];
		if (source_drive_letter == dest_drive_letter) {
			if (DOS_Rename(source.c_str(), final_destination.c_str())) {
				WriteOut("%s => %s\n",
				         source.c_str(),
				         final_destination.c_str());
			} else {
				WriteOut(MSG_Get("SHELL_FILE_CREATE_ERROR"),
				         final_destination.c_str());
			}
		} else {
			FatAttributeFlags source_attr = 0;
			if (!DOS_GetFileAttr(source.c_str(), &source_attr)) {
				WriteOut("SHELL_FILE_NOT_FOUND", source.c_str());
				continue;
			}
			uint16_t source_handle = 0;
			if (!DOS_OpenFile(source.c_str(), OPEN_READ, &source_handle)) {
				WriteOut("SHELL_FILE_OPEN_ERROR", source.c_str());
				continue;
			}
			uint16_t dest_handle = 0;
			if (!DOS_CreateFile(final_destination.c_str(),
			                    source_attr,
			                    &dest_handle)) {
				WriteOut(MSG_Get("SHELL_FILE_CREATE_ERROR"),
				         final_destination.c_str());
				DOS_CloseFile(source_handle);
				continue;
			}
			constexpr uint16_t buffer_capacity = 4096;
			uint8_t buffer[buffer_capacity];
			uint16_t bytes_requested = buffer_capacity;
			bool success             = true;
			do {
				if (!DOS_ReadFile(source_handle, buffer, &bytes_requested)) {
					WriteOut(MSG_Get("SHELL_READ_ERROR"),
					         source.c_str());
					success = false;
					break;
				}
				uint16_t bytes_read = bytes_requested;
				if (!DOS_WriteFile(dest_handle, buffer, &bytes_requested) ||
				    bytes_requested != bytes_read) {
					WriteOut(MSG_Get("SHELL_WRITE_ERROR"),
					         final_destination.c_str());
					success = false;
					break;
				}
			} while (bytes_requested == buffer_capacity);

			if (success) {
				WriteOut("%s => %s\n",
				         source.c_str(),
				         final_destination.c_str());
			}

			DOS_CloseFile(source_handle);
			DOS_CloseFile(dest_handle);
			if (success) {
				if (!DOS_UnlinkFile(source.c_str())) {
					WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),
					         source.c_str());
				}
			}
		}
	}
}

static std::vector<std::string> search_files(const std::string_view query)
{
	const auto save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	const auto dta = DOS_DTA(dos.dta());

	auto found = DOS_FindFirst(std::string(query).c_str(),
	                           ~(FatAttributeFlags::Volume |
	                             FatAttributeFlags::Directory));

	std::vector<std::string> files = {};
	while (found) {
		DOS_DTA::Result result = {};
		dta.GetResult(result);
		files.emplace_back(std::move(result.name));
		found = DOS_FindNext();
	}

	dos.dta(save_dta);

	return files;
}

static DosFilename split_extension(const std::string& fullname)
{
	DosFilename split_name = {};
	size_t pos             = fullname.rfind('.');
	split_name.name        = fullname.substr(0, pos);
	++pos;
	if (pos > 0 && pos < fullname.size()) {
		split_name.ext = fullname.substr(pos);
	} else {
		split_name.ext = {};
	}
	return split_name;
}

static std::string handle_wildcards(const std::string& wildcards,
                                    const std::string& old_filename)
{
	std::string expanded_name = {};
	for (size_t i = 0; i < wildcards.size(); ++i) {
		char c    = wildcards[i];
		bool done = false;
		switch (c) {
		case '*':
			if (i < old_filename.size()) {
				expanded_name.append(old_filename, i);
			}
			done = true;
			break;
		case '?':
			if (i < old_filename.size()) {
				expanded_name.push_back(old_filename[i]);
			}
			break;
		default: expanded_name.push_back(c);
		}
		if (done) {
			break;
		}
	}
	return expanded_name;
}

void DOS_Shell::CMD_RENAME(char* args)
{
	HELP("RENAME");

	const std::string source = DOS_Canonicalize(strip_word(args));
	const std::string target = strip_word(args);
	if (source.empty() || target.empty()) {
		SyntaxError();
		return;
	}

	// Second argument must not contain a path
	if (target.find_first_of("\\:") != std::string::npos) {
		SyntaxError();
		return;
	}

	const std::string path      = source.substr(0, source.rfind('\\') + 1);
	const DosFilename wildcards = split_extension(target);

	// Search for files matching the first argument (may be multiple files
	// due to wildcards)
	for (const std::string& old_filename : search_files(source)) {
		const DosFilename old_split = split_extension(old_filename);

		DosFilename new_split = {};
		new_split.name = handle_wildcards(wildcards.name, old_split.name);
		new_split.ext = handle_wildcards(wildcards.ext, old_split.ext);

		std::string old_fullpath = path + old_filename;

		std::string new_fullpath = path + new_split.name;
		if (!new_split.ext.empty()) {
			new_fullpath.push_back('.');
			new_fullpath.append(new_split.ext);
		}

		if (!DOS_Rename(old_fullpath.c_str(), new_fullpath.c_str())) {
			WriteOut("Rename %s -> %s failed\n",
			         old_fullpath.c_str(),
			         new_fullpath.c_str());
		}
	}
}

void DOS_Shell::CMD_FOR(char* args)
{
	HELP("FOR");

	static constexpr auto delimiters = std::string_view(",;= \t");

	auto argsview = std::string_view(args);
	auto consume_next_token = [&argsview]() -> std::optional<std::string_view> {
		const auto start = argsview.find_first_not_of(delimiters);
		if (start == std::string_view::npos) {
			return {};
		}

		argsview       = argsview.substr(start);
		const auto end = argsview.find_first_of(delimiters);
		if (end == std::string_view::npos) {
			return {};
		}

		const auto token = argsview.substr(0, end);
		argsview         = argsview.substr(end + 1);
		return token;
	};

	const auto variable = consume_next_token();
	if (!variable || variable->size() != std::strlen("%") + 1 ||
	    variable->front() != '%') {
		SyntaxError();
		return;
	}

	const auto in_keyword = consume_next_token();
	if (!in_keyword || !iequals(*in_keyword, "IN")) {
		SyntaxError();
		return;
	}

	const auto parameters =
	        [&argsview]() -> std::optional<std::vector<std::string>> {
		const auto parameter_list_start = argsview.find_first_of('(');
		if (parameter_list_start == std::string_view::npos) {
			return {};
		}

		const auto parameter_list_end = argsview.find_first_of(')');
		if (parameter_list_end == std::string_view::npos) {
			return {};
		}

		const auto raw_parameter_input = argsview.substr(
		        parameter_list_start + std::strlen("("),
		        parameter_list_end - std::strlen(")") - parameter_list_start);
		auto raw_parameters = split(raw_parameter_input, delimiters);
		std::vector<std::string> expanded_parameters = {};

		for (auto& parameter : raw_parameters) {
			if (parameter.find('*') == std::string::npos &&
			    parameter.find('?') == std::string::npos) {
				expanded_parameters.emplace_back(std::move(parameter));
				continue;
			}

			auto files = search_files(parameter);
			expanded_parameters.insert(
			        expanded_parameters.end(),
			        std::make_move_iterator(files.begin()),
			        std::make_move_iterator(files.end()));
		}

		argsview = argsview.substr(parameter_list_end + std::strlen(")"));
		return expanded_parameters;
	}();

	if (!parameters) {
		SyntaxError();
		return;
	}

	const auto do_keyword = consume_next_token();
	if (!do_keyword || !iequals(*do_keyword, "DO")) {
		SyntaxError();
		return;
	}

	const auto raw_command = [&argsview]() -> std::optional<std::string_view> {
		const auto start = argsview.find_first_not_of(delimiters);
		if (start == std::string_view::npos) {
			return {};
		}

		return argsview.substr(start);
	}();

	if (!raw_command || iequals(raw_command->substr(0, std::strlen("for")), "for")) {
		SyntaxError();
		return;
	}


	for (const auto& parameter : *parameters) {
		// TODO: C++20: Use std::vformat instead
		// Remember to escape the braces in the command string
		auto command                    = std::string(*raw_command);
		std::string::size_type position = {};
		while ((position = command.find(*variable, position)) !=
		       std::string::npos) {
			command.replace(position, variable->size(), parameter);
			position += parameter.size();
		}
		// TODO: Pass command directly to ParseLine when it takes
		// a std::string_view instead of a char*
		char cmd_array[CMD_MAXLINE];
		std::strncpy(cmd_array, command.c_str(), CMD_MAXLINE);
		cmd_array[CMD_MAXLINE - 1] = '\0';
		ParseLine(cmd_array);
	}
}
