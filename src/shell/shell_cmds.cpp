/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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
#include "drives.h"
#include "paging.h"
#include "regs.h"
#include "string_utils.h"
#include "support.h"
#include "timer.h"

// clang-format off
static const std::map<std::string, SHELL_Cmd> shell_cmds = {
	{ "CALL",     {&DOS_Shell::CMD_CALL,     "CALL",     HELP_Filter::All, HELP_Category::Batch } },
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
	{ "SUBST",    {&DOS_Shell::CMD_SUBST,    "SUBST",    HELP_Filter::All,    HELP_Category::File} },
	{ "TIME",     {&DOS_Shell::CMD_TIME,     "TIME",     HELP_Filter::All,    HELP_Category::Misc } },
	{ "TYPE",     {&DOS_Shell::CMD_TYPE,     "TYPE",     HELP_Filter::Common, HELP_Category::Misc } },
	{ "VER",      {&DOS_Shell::CMD_VER,      "VER",      HELP_Filter::All,    HELP_Category::Misc } },
	};
// clang-format on

/* support functions */
static char empty_char = 0;
static char* empty_string = &empty_char;
static void StripSpaces(char*&args) {
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

bool DOS_Shell::CheckConfig(char *cmd_in, char *line) {
	Section* test = control->GetSectionFromProperty(cmd_in);
	if (!test)
		return false;

	if (line && !line[0]) {
		std::string val = test->GetPropValue(cmd_in);
		if (val != NO_SUCH_PROPERTY)
			WriteOut("%s\n", val.c_str());
		return true;
	}
	char newcom[1024];
	safe_sprintf(newcom, "z:\\config -set %s %s%s", test->GetName(), cmd_in,
	             line ? line : "");
	DoCommand(newcom);
	return true;
}

bool DOS_Shell::execute_shell_cmd(char *name, char *arguments) {
	SHELL_Cmd shell_cmd = {};
	if (!lookup_shell_cmd(name, shell_cmd))
		return false; // name isn't a shell command!
	(this->*(shell_cmd.handler))(arguments);
	return true;
}

void DOS_Shell::DoCommand(char * line) {
/* First split the line into command and arguments */
	line=trim(line);
	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write=cmd_buffer;

	while (*line) {
		if (*line == 32) break;
		if (*line == '/') break;
		if (*line == '\t') break;
		if (*line == '=') break;
//		if (*line == ':') break; //This breaks drive switching as that is handled at a later stage.
		if ((*line == '.') ||(*line == '\\')) {  //allow stuff like cd.. and dir.exe cd\kees
			*cmd_write=0;
			if (execute_shell_cmd(cmd_buffer, line)) {
				return;
			}
		}
		*cmd_write++=*line++;
	}
	*cmd_write=0;
	if (is_empty(cmd_buffer))
		return;
	/* Check the internal list */
	if (execute_shell_cmd(cmd_buffer, line))
		return;
/* This isn't an internal command execute it */
	if (Execute(cmd_buffer,line)) return;
	if (CheckConfig(cmd_buffer,line)) return;
	WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),cmd_buffer);
}

bool DOS_Shell::WriteHelp(const std::string &command, char *args) {
	if (!args || !ScanCMDBool(args, "?"))
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

	// Re-apply current video mode. This clears the console, resets the palette, etc.
	if (CurMode->mode < 0x100) {
		reg_ah = 0x00;
		reg_al = static_cast<uint8_t>(CurMode->mode);
		CALLBACK_RunRealInt(0x10);
	} else {
		reg_ah = 0x4f;
		reg_al = 0x02;
		reg_bx = CurMode->mode;
		CALLBACK_RunRealInt(0x10);
	}
}

void DOS_Shell::CMD_DELETE(char * args) {
	HELP("DELETE");
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);

	char * rem=ScanCMDRemain(args);
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
//TODO Maybe support confirmation for *.* like dos does.
	bool res=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!res) {
		WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"), args);
		dos.dta(save_dta);
		return;
	}
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	char * end=strrchr(full,'\\')+1;*end=0;
	char name[DOS_NAMELENGTH_ASCII];uint32_t size;uint16_t time,date;uint8_t attr;
	DOS_DTA dta(dos.dta());
	while (res) {
		dta.GetResult(name,size,date,time,attr);
		strcpy(end, name);
		if (attr & DOS_ATTR_READ_ONLY) {
			WriteOut(MSG_Get("SHELL_ACCESS_DENIED"), full);
		} else if (!(attr & DOS_ATTR_DIRECTORY)) {
			if (!DOS_UnlinkFile(full)) WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),full);
		}
		res=DOS_FindNext();
	}
	dos.dta(save_dta);
}

