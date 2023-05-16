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

#include "shell.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <regex>

#include "regs.h"
#include "callback.h"
#include "string_utils.h"
#include "../ints/int10.h"

[[nodiscard]] static std::vector<std::string> get_completions(std::string_view command);

namespace {
class CommandPrompt {
public:
	CommandPrompt();
	void Update(std::string_view command, std::string::size_type cursor);
	void Newline();

	[[nodiscard]] static uint32_t MaxCommandSize();

private:
	struct CursorPosition {
		uint8_t page   = 0;
		uint8_t row    = 0;
		uint8_t column = 0;
	};
	CursorPosition position_zero = {};

	std::string::size_type previous_size = 0;

	static void outc(uint8_t c);
	void SetCurrentPositionToZero();
	void SetCursor(std::string::size_type index);
};
} // namespace

DOS_Shell::~DOS_Shell() {
	bf.reset();
}

void DOS_Shell::ShowPrompt(void) {
	uint8_t drive=DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	reset_str(dir); // DOS_GetCurrentDir doesn't always return
	                // something. (if drive is messed up)
	DOS_GetCurrentDir(0, dir);
	InjectMissingNewline();
	WriteOut("%c:\\%s>", drive, dir);
	ResetLastWrittenChar('\n'); // prevents excessive newline if cmd prints nothing
}

void DOS_Shell::InputCommand(char* line)
{
	std::string command = ReadCommand();
	history.emplace_back(command);

	std::strncpy(line, command.c_str(), CMD_MAXLINE);
	line[CMD_MAXLINE - 1] = '\0';

	const auto* const dos_section = dynamic_cast<Section_prop*>(
	        control->GetSection("dos"));
	assert(dos_section != nullptr);
	const std::string_view expand_shell_variable_pref = dos_section->Get_string(
	        "expand_shell_variable");
	if ((expand_shell_variable_pref == "auto" && dos.version.major >= 7) ||
	    expand_shell_variable_pref == "true") {
		ProcessCmdLineEnvVarStitution(line);
	}
}

