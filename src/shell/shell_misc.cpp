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

#include "regs.h"
#include "callback.h"
#include "string_utils.h"
#include "../ints/int10.h"

[[nodiscard]] static std::vector<std::string> get_completions(std::string_view command);
static void run_binary_executable(std::string_view fullname, std::string_view args);

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

	const auto* const dos_section = dynamic_cast<Section_prop*>(
	        control->GetSection("dos"));
	assert(dos_section != nullptr);
	const std::string_view expand_shell_variable_pref = dos_section->Get_string(
	        "expand_shell_variable");
	if ((expand_shell_variable_pref == "auto" && dos.version.major >= 7) ||
	    expand_shell_variable_pref == "true") {
		command = SubstituteEnvironmentVariables(command);
	}

	std::strncpy(line, command.c_str(), CMD_MAXLINE);
	line[CMD_MAXLINE - 1] = '\0';
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

std::string DOS_Shell::SubstituteEnvironmentVariables(std::string_view command)
{
	std::string expanded = {};

	auto percent_index = command.find_first_of('%');
	while (percent_index != std::string::npos) {
		expanded += command.substr(0, percent_index);
		command              = command.substr(percent_index);
		auto closing_percent = command.substr(1).find_first_of('%');

		if (closing_percent == std::string::npos) {
			break;
		}
		closing_percent += 1;

		const std::string env_key(command.substr(1, closing_percent - 1));
		if (std::string env_val = {}; GetEnvStr(env_key.c_str(), env_val)) {
			expanded += env_val.substr(env_key.length() + sizeof('='));

			command = command.substr(closing_percent + 1);
		} else {
			expanded += command.substr(0, closing_percent);
			command = command.substr(closing_percent);
		}

		percent_index = command.find_first_of('%');
	}

	expanded += command;
	return expanded;
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

bool DOS_Shell::Execute(std::string_view name, std::string_view args)
{
	if (name.size() > 1 && (std::isalpha(name[0]) != 0) &&
	    (name.substr(1) == ":" || name.substr(1) == ":\\")) {
		const auto drive_idx = drive_index(name[0]);
		if (!DOS_SetDrive(drive_idx)) {
			WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),
			         toupper(name[0]));
		}
		return true;
	}

	const auto fullname                                = Which(name);
	constexpr decltype(fullname.size()) extension_size = 4;

	if (fullname.empty() || fullname.size() <= extension_size) {
		return false;
	}

	auto extension = fullname.substr(fullname.size() - extension_size);

	if (iequals(extension, ".BAT")) {
		/* delete old batch file if call is not active*/
		/*keep the current echostate (as delete bf might change it )*/
		const bool temp_echo = echo;
		if (bf && !call) {
			bf.reset();
		}
		bf   = std::make_shared<BatchFile>(this,
                                                 fullname.c_str(),
                                                 std::string(name).c_str(),
                                                 std::string(args).c_str());
		echo = temp_echo;
		return true;
	}

	if (iequals(extension, ".COM") || iequals(extension, ".EXE")) {
		run_binary_executable(fullname, args);
		return true;
	}

	return false;
}

std::string DOS_Shell::Which(const std::string_view name) const
{
	static constexpr auto extensions = {"", ".COM", ".EXE", ".BAT"};

	std::vector<std::string> prefixes = {""};
	std::string path_environment;
	GetEnvStr("PATH", path_environment);
	const auto path_equals = path_environment.find_first_of('=');

	if (path_equals != std::string::npos) {
		path_environment = path_environment.substr(path_equals + 1);
		auto path_directories = split(path_environment, ';');

		prefixes.insert(prefixes.end(),
		                std::make_move_iterator(path_directories.begin()),
		                std::make_move_iterator(path_directories.end()));
	}

	for (const auto& prefix : prefixes) {
		for (const auto& extension : extensions) {
			std::string file = prefix + std::string(name) + extension;
			if (DOS_FileExists(file.c_str())) {
				return file;
			}
		}
	}

	return "";
}

std::string full_arguments = "";
// TODO De-mystify magic numbers and verify logical correctness
static void run_binary_executable(const std::string_view fullname,
                                  std::string_view args)
{
	/* Run the .exe or .com file from the shell */
	/* Allocate some stack space for tables in physical memory */
	reg_sp -= 0x200;
	// Add Parameter block
	DOS_ParamBlock block(SegPhys(ss) + reg_sp);
	block.Clear();

	// Add a filename
	const RealPt file_name = RealMakeSeg(ss, reg_sp + 0x20);
	MEM_BlockWrite(RealToPhysical(file_name),
	               std::string(fullname).c_str(),
	               fullname.size() + 1);

	/* HACK: Store full commandline for mount and imgmount */
	full_arguments.assign(args);

	/* Fill the command line */
	CommandTail cmdtail                    = {};
	constexpr auto max_cmdtail_buffer_size = 126;
	std::copy_n(args.begin(), max_cmdtail_buffer_size, cmdtail.buffer);

	cmdtail.count = args.size() > max_cmdtail_buffer_size
	                      ? max_cmdtail_buffer_size
	                      : static_cast<uint8_t>(args.size());

	cmdtail.buffer[cmdtail.count] = '\r';

	/* Copy command line in stack block too */
	MEM_BlockWrite(SegPhys(ss) + reg_sp + 0x100, &cmdtail, 128);

	/* Split input line up into parameters, using a few special rules, most
	 * notable the one for /AAA => A\0AA Qbix: It is extremly messy, but
	 * this was the only way I could get things like /:aa and :/aa to work
	 * correctly */
	std::array<std::string, 2> fcb_args;
	decltype(fcb_args.size()) fcb_index = 0;

	constexpr auto separators = "=;,\t /";
	auto arg_index            = args.find_first_not_of(separators);
	while (arg_index != std::string::npos && fcb_index < fcb_args.size()) {
		args = args.substr(arg_index);

		if (args.size() > 1 && args[0] == '/') {
			fcb_args[fcb_index] = args[1];
			++fcb_index;
			args = args.substr(2);
		}

		else {
			auto next_separator = args.find_first_not_of(separators);
			fcb_args[fcb_index] = args.substr(0, next_separator);
			++fcb_index;
			args = args.substr(next_separator + 1);
		}
	}

	/* Parse FCB (first two parameters) and put them into the current
	 * DOS_PSP */
	uint8_t add = 0;
	FCB_Parsename(dos.psp(), 0x5C, 0x01, fcb_args[0].c_str(), &add);
	FCB_Parsename(dos.psp(), 0x6C, 0x01, fcb_args[1].c_str(), &add);

	block.exec.fcb1 = RealMake(dos.psp(), 0x5C);
	block.exec.fcb2 = RealMake(dos.psp(), 0x6C);

	/* Set the command line in the block and save it */
	block.exec.cmdtail = RealMakeSeg(ss, reg_sp + 0x100);
	block.SaveData();

	/* Start up a dos execute interrupt */
	reg_ax = 0x4b00;

	// Filename pointer
	SegSet16(ds, SegValue(ss));
	reg_dx = RealOffset(file_name);

	// Paramblock
	SegSet16(es, SegValue(ss));
	reg_bx = reg_sp;
	SETFLAGBIT(IF, false);
	CALLBACK_RunRealInt(0x21);

	/* Restore CS:IP and the stack */
	reg_sp += 0x200;
}