void DOS_Shell::PrintHelpForCommands(MoreOutputStrings &output, HELP_Filter req_filter)
{
	static const auto format_header_str  = convert_ansi_markup("[color=blue]%s[reset]\n");
	static const auto format_command_str = convert_ansi_markup("  [color=green]%-8s[reset] %s");
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
		Execute(args, help_arg);
	} else if (lookup_shell_cmd(args, shell_cmd)) {
		// Print help for the provided command by
		// calling it with the '/?' arg
		(this->*(shell_cmd.handler))(help_arg);
	} else if (ScanCMDBool(args, "A") || ScanCMDBool(args, "ALL")) {
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

void DOS_Shell::CMD_RENAME(char * args){
	HELP("RENAME");
	StripSpaces(args);
	if (!*args) {SyntaxError();return;}
	if ((strchr(args,'*')!=NULL) || (strchr(args,'?')!=NULL) ) { WriteOut(MSG_Get("SHELL_CMD_NO_WILD"));return;}
	char * arg1=strip_word(args);
	StripSpaces(args);
	if (!*args) {SyntaxError();return;}
	char* slash = strrchr(arg1,'\\');
	if (slash) {
		/* If directory specified (crystal caves installer)
		 * rename from c:\X : rename c:\abc.exe abc.shr.
		 * File must appear in C:\
		 * Ren X:\A\B C => ren X:\A\B X:\A\C */

		char dir_source[DOS_PATHLENGTH + 4] = {0}; //not sure if drive portion is included in pathlength
		//Copy first and then modify, makes GCC happy
		safe_strcpy(dir_source, arg1);
		char* dummy = strrchr(dir_source,'\\');
		if (!dummy) { //Possible due to length
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}
		dummy++;
		*dummy = 0;

		//Maybe check args for directory, as I think that isn't allowed

		//dir_source and target are introduced for when we support multiple files being renamed.
		char target[DOS_PATHLENGTH+CROSS_LEN + 5] = {0};
		safe_strcpy(target, dir_source);
		strncat(target,args,CROSS_LEN);

		DOS_Rename(arg1,target);

	} else {
		DOS_Rename(arg1,args);
	}
}

void DOS_Shell::CMD_ECHO(char * args){
	if (!*args) {
		if (echo) { WriteOut(MSG_Get("SHELL_CMD_ECHO_ON"));}
		else { WriteOut(MSG_Get("SHELL_CMD_ECHO_OFF"));}
		return;
	}
	char buffer[512];
	char* pbuffer = buffer;
	safe_strcpy(buffer, args);
	StripSpaces(pbuffer);
	if (strcasecmp(pbuffer,"OFF") == 0) {
		echo=false;
		return;
	}
	if (strcasecmp(pbuffer,"ON") == 0) {
		echo=true;
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

	const bool wants_force_exit = control->cmdline->FindExist("-exit");
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
	char * rem=ScanCMDRemain(args);
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
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
}

static std::string format_number(size_t num)
{
	[[maybe_unused]] constexpr uint64_t petabyte_si = 1'000'000'000'000'000;
	assert(num <= petabyte_si);
	const auto b = static_cast<unsigned>(num % 1000);
	num /= 1000;
	const auto kb = static_cast<unsigned>(num % 1000);
	num /= 1000;
	const auto mb = static_cast<unsigned>(num % 1000);
	num /= 1000;
	const auto gb = static_cast<unsigned>(num % 1000);
	num /= 1000;
	const auto tb = static_cast<unsigned>(num);
	const char thousands_separator = dos.tables.country[DOS_THOUSANDS_SEPARATOR_OFS];
	char buf[22];
	if (tb) {
		safe_sprintf(buf, "%u%c%03u%c%03u%c%03u%c%03u", tb,
		             thousands_separator, gb, thousands_separator, mb,
		             thousands_separator, kb, thousands_separator, b);
		return buf;
	}
	if (gb) {
		safe_sprintf(buf, "%u%c%03u%c%03u%c%03u", gb,
		             thousands_separator, mb, thousands_separator, kb,
		             thousands_separator, b);
		return buf;
	}
	if (mb) {
		safe_sprintf(buf, "%u%c%03u%c%03u", mb, thousands_separator,
		             kb, thousands_separator, b);
		return buf;
	}
	if (kb) {
		safe_sprintf(buf, "%u%c%03u", kb, thousands_separator, b);
		return buf;
	}
	sprintf(buf, "%u", b);
	return buf;
}

struct DtaResult {
	char name[DOS_NAMELENGTH_ASCII];
	uint32_t size;
	uint16_t date;
	uint16_t time;
	uint8_t attr;

	static bool compareName(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.name, rhs.name) < 0; }
	static bool compareExt(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.getExtension(), rhs.getExtension()) < 0; }
	static bool compareSize(const DtaResult &lhs, const DtaResult &rhs) { return lhs.size < rhs.size; }
	static bool compareDate(const DtaResult &lhs, const DtaResult &rhs) { return lhs.date < rhs.date || (lhs.date == rhs.date && lhs.time < rhs.time); }

	const char * getExtension() const {
		const char * ext = empty_string;
		if (name[0] != '.') {
			ext = strrchr(name, '.');
			if (!ext) ext = empty_string;
		}
		return ext;
	}

};