std::string DOS_Shell::ReadCommand()
{
	std::vector<std::string> history_clone = history;
	history_clone.emplace_back("");
	auto history_index = history_clone.size() - 1;

	std::vector<std::string> completion = {};

	auto completion_index = completion.size();

	std::string command   = {};
	auto cursor_position  = command.size();
	auto completion_start = command.size();

	uint8_t data        = 0;
	uint16_t byte_count = 1;

	CommandPrompt prompt;

	while (!shutdown_requested) {
		assert(history_index < history_clone.size());
		assert(completion.empty() || completion_index < completion.size());
		assert(command.empty() || cursor_position <= command.size());
		assert(command.empty() || completion_start <= command.size());

		bool viewing_tab_completions = false;

		while (!DOS_ReadFile(input_handle, &data, &byte_count)) {
			uint16_t dummy = 1;
			DOS_CloseFile(input_handle);
			DOS_OpenFile("con", 2, &dummy);
			LOG(LOG_MISC, LOG_ERROR)
			("Reopening the input handle. This is a bug!");
		}

		if (byte_count == 0) {
			break;
		}

		constexpr decltype(data) ExtendedKey = 0x00;
		constexpr decltype(data) Escape      = 0x1B;
		switch (data) {
		case ExtendedKey: {
			DOS_ReadFile(input_handle, &data, &byte_count);

			constexpr uint8_t ShiftPlusTab = 0x0F;
			constexpr uint8_t F3           = 0x3D;
			constexpr uint8_t Home         = 0x47;
			constexpr uint8_t Up           = 0x48;
			constexpr uint8_t Left         = 0x4B;
			constexpr uint8_t Right        = 0x4D;
			constexpr uint8_t End          = 0x4F;
			constexpr uint8_t Down         = 0x50;
			constexpr uint8_t Delete       = 0x53;
			switch (data) {
			case F3: {
				if (history.empty()) {
					break;
				}

				std::string_view last_command = history.back();
				if (last_command.size() <= command.size()) {
					break;
				}

				last_command = last_command.substr(command.size());
				command += last_command;
				cursor_position = command.size();
				break;
			}

			case Left:
				if (cursor_position > 0) {
					--cursor_position;
				}
				break;

			case Right:
				if (cursor_position < command.size()) {
					++cursor_position;
				}
				break;

			case Up:
				if (history_index > 0) {
					history_clone[history_index] = command;
					--history_index;
					command = history_clone[history_index];
					cursor_position = command.size();
				}
				break;

			case Down:
				if (history_index + 1 < history_clone.size()) {
					history_clone[history_index] = command;
					++history_index;
					command = history_clone[history_index];
					cursor_position = command.size();
				}
				break;

			case Home: cursor_position = 0; break;

			case End: cursor_position = command.size(); break;

			case Delete: command.erase(cursor_position, 1); break;

			case ShiftPlusTab:
				if (completion.empty()) {
					break;
				}
				if (completion_index == 0) {
					completion_index = completion.size();
				}
				--completion_index;
				command.erase(completion_start);
				command += completion[completion_index];
				cursor_position         = command.size();
				viewing_tab_completions = true;
				break;

			default: break;
			}
			break;
		}

		case '\t': {
			if (!completion.empty()) {
				++completion_index;
				if (completion_index == completion.size()) {
					completion_index = 0;
				}
			} else {
				completion = get_completions(command);
				if (completion.empty()) {
					break;
				}
				completion_index = 0;

				const auto delimiter = command.find_last_of(" \\");
				if (delimiter == std::string::npos) {
					completion_start = 0;
				} else {
					completion_start = delimiter + 1;
				}
			}

			command.erase(completion_start);
			command += completion[completion_index];
			cursor_position         = command.size();
			viewing_tab_completions = true;
			break;
		}

		case '\b':
			if (cursor_position > 0) {
				command.erase(cursor_position - 1, 1);
				--cursor_position;
			}
			break;

		case '\n': break;
		case '\r': prompt.Newline(); return command;

		case Escape:
			command += "\\";
			prompt.Update(command, cursor_position);
			prompt.Newline();
			command.clear();
			cursor_position = 0;
			break;

		default:
			if (CommandPrompt::MaxCommandSize() < command.size()) {
				break;
			}
			command.insert(cursor_position, 1, static_cast<char>(data));
			++cursor_position;
			break;
		}

		prompt.Update(command, cursor_position);

		if (!viewing_tab_completions) {
			completion.clear();
		}
	}

	return "";
}

static std::vector<std::string> get_completions(const std::string_view command)
{
	const std::vector<std::string> args = split(command);

	const bool dir_only = (!args.empty() && iequals(args[0], "CD") &&
	                       (args.size() > 1 || command.back() == ' '));

	std::string search = {};
	if (!args.empty() && command.back() != ' ') {
		search += args.back();
	}
	search += "*.*";

	const auto save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	const auto dta = DOS_DTA(dos.dta());

	bool res = DOS_FindFirst(search.c_str(), ~DOS_ATTR_VOLUME);

	std::vector<std::string> files           = {};
	std::vector<std::string> non_executables = {};
	while (res) {
		DOS_DTA::Result result = {};
		dta.GetResult(result);

		if (!result.IsDummyDirectory() &&
		    (!dir_only || result.IsDirectory())) {
			if (is_executable_filename(result.name)) {
				files.emplace_back(std::move(result.name));
			} else {
				non_executables.emplace_back(std::move(result.name));
			}
		}

		res = DOS_FindNext();
	}

	dos.dta(save_dta);

	files.insert(files.end(),
	             std::make_move_iterator(non_executables.begin()),
	             std::make_move_iterator(non_executables.end()));
	return files;
}