static std::string to_search_pattern(const char *arg)
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
		uint16_t attr = 0;
		if (DOS_GetFileAttr(p, &attr) && (attr & DOS_ATTR_DIRECTORY))
			pattern += "\\*.*";
	}

	// If no extension, list all files.
	// This makes patterns like foo* work.
	if (!strrchr(pattern.c_str(), '.'))
		pattern += ".*";

	return pattern;
}

// Map a vector of dir contents to a vector of word widths.
static std::vector<int> to_name_lengths(const std::vector<DtaResult> &dir_contents,
                                        int padding)
{
	std::vector<int> ret;
	ret.reserve(dir_contents.size());
	for (const auto &entry : dir_contents) {
		const int len = static_cast<int>(strlen(entry.name));
		ret.push_back(len + padding);
	}
	return ret;
}

static std::vector<int> calc_column_widths(const std::vector<int> &word_widths,
                                           int min_col_width)
{
	assert(min_col_width > 0);

	// Actual terminal width (number of text columns) using current text
	// mode; in practice it's either 40, 80, or 132.
	const int term_width = INT10_GetTextColumns();

	// Use term_width-1 because we never want to print line up to the actual
	// limit; this would cause unnecessary line wrapping
	const size_t max_columns = (term_width - 1) / min_col_width;
	std::vector<int> col_widths(max_columns);

	// This function returns true when column number is too high to fit
	// words into a terminal width.  If it returns false, then the first
	// coln integers in col_widths vector describe the column widths.
	auto too_many_columns = [&](size_t coln) -> bool {
		std::fill(col_widths.begin(), col_widths.end(), 0);
		if (coln <= 1)
			return false;
		int max_line_width = 0; // tally of the longest line
		int c = 0;              // current columnt
		for (const int width : word_widths) {
			const int old_col_width = col_widths[c];
			const int new_col_width = std::max(old_col_width, width);
			col_widths[c] = new_col_width;
			max_line_width += (new_col_width - old_col_width);
			if (max_line_width >= term_width)
				return true;
			c = (c + 1) % coln;
		}
		return false;
	};

	size_t col_count = max_columns;
	while (too_many_columns(col_count)) {
		col_count--;
		col_widths.pop_back();
	}
	return col_widths;
}