/* Note: Buffer pointed to by "line" must be at least CMD_MAXLINE+1 bytes long! */
void DOS_Shell::ProcessCmdLineEnvVarStitution(char* line) {
	constexpr char surrogate_percent = 8;
	const static std::regex re("\\%([^%0-9][^%]*)?%");
	std::string text = line;
	std::smatch match;
	/* Iterate over potential %var1%, %var2%, etc matches found in the text string */
	while (std::regex_search(text, match, re)) {
		// Get the first matching %'s position and length
		const auto percent_pos = static_cast<size_t>(match.position(0));
		const auto percent_len = static_cast<size_t>(match.length(0));

		std::string variable_name = match[1].str();
		if (variable_name.empty()) {
			/* Replace %% with the character "surrogate_percent", then (eventually) % */
			text.replace(percent_pos, percent_len,
			             std::string(1, surrogate_percent));
			continue;
		}
		/* Trim preceding spaces from the variable name */
		variable_name.erase(0, variable_name.find_first_not_of(' '));
		std::string variable_value;
		if (variable_name.size() && GetEnvStr(variable_name.c_str(), variable_value)) {
			const size_t equal_pos = variable_value.find_first_of('=');
			/* Replace the original %var% with its corresponding value from the environment */
			const std::string replacement = equal_pos != std::string::npos
			                ? variable_value.substr(equal_pos + 1) : "";
			text.replace(percent_pos, percent_len, replacement);
		}
		else {
			text.replace(percent_pos, percent_len, "");
		}
	}
	std::replace(text.begin(), text.end(), surrogate_percent, '%');
	assert(text.size() <= CMD_MAXLINE);
	safe_strncpy(line, text.c_str(), CMD_MAXLINE);
}

CommandPrompt::CommandPrompt()
{
	SetCurrentPositionToZero();
}

uint32_t CommandPrompt::MaxCommandSize()
{
	// This size is somewhat arbitrary. It is the number of characters
	// needed to fill a whole screen minus two rows. This extra space is
	// to allow the prompt to also fit when deep in the filesystem.
	// A larger maximum command size would require complex
	// scrolling behaviour to work as intended. This is likely
	// uneeded as a user will not likely write commands that large.

	return INT10_GetTextColumns() * (INT10_GetTextRows() - 2);
}

void CommandPrompt::Update(const std::string_view command,
                           const std::string::size_type cursor)
{
	uint16_t byte_count = 1;
	SetCursor(0);
	for (uint8_t c : command) {
		DOS_WriteFile(STDOUT, &c, &byte_count);
	}

	if (previous_size > command.size()) {
		auto space_count = previous_size - command.size();
		for (decltype(space_count) i = 0; i < space_count; ++i) {
			outc(' ');
		}
	}

	SetCursor(cursor);
	previous_size = command.size();
}

void CommandPrompt::Newline()
{
	outc('\r');
	outc('\n');
	SetCurrentPositionToZero();
}

void CommandPrompt::outc(uint8_t c)
{
	uint16_t n = 1;
	DOS_WriteFile(STDOUT, &c, &n);
}

void CommandPrompt::SetCurrentPositionToZero()
{
	position_zero.page   = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	position_zero.row    = CURSOR_POS_ROW(position_zero.page);
	position_zero.column = CURSOR_POS_COL(position_zero.page);
}

void CommandPrompt::SetCursor(const std::string::size_type index)
{
	auto ncols  = INT10_GetTextColumns();
	auto nrows  = INT10_GetTextRows();
	auto column = position_zero.column + index;
	auto row    = position_zero.row + column / ncols;
	column %= ncols;
	if (row >= nrows) {
		position_zero.row -= static_cast<uint8_t>(row - nrows + 1);
		row = nrows - 1;
	}
	INT10_SetCursorPos(static_cast<uint8_t>(row),
	                   static_cast<uint8_t>(column),
	                   position_zero.page);
}

std::string full_arguments = "";
bool DOS_Shell::Execute(char * name,char * args) {
/* return true  => don't check for hardware changes in do_command 
 * return false =>       check for hardware changes in do_command */
	char fullname[DOS_PATHLENGTH+4]; //stores results from Which
	char line[CMD_MAXLINE];
	const bool have_args = args && args[0] != '\0';
	if (have_args) {
		if (*args != ' ') { // put a space in front
			line[0] = ' ';
			terminate_str_at(line, 1);
			strncat(line, args, CMD_MAXLINE - 2);
			terminate_str_at(line, CMD_MAXLINE - 1);
		} else {
			safe_strcpy(line, args);
		}
	} else {
		reset_str(line);
	}

	/* check for a drive change */
	if (((strcmp(name + 1, ":") == 0) || (strcmp(name + 1, ":\\") == 0)) && isalpha(*name))
	{
		const auto drive_idx = drive_index(name[0]);
		if (!DOS_SetDrive(drive_idx)) {
			WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),
			         toupper(name[0]));
		}
		return true;
	}

	/* Check for a full name */
	const char *p_fullname = Which(name);
	if (!p_fullname)
		return false;
	safe_strcpy(fullname, p_fullname);

	// Always disallow files without extension from being executed.
	// Only internal commands can be run this way and they never get in
	// this handler.
	const char *extension = strrchr(fullname, '.');
	if (!extension) {
		// Check if the result will fit in the parameters.
		if (safe_strlen(fullname) > (DOS_PATHLENGTH - 1)) {
			WriteOut(MSG_Get("PROGRAM_PATH_TOO_LONG"), fullname,
			         DOS_PATHLENGTH);
			return false;
		}

		// Try to add .COM, .EXE and .BAT extensions to the filename
		char temp_name[DOS_PATHLENGTH + 4];
		for (const char *ext : {".COM", ".EXE", ".BAT"}) {
			safe_sprintf(temp_name, "%s%s", fullname, ext);
			const char *temp_fullname = Which(temp_name);
			if (temp_fullname) {
				extension = ext;
				safe_strcpy(fullname, temp_fullname);
				break;
			}
		}

		if (!extension)
			return false;
	}

	if (strcasecmp(extension, ".bat") == 0) 
	{	/* Run the .bat file */
		/* delete old batch file if call is not active*/
		bool temp_echo=echo; /*keep the current echostate (as delete bf might change it )*/
		if (bf && !call)
			bf.reset();
		bf = std::make_shared<BatchFile>(this, fullname, name, line);
		echo = temp_echo; // restore it.
	} 
	else 
	{	/* only .bat .exe .com extensions maybe be executed by the shell */
		if(strcasecmp(extension, ".com") !=0) 
		{
			if(strcasecmp(extension, ".exe") !=0) return false;
		}
		/* Run the .exe or .com file from the shell */
		/* Allocate some stack space for tables in physical memory */
		reg_sp-=0x200;
		//Add Parameter block
		DOS_ParamBlock block(SegPhys(ss)+reg_sp);
		block.Clear();
		//Add a filename
		RealPt file_name=RealMakeSeg(ss,reg_sp+0x20);
		MEM_BlockWrite(RealToPhysical(file_name), fullname,
		               (Bitu)(safe_strlen(fullname) + 1));

		/* HACK: Store full commandline for mount and imgmount */
		full_arguments.assign(line);

		/* Fill the command line */
		CommandTail cmdtail = {};

		// copy at-most 126 chracters plus the terminating zero
		safe_strcpy(cmdtail.buffer, line);

		cmdtail.count = check_cast<uint8_t>(safe_strlen(cmdtail.buffer));
		terminate_str_at(line, cmdtail.count);

		assert(cmdtail.count < sizeof(cmdtail.buffer));
		cmdtail.buffer[cmdtail.count] = 0xd;

		/* Copy command line in stack block too */
		MEM_BlockWrite(SegPhys(ss)+reg_sp+0x100,&cmdtail,128);

		
		/* Split input line up into parameters, using a few special rules, most notable the one for /AAA => A\0AA
		 * Qbix: It is extremly messy, but this was the only way I could get things like /:aa and :/aa to work correctly */
		
		//Prepare string first
		char parseline[258] = { 0 };
		for(char *pl = line,*q = parseline; *pl ;pl++,q++) {
			if (*pl == '=' || *pl == ';' || *pl ==',' || *pl == '\t' || *pl == ' ') 
				reset_str(q);
			else
				*q = *pl; //Replace command seperators with 0.
		} //No end of string \0 needed as parseline is larger than line

		for(char* p = parseline; (p-parseline) < 250 ;p++) { //Stay relaxed within boundaries as we have plenty of room
			if (*p == '/') { //Transform /Hello into H\0ello
				reset_str(p);
				p++;
				while ( *p == 0 && (p-parseline) < 250) p++; //Skip empty fields
				if ((p-parseline) < 250) { //Found something. Lets get the first letter and break it up
					p++;
					memmove(static_cast<void*>(p + 1),static_cast<void*>(p),(250-(p-parseline)));
					if ((p - parseline) < 250)
						reset_str(p);
				}
			}
		}
		// Just to be safe
		terminate_str_at(parseline, 255);
		terminate_str_at(parseline, 256);
		terminate_str_at(parseline, 257);

		/* Parse FCB (first two parameters) and put them into the current DOS_PSP */
		uint8_t add;
		uint16_t skip = 0;
		//find first argument, we end up at parseline[256] if there is only one argument (similar for the second), which exists and is 0.
		while(skip < 256 && parseline[skip] == 0) skip++;
		FCB_Parsename(dos.psp(),0x5C,0x01,parseline + skip,&add);
		skip += add;
		
		//Move to next argument if it exists
		while(parseline[skip] != 0) skip++;  //This is safe as there is always a 0 in parseline at the end.
		while(skip < 256 && parseline[skip] == 0) skip++; //Which is higher than 256
		FCB_Parsename(dos.psp(),0x6C,0x01,parseline + skip,&add); 

		block.exec.fcb1=RealMake(dos.psp(),0x5C);
		block.exec.fcb2=RealMake(dos.psp(),0x6C);
		/* Set the command line in the block and save it */
		block.exec.cmdtail=RealMakeSeg(ss,reg_sp+0x100);
		block.SaveData();
#if 0
		/* Save CS:IP to some point where i can return them from */
		uint32_t oldeip=reg_eip;
		uint16_t oldcs=SegValue(cs);
		RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
		SegSet16(cs,RealSeg(newcsip));
		reg_ip=RealOffset(newcsip);
#endif
		/* Start up a dos execute interrupt */
		reg_ax=0x4b00;
		//Filename pointer
		SegSet16(ds,SegValue(ss));
		reg_dx=RealOffset(file_name);
		//Paramblock
		SegSet16(es,SegValue(ss));
		reg_bx=reg_sp;
		SETFLAGBIT(IF,false);
		CALLBACK_RunRealInt(0x21);
		/* Restore CS:IP and the stack */
		reg_sp+=0x200;
#if 0
		reg_eip=oldeip;
		SegSet16(cs,oldcs);
#endif
	}
	return true; //Executable started
}