char *format_date(const uint16_t year, const uint8_t month, const uint8_t day)
{
	char format_string[6];
	static char return_date_buffer[15] = {0};
	const char date_format = dos.tables.country[DOS_DATE_FORMAT_OFS];
	const char date_separator = dos.tables.country[DOS_DATE_SEPARATOR_OFS];
	int result;
	switch (date_format) {
	case 1:
		result = sprintf(format_string, "D%cM%cY", date_separator,
		                 date_separator);
		break;
	case 2:
		result = sprintf(format_string, "Y%cM%cD", date_separator,
		                 date_separator);
		break;
	default:
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

char *format_time(const uint8_t hour,
                  const uint8_t min,
                  const uint8_t sec,
                  const uint8_t msec,
                  bool full = false)
{
	uint8_t fhour = hour;
	static char return_time_buffer[19] = {0};
	char ampm[3] = "";
	char time_format = dos.tables.country[DOS_TIME_FORMAT_OFS];
	if (!time_format) { // 12 hour notation?
		if (fhour != 12)
			fhour %= 12;
		strcpy(ampm, hour != 12 && hour == fhour ? "am" : "pm");
		if (!full)
			*(ampm + 1) = 0; // "a" or "p" in short time format
	}
	const char time_separator = dos.tables.country[DOS_TIME_SEPARATOR_OFS];
	const char decimal_separator = dos.tables.country[DOS_DECIMAL_SEPARATOR_OFS];
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

void DOS_Shell::CMD_DIR(char * args) {
	HELP("DIR");

	std::string line;
	if (GetEnvStr("DIRCMD",line)){
		std::string::size_type idx = line.find('=');
		std::string value=line.substr(idx +1 , std::string::npos);
		line = std::string(args) + " " + value;
		args=const_cast<char*>(line.c_str());
	}

	bool optW=ScanCMDBool(args,"W");

	(void)ScanCMDBool(args, "S");

	bool optP=ScanCMDBool(args,"P");
	if (ScanCMDBool(args,"WP") || ScanCMDBool(args,"PW")) {
		optW=optP=true;
	}
	bool optB=ScanCMDBool(args,"B");
	bool optAD=ScanCMDBool(args,"AD");
	bool optAminusD=ScanCMDBool(args,"A-D");
	// Sorting flags
	bool reverseSort = false;
	bool optON=ScanCMDBool(args,"ON");
	if (ScanCMDBool(args,"O-N")) {
		optON = true;
		reverseSort = true;
	}
	bool optOD=ScanCMDBool(args,"OD");
	if (ScanCMDBool(args,"O-D")) {
		optOD = true;
		reverseSort = true;
	}
	bool optOE=ScanCMDBool(args,"OE");
	if (ScanCMDBool(args,"O-E")) {
		optOE = true;
		reverseSort = true;
	}
	bool optOS=ScanCMDBool(args,"OS");
	if (ScanCMDBool(args,"O-S")) {
		optOS = true;
		reverseSort = true;
	}
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}

	const std::string pattern = to_search_pattern(args);

	/* Make a full path in the args */
	char path[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(pattern.c_str(), path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}

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
	unsigned p_count = 0; // line counter for 'pause' command

	if (!optB) {
		if (print_label) {
			const auto label = To_Label(Drives.at(drive_idx)->GetLabel());
			WriteOut(MSG_Get("SHELL_CMD_DIR_VOLUME"), drive_letter,
			         label.c_str());
			p_count += 1;
		}
		WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"), path);
		WriteOut_NoParsing("\n");
		p_count += 2;
	}

	// Helper function to handle 'Press any key to continue' message
	// regardless of user-selected formatting of DIR command output.
	//
	// Call it whenever a newline gets printed to potentially display
	// this one-line message.
	//
	const int term_rows = INT10_GetTextRows() - 1;
	auto show_press_any_key = [&]() {
		p_count += 1;
		if (optP && (p_count % term_rows) == 0)
			CMD_PAUSE(empty_string);
	};

	const bool is_root = strnlen(path, sizeof(path)) == 3;

	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	bool ret = DOS_FindFirst(pattern.c_str(), 0xffff & ~DOS_ATTR_VOLUME);
	if (!ret) {
		if (!optB)
			WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"),
			         pattern.c_str());
		dos.dta(save_dta);
		return;
	}

	std::vector<DtaResult> results;
	// TODO OS should be asked for a number of files and appropriate
	// vector capacity should be set

	do {    /* File name and extension */
		DtaResult result;
		dta.GetResult(result.name,result.size,result.date,result.time,result.attr);

		/* Skip non-directories if option AD is present, or skip dirs in case of A-D */
		if (optAD && !(result.attr&DOS_ATTR_DIRECTORY) ) continue;
		else if (optAminusD && (result.attr&DOS_ATTR_DIRECTORY) ) continue;

		results.push_back(result);

	} while (DOS_FindNext());

	if (optON) {
		// Sort by name
		std::sort(results.begin(), results.end(), DtaResult::compareName);
	} else if (optOE) {
		// Sort by extension
		std::sort(results.begin(), results.end(), DtaResult::compareExt);
	} else if (optOD) {
		// Sort by date
		std::sort(results.begin(), results.end(), DtaResult::compareDate);
	} else if (optOS) {
		// Sort by size
		std::sort(results.begin(), results.end(), DtaResult::compareSize);
	}
	if (reverseSort) {
		std::reverse(results.begin(), results.end());
	}

	uint32_t byte_count = 0;
	uint32_t file_count = 0;
	uint32_t dir_count = 0;
	unsigned w_count = 0;

	for (auto &entry : results) {

		char *name = entry.name;
		const uint32_t size = entry.size;
		const uint16_t date = entry.date;
		const uint16_t time = entry.time;
		const bool is_dir = entry.attr & DOS_ATTR_DIRECTORY;

		// Skip listing . and .. from toplevel directory, to simulate
		// DIR output correctly.
		// Bare format never lists .. nor . as directories.
		if (is_root || optB) {
			if (strcmp(".", name) == 0 || strcmp("..", name) == 0)
				continue;
		}

		if (is_dir) {
			dir_count += 1;
		} else {
			file_count += 1;
			byte_count += size;
		}

		// 'Bare' format: just the name, one per line, nothing else
		//
		if (optB) {
			WriteOut("%s\n", name);
			show_press_any_key();
			continue;
		}

		// 'Wide list' format: using several columns
		//
		if (optW) {
			if (is_dir) {
				const int length = static_cast<int>(strlen(name));
				WriteOut("[%s]%*s", name, (14 - length), "");
			} else {
				WriteOut("%-16s", name);
			}
			w_count += 1;
			if ((w_count % 5) == 0)
				show_press_any_key();
			continue;
		}

		// default format: one detailed entry per line
		//
		const auto year   = static_cast<uint16_t>((date >> 9) + 1980);
		const auto month  = static_cast<uint8_t>((date >> 5) & 0x000f);
		const auto day    = static_cast<uint8_t>(date & 0x001f);
		const auto hour   = static_cast<uint8_t>((time >> 5) >> 6);
		const auto minute = static_cast<uint8_t>((time >> 5) & 0x003f);

		char *ext = empty_string;
		if ((name[0] != '.')) {
			ext = strrchr(name, '.');
			if (ext) {
				*ext = '\0';
				ext++;
			} else {
				// prevent (null) from appearing
				ext = empty_string;
			}
		}

		if (is_dir) {
			WriteOut("%-8s %-3s   %-21s %s %s\n", name, ext,
			         "<DIR>", format_date(year, month, day),
			         format_time(hour, minute, 0, 0));
		} else {
			const auto file_size = format_number(size);
			WriteOut("%-8s %-3s   %21s %s %s\n", name, ext,
			         file_size.c_str(), format_date(year, month, day),
			         format_time(hour, minute, 0, 0));
		}
		show_press_any_key();
	}

	// Additional newline in case last line in 'Wide list` format was
	// not wrapped automatically.
	if (optW && (w_count % 5)) {
		WriteOut("\n");
		show_press_any_key();
	}

	// Show the summary of results
	if (!optB) {
		const auto bytes_used = format_number(byte_count);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_USED"), file_count,
		         bytes_used.c_str());
		show_press_any_key();

		uint8_t drive = dta.GetSearchDrive();
		size_t free_space = 1024 * 1024 * 100;
		if (Drives.at(drive)) {
			uint16_t bytes_sector;
			uint8_t sectors_cluster;
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
		const auto bytes = format_number(free_space);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"), dir_count,
		         bytes.c_str());
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_LS(char *args)
{
	using namespace std::string_literals;

	HELP("LS");

	const RealPt original_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	const std::string pattern = to_search_pattern(args);
	if (!DOS_FindFirst(pattern.c_str(), 0xffff & ~DOS_ATTR_VOLUME)) {
		WriteOut(MSG_Get("SHELL_CMD_LS_PATH_ERR"), trim(args));
		dos.dta(original_dta);
		return;
	}

	std::vector<DtaResult> dir_contents;
	// reserve space for as many as we can fit into a single memory page
	// nothing more to it; make it larger if necessary
	dir_contents.reserve(MEM_PAGE_SIZE / sizeof(DtaResult));

	do {
		DtaResult result;
		dta.GetResult(result.name, result.size, result.date,
		              result.time, result.attr);
		if (result.name == "."s || result.name == ".."s)
			continue;
		dir_contents.push_back(result);
	} while (DOS_FindNext());

	const int column_sep = 2; // chars separating columns
	const auto word_widths = to_name_lengths(dir_contents, column_sep);
	const auto column_widths = calc_column_widths(word_widths, column_sep + 1);
	const size_t cols = column_widths.size();

	size_t w_count = 0;

	constexpr int ansi_blue = 34;
	constexpr int ansi_green = 32;
	auto write_color = [&](int color, const std::string &txt, int width) {
		const int padr = width - static_cast<int>(txt.size());
		WriteOut("\033[%d;1m%s\033[0m%-*s", color, txt.c_str(), padr, "");
	};

	for (const auto &entry : dir_contents) {
		std::string name = entry.name;
		const bool is_dir = entry.attr & DOS_ATTR_DIRECTORY;
		const size_t col = w_count % cols;
		const int cw = column_widths[col];

		if (is_dir) {
			upcase(name);
			write_color(ansi_blue, name, cw);
		} else {
			lowcase(name);
			if (is_executable_filename(name))
				write_color(ansi_green, name, cw);
			else
				WriteOut("%-*s", cw, name.c_str());
		}

		++w_count;
		if (w_count % cols == 0)
			WriteOut_NoParsing("\n");
	}
	dos.dta(original_dta);
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

void DOS_Shell::CMD_COPY(char * args) {
	HELP("COPY");
	static char defaulttarget[] = ".";
	StripSpaces(args);
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	uint32_t size;uint16_t date;uint16_t time;uint8_t attr;
	char name[DOS_NAMELENGTH_ASCII];
	std::vector<copysource> sources;
	// ignore /b and /t switches: always copy binary
	while (ScanCMDBool(args,"B")) ;
	while (ScanCMDBool(args,"T")) ; //Shouldn't this be A ?
	while (ScanCMDBool(args,"A")) ;

	(void)ScanCMDBool(args, "Y");
	(void)ScanCMDBool(args, "-Y");
	(void)ScanCMDBool(args, "V");

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		dos.dta(save_dta);
		return;
	}
	// Gather all sources (extension to copy more then 1 file specified at command line)
	// Concatenating files go as follows: All parts except for the last bear the concat flag.
	// This construction allows them to be counted (only the non concat set)
	char* source_p = NULL;
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
				if (DOS_FindFirst(source_p,0xffff & ~DOS_ATTR_VOLUME)) {
					dta.GetResult(name,size,date,time,attr);
					if (attr & DOS_ATTR_DIRECTORY)
						strcat(source_x,"\\*.*");
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

		if (!DOS_Canonicalize(const_cast<char*>(source.filename.c_str()),pathSource)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		// cut search pattern
		char* pos = strrchr(pathSource,'\\');
		if (pos) *(pos+1) = 0;

		if (!DOS_Canonicalize(const_cast<char*>(target.filename.c_str()),pathTarget)) {
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
			if (DOS_FindFirst(pathTarget, 0xffff & ~DOS_ATTR_VOLUME)) {
				dta.GetResult(name, size, date, time, attr);
				if (attr & DOS_ATTR_DIRECTORY) {
					strcat(pathTarget,"\\");
					target_is_file = false;
				}
			}
		} else
			target_is_file = false;

		//Find first sourcefile
		bool ret = DOS_FindFirst(const_cast<char*>(source.filename.c_str()),0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"),const_cast<char*>(source.filename.c_str()));
			dos.dta(save_dta);
			return;
		}

		uint16_t sourceHandle = 0;
		uint16_t targetHandle = 0;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH];

		bool second_file_of_current_source = false;
		while (ret) {
			dta.GetResult(name,size,date,time,attr);

			if ((attr & DOS_ATTR_DIRECTORY) == 0) {
				safe_strcpy(nameSource, pathSource);
				strcat(nameSource,name);
				// Open Source
				if (DOS_OpenFile(nameSource,0,&sourceHandle)) {
					// Create Target or open it if in concat mode
					safe_strcpy(nameTarget, pathTarget);
					const auto name_length = strlen(nameTarget);
					if (name_length > 0 && nameTarget[name_length - 1] == '\\')
						strcat(nameTarget, name);

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
								DOS_GetFileDate(sourceHandle,
								                &time,
								                &date);
								DOS_SetFileDate(targetHandle,
								                time,
								                date);
							}
							DOS_CloseFile(sourceHandle);
							DOS_CloseFile(targetHandle);
							WriteOut(" %s\n",name);
							if (!source.concat && !special) count++; //Only count concat files once
						} else {
							DOS_CloseFile(sourceHandle);
							WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
						}
					} else {
						DOS_CloseFile(sourceHandle);
						WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
					}
				} else WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(source.filename.c_str()));
			};
			//On to the next file if the previous one wasn't a device
			if ((attr&DOS_ATTR_DEVICE) == 0) ret = DOS_FindNext();
			else ret = false;
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

static void show_attributes(DOS_Shell *shell, const uint16_t fattr, const char *name) {
	const bool attr_a = fattr & DOS_ATTR_ARCHIVE;
	const bool attr_s = fattr & DOS_ATTR_SYSTEM;
	const bool attr_h = fattr & DOS_ATTR_HIDDEN;
	const bool attr_r = fattr & DOS_ATTR_READ_ONLY;
	shell->WriteOut("  %c  %c%c%c	%s\n",
			attr_a ? 'A' : ' ',
			attr_h ? 'H' : ' ',
			attr_s ? 'S' : ' ',
			attr_r ? 'R' : ' ', name);
}

char *get_filename(char *args)
{
	static char *fname = strrchr(args, '\\');
	if (fname != NULL)
		fname++;
	else {
		fname = strrchr(args, ':');
		if (fname != NULL)
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
	if (!DOS_Canonicalize(args, full) || strrchr(full, '\\') == NULL) {
		shell->WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return false;
	}
	bool found = false, res = DOS_FindFirst(args, 0xffff & ~DOS_ATTR_VOLUME);
	if (!res && !optS)
		return false;
	char *end = strrchr(full, '\\');
	if (!end)
		return false;
	end++;
	*end = 0;
	strcpy(path, full);
	char name[DOS_NAMELENGTH_ASCII];
	uint32_t size;
	uint16_t date;
	uint16_t time;
	uint8_t attr;
	uint16_t fattr;
	while (res) {
		dta.GetResult(name, size, date, time, attr);
		if (!((!strcmp(name, ".") || !strcmp(name, "..") ||
		       strchr(args, '*') != NULL || strchr(args, '?') != NULL) &&
		      attr & DOS_ATTR_DIRECTORY)) {
			found = true;
			strcpy(end, name);
			if (!*full || !DOS_GetFileAttr(full, &fattr)) {
				shell->WriteOut(MSG_Get("SHELL_CMD_ATTRIB_GET_ERROR"),
				                full);
			} else if (attribs.add_a || attribs.add_s || attribs.add_h ||
			           attribs.add_r || attribs.min_a || attribs.min_s ||
			           attribs.min_h || attribs.min_r) {
				fattr |= (attribs.add_a ? DOS_ATTR_ARCHIVE : 0);
				fattr |= (attribs.add_s ? DOS_ATTR_SYSTEM : 0);
				fattr |= (attribs.add_h ? DOS_ATTR_HIDDEN : 0);
				fattr |= (attribs.add_r ? DOS_ATTR_READ_ONLY : 0);
				fattr &= (attribs.min_a ? ~DOS_ATTR_ARCHIVE : 0xffff);
				fattr &= (attribs.min_s ? ~DOS_ATTR_SYSTEM : 0xffff);
				fattr &= (attribs.min_h ? ~DOS_ATTR_HIDDEN : 0xffff);
				fattr &= (attribs.min_r ? ~DOS_ATTR_READ_ONLY : 0xffff);
				if (DOS_SetFileAttr(full, fattr) &&
				    DOS_GetFileAttr(full, &fattr))
					show_attributes(shell, fattr, full);
				else
					shell->WriteOut(MSG_Get("SHELL_CMD_ATTRIB_SET_ERROR"),
					                full);
			} else {
				show_attributes(shell, fattr, full);
			}
		}
		res = DOS_FindNext();
	}
	if (optS) {
		size_t len = strlen(path);
		strcat(path, "*.*");
		bool ret = DOS_FindFirst(path, 0xffff & ~DOS_ATTR_VOLUME);
		*(path + len) = 0;
		if (ret) {
			std::vector<std::string> found_dirs;
			found_dirs.clear();
			do { /* File name and extension */
				DtaResult result;
				dta.GetResult(result.name, result.size, result.date,
				              result.time, result.attr);
				if ((result.attr & DOS_ATTR_DIRECTORY) &&
				    strcmp(result.name, ".") &&
				    strcmp(result.name, "..")) {
					std::string fullname = result.name +
					                       std::string(1, '\\') +
					                       get_filename(args);
					found_dirs.push_back(fullname);
					strcpy(path, fullname.c_str());
					*(path + len) = 0;
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

	bool optS = ScanCMDBool(args, "S");
	char *rem = ScanCMDRemain(args);
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
	RealPt save_dta = dos.dta();
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
	std::string line;
	if (!*args) {
		/* No command line show all environment lines */
		Bitu count=GetEnvCount();
		for (Bitu a=0;a<count;a++) {
			if (GetEnvNum(a,line)) WriteOut("%s\n",line.c_str());
		}
		return;
	}
	//There are args:
	char * pcheck = args;
	while ( *pcheck && (*pcheck == ' ' || *pcheck == '\t')) pcheck++;
	if (*pcheck && strlen(pcheck) >3 && (strncasecmp(pcheck,"/p ",3) == 0)) E_Exit("Set /P is not supported. Use Choice!");

	char * p=strpbrk(args, "=");
	if (!p) {
		if (!GetEnvStr(args,line)) WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),args);
		WriteOut("%s\n",line.c_str());
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
				std::string temp;
				if (GetEnvStr(p,temp)) {
					std::string::size_type equals = temp.find('=');
					if (equals == std::string::npos)
						continue;
					const uintptr_t remaining_len = std::min(
					        sizeof(parsed) - static_cast<uintptr_t>(p_parsed - parsed),
					        sizeof(parsed));
					safe_strncpy(p_parsed,
					             temp.substr(equals + 1).c_str(),
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

void DOS_Shell::CMD_IF(char * args) {
	HELP("IF");
	StripSpaces(args,'=');
	bool has_not=false;

	while (strncasecmp(args,"NOT",3) == 0) {
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
			RealPt save_dta=dos.dta();
			dos.dta(dos.tables.tempdta);
			bool ret=DOS_FindFirst(word,0xffff & ~DOS_ATTR_VOLUME);
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
	if (!bf) return;
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
	assert(bf);
	if (!bf->Goto(args)) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_LABEL_NOT_FOUND"),args);
		bf.reset();
		return;
	}
}

void DOS_Shell::CMD_SHIFT(char * args ) {
	HELP("SHIFT");
	if (bf) bf->Shift();
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
	const char date_format = dos.tables.country[DOS_DATE_FORMAT_OFS];
	const char date_separator = dos.tables.country[DOS_DATE_SEPARATOR_OFS];
	char format[11];
	int result;
	switch (date_format) {
	case 1:
		result = safe_sprintf(format, "DD%cMM%cYYYY", date_separator,
		                      date_separator);
		break;
	case 2:
		result = safe_sprintf(format, "YYYY%cMM%cDD", date_separator,
		                      date_separator);
		break;
	default:
		result = safe_sprintf(format, "MM%cDD%cYYYY", date_separator,
		                      date_separator);
	}
	if (result < 0) {
		LOG_WARNING("SHELL: Incorrect date format");
		return;
	}
	if (ScanCMDBool(args, "?")) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_DATE_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("SHELL_CMD_DATE_HELP_LONG"),
		                 format,
		                 format_date(2012, 10, 11));
		output.Display();
		return;
	}
	if (ScanCMDBool(args, "H")) {
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
	case 1:
		n = sscanf(args, "%u%c%u%c%u", &newday, &date_separator_placeholder_1,
		           &newmonth, &date_separator_placeholder_2, &newyear);
		break;
	case 2:
		n = sscanf(args, "%u%c%u%c%u", &newyear, &date_separator_placeholder_1,
		           &newmonth, &date_separator_placeholder_2, &newday);
		break;
	default:
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
	bool dateonly = ScanCMDBool(args, "T");
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
	const char time_separator = dos.tables.country[DOS_TIME_SEPARATOR_OFS];
	sprintf(format, "hh%cmm%css", time_separator, time_separator);
	sprintf(example, "13%c14%c15", time_separator, time_separator);
	if (ScanCMDBool(args, "?")) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_TIME_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("SHELL_CMD_TIME_HELP_LONG"), format, example);
		output.Display();
		return;
	}
	if (ScanCMDBool(args, "H")) {
		// synchronize time with host
		const time_t curtime = time(NULL);
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
	bool timeonly = ScanCMDBool(args, "T");

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
	localDrive* ldp=0;
	char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
	char temp_str[2] = { 0,0 };
	try {
		safe_strcpy(mountstring, "MOUNT ");
		StripSpaces(args);
		std::string arg;
		CommandLine command(0,args);

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
		if (!DOS_MakeName(const_cast<char*>(arg.c_str()),fulldir,&drive)) throw 0;

		ldp = dynamic_cast<localDrive*>(Drives.at(drive));
		if (!ldp) {
			throw 0;
		}
		char newname[CROSS_LEN];
		safe_strcpy(newname, ldp->GetBasedir());
		strcat(newname,fulldir);
		CROSS_FILENAME(newname);
		ldp->dirCache.ExpandName(newname);
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
void MAPPER_AutoTypeStopImmediately();
void DOS_21Handler();

void DOS_Shell::CMD_CHOICE(char * args){
	HELP("CHOICE");

	// Parse "/n"; does the user want to show choices or not?
	const bool should_show_choices = !ScanCMDBool(args, "N");

	// Parse "/s"; does the user want choices to be case-sensitive?
	const bool always_capitalize = !ScanCMDBool(args, "S");

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
			MAPPER_AutoTypeStopImmediately();
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
		std::string line;
		if (GetEnvStr("PATH", line))
			WriteOut("%s\n", line.c_str());
		else
			WriteOut("PATH=(null)\n");
	}
}

void DOS_Shell::CMD_VER(char *args)
{
	HELP("VER");
	if (args && strlen(args)) {
		char *word = strip_word(args);
		if (strcasecmp(word, "set"))
			return;
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