static char which_ret[DOS_PATHLENGTH+4];

const char *DOS_Shell::Which(const char *name) const
{
	const size_t name_len = strlen(name);
	if (name_len >= DOS_PATHLENGTH)
		return 0;

	/* Parse through the Path to find the correct entry */
	/* Check if name is already ok but just misses an extension */

	if (DOS_FileExists(name))
		return name;

	/* try to find .com .exe .bat */
	for (const char *ext_fmt : {"%s.COM", "%s.EXE", "%s.BAT"}) {
		safe_sprintf(which_ret, ext_fmt, name);
		if (DOS_FileExists(which_ret))
			return which_ret;
	}

	/* No Path in filename look through path environment string */
	char path[DOS_PATHLENGTH];std::string temp;
	if (!GetEnvStr("PATH",temp)) return 0;
	const char * pathenv=temp.c_str();

	assert(pathenv);
	pathenv = strchr(pathenv,'=');
	if (!pathenv) return 0;
	pathenv++;
	Bitu i_path = 0;
	while (*pathenv) {
		/* remove ; and ;; at the beginning. (and from the second entry etc) */
		while(*pathenv == ';')
			pathenv++;

		/* get next entry */
		i_path = 0; /* reset writer */
		while(*pathenv && (*pathenv !=';') && (i_path < DOS_PATHLENGTH) )
			path[i_path++] = *pathenv++;

		if(i_path == DOS_PATHLENGTH) {
			/* If max size. move till next ; and terminate path */
			while(*pathenv && (*pathenv != ';')) 
				pathenv++;
			terminate_str_at(path, DOS_PATHLENGTH - 1);
		} else {
			terminate_str_at(path, i_path);
		}

		/* check entry */
		if (size_t len = safe_strlen(path)) {
			if(len >= (DOS_PATHLENGTH - 2)) continue;

			if(path[len - 1] != '\\') {
				safe_strcat(path, "\\");
				len++;
			}

			//If name too long =>next
			if((name_len + len + 1) >= DOS_PATHLENGTH) continue;

			safe_strcat(path, name);
			safe_strcpy(which_ret, path);
			if (DOS_FileExists(which_ret))
				return which_ret;

			for (const char *ext_fmt : {"%s.COM", "%s.EXE", "%s.BAT"}) {
				safe_sprintf(which_ret, ext_fmt, path);
				if (DOS_FileExists(which_ret))
					return which_ret;
			}
		}
	}
	return 0;
}
