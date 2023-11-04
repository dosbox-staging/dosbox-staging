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
#include <cstring>
#include <memory>

#include "../ints/int10.h"
#include "callback.h"
#include "file_reader.h"
#include "keyboard.h"
#include "regs.h"
#include "string_utils.h"

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

void DOS_Shell::ShowPrompt()
{
	const uint8_t drive = DOS_GetDefaultDrive() + 'A';
	char dir[DOS_PATHLENGTH];
	reset_str(dir);

	// DOS_GetCurrentDir doesn't always return something.
	// (if drive is messed up)
	DOS_GetCurrentDir(0, dir);
	InjectMissingNewline();
	WriteOut("%c:\\%s>", drive, dir);

	// prevents excessive newline if cmd prints nothing
	ResetLastWrittenChar('\n');
}

void DOS_Shell::InputCommand(char* line)
{
	std::string command = ReadCommand();
	trim(command);
	if (!command.empty()) {
		history.emplace_back(command);
	}

	const auto* const dos_section = dynamic_cast<Section_prop*>(
	        control->GetSection("dos"));
	if (dos_section == nullptr) {
		assert(false);
		return;
	}

	const std::string expand_shell_variable_pref = dos_section->Get_string(
	        "expand_shell_variable");

	const auto expand_shell_pref_has_bool = parse_bool_setting(
	        expand_shell_variable_pref);

	if ((expand_shell_variable_pref == "auto" && dos.version.major >= 7) ||
	    (expand_shell_pref_has_bool && *expand_shell_pref_has_bool == true)) {
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
		assert(completion.empty() || completion_start <= command.size());

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
			switch (static_cast<ScanCode>(data)) {
			case ScanCode::F3: {
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

			case ScanCode::Left:
				if (cursor_position > 0) {
					--cursor_position;
				}
				break;

			case ScanCode::Right:
				if (cursor_position < command.size()) {
					++cursor_position;
				}
				break;

			case ScanCode::Up:
				if (history_index > 0) {
					history_clone[history_index] = command;
					--history_index;
					command = history_clone[history_index];
					cursor_position = command.size();
				}
				break;

			case ScanCode::Down:
				if (history_index + 1 < history_clone.size()) {
					history_clone[history_index] = command;
					++history_index;
					command = history_clone[history_index];
					cursor_position = command.size();
				}
				break;

			case ScanCode::Home: cursor_position = 0; break;

			case ScanCode::End:
				cursor_position = command.size();
				break;

			case ScanCode::Delete:
				command.erase(cursor_position, 1);
				break;

			case ScanCode::ShiftTab:
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

				const auto delimiter = command.find_last_of(" \\/");
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

	search += "*";

	if (search.find_first_of('.') == std::string::npos) {
		search += ".*";
	}

	const auto save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	const auto dta = DOS_DTA(dos.dta());

	bool res = DOS_FindFirst(search.c_str(), FatAttributeFlags::NotVolume);

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

bool DOS_Shell::ExecuteProgram(std::string_view name, std::string_view args)
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
		if (!batchfiles.empty() && !call) {
			batchfiles.pop();
		}

		auto reader = FileReader::GetFileReader(fullname);
		if (reader) {
			batchfiles.emplace(*this, std::move(*reader), name, args, echo);
		} else {
			WriteOut("Could not open %s", fullname.c_str());
		}

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
	const auto have_path_env = GetEnvStr("PATH", path_environment);
	const auto path_equals   = path_environment.find_first_of('=');

	if (have_path_env && path_equals != std::string::npos) {
		path_environment = path_environment.substr(path_equals + 1);
		auto path_directories = split_with_empties(path_environment, ';');

		remove_empties(path_directories);

		for (auto& directory : path_directories) {
			if (directory.back() != '\\') {
				directory += '\\';
			}
		}

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
	CommandTail cmdtail = {};
	cmdtail.count       = static_cast<uint8_t>(
                std::min(args.size(), CommandTail::MaxCmdtailBufferSize));
	std::copy_n(args.begin(), cmdtail.count, cmdtail.buffer);
	cmdtail.buffer[cmdtail.count] = '\r';

	/* Copy command line in stack block too */
	MEM_BlockWrite(SegPhys(ss) + reg_sp + 0x100, &cmdtail, 128);

	/* Split input line up into parameters, using a few special rules, most
	 * notable the one for /AAA => A\0AA Qbix: It is extremly messy, but
	 * this was the only way I could get things like /:aa and :/aa to work
	 * correctly */
	std::array<std::string, 2> fcb_args;

	constexpr auto separators = "=;,\t /";
	for (auto& fcb : fcb_args) {
		const auto arg_index = std::min(args.find_first_not_of(separators),
		                                args.find_first_of('/'));
		if (arg_index == std::string::npos) {
			break;
		}

		args = args.substr(arg_index);
		if (args.size() > 1 && args[0] == '/') {
			fcb  = args[1];
			args = args.substr(2);
		}

		else {
			const auto next_separator = args.find_first_of(separators);
			fcb  = args.substr(0, next_separator);
			args = args.substr(fcb.size());
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

bool DOS_Shell::GetEnvStr(const char* entry, std::string& result) const
{
	/* Walk through the internal environment and see for a match */
	PhysPt env_read = PhysicalMake(psp->GetEnvironment(), 0);

	char env_string[1024 + 1];
	result.erase();
	if (!entry[0]) {
		return false;
	}
	const auto entry_length = strlen(entry);
	do {
		MEM_StrCopy(env_read, env_string, 1024);
		if (!env_string[0]) {
			return false;
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		char* equal = strchr(env_string, '=');
		if (!equal) {
			continue;
		}
		/* replace the = with \0 to get the length */
		*equal = 0;
		if (strlen(env_string) != entry_length) {
			continue;
		}
		if (strcasecmp(entry, env_string) != 0) {
			continue;
		}
		/* restore the = to get the original result */
		*equal = '=';
		result = env_string;
		return true;
	} while (1);
	return false;
}

bool DOS_Shell::GetEnvNum(Bitu num, std::string& result) const
{
	char env_string[1024 + 1];
	PhysPt env_read = PhysicalMake(psp->GetEnvironment(), 0);
	do {
		MEM_StrCopy(env_read, env_string, 1024);
		if (!env_string[0]) {
			break;
		}
		if (!num) {
			result = env_string;
			return true;
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		num--;
	} while (1);
	return false;
}

Bitu DOS_Shell::GetEnvCount() const
{
	PhysPt env_read = PhysicalMake(psp->GetEnvironment(), 0);
	Bitu num        = 0;
	while (mem_readb(env_read) != 0) {
		for (; mem_readb(env_read); env_read++) {
		};
		env_read++;
		num++;
	}
	return num;
}

bool DOS_Shell::SetEnv(const char* entry, const char* new_string)
{
	PhysPt env_read = PhysicalMake(psp->GetEnvironment(), 0);

	// Get size of environment.
	DOS_MCB mcb(psp->GetEnvironment() - 1);
	uint16_t envsize = mcb.GetSize() * 16;

	PhysPt env_write          = env_read;
	PhysPt env_write_start    = env_read;
	char env_string[1024 + 1] = {0};
	const auto entry_length = strlen(entry);
	do {
		MEM_StrCopy(env_read, env_string, 1024);
		if (!env_string[0]) {
			break;
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		if (!strchr(env_string, '=')) {
			continue; /* Remove corrupt entry? */
		}
		if ((strncasecmp(entry, env_string, entry_length) == 0) &&
		    env_string[entry_length] == '=') {
			continue;
		}
		MEM_BlockWrite(env_write,
		               env_string,
		               (Bitu)(safe_strlen(env_string) + 1));
		env_write += (PhysPt)(safe_strlen(env_string) + 1);
	} while (1);
	/* TODO Maybe save the program name sometime. not really needed though */
	/* Save the new entry */

	// ensure room
	if (envsize <= (env_write - env_write_start) + strlen(entry) + 1 +
	                       strlen(new_string) + 2) {
		return false;
	}

	if (new_string[0]) {
		std::string bigentry(entry);
		for (std::string::iterator it = bigentry.begin();
		     it != bigentry.end();
		     ++it) {
			*it = toupper(*it);
		}
		snprintf(env_string, 1024 + 1, "%s=%s", bigentry.c_str(), new_string);
		MEM_BlockWrite(env_write,
		               env_string,
		               (Bitu)(safe_strlen(env_string) + 1));
		env_write += (PhysPt)(safe_strlen(env_string) + 1);
	}
	/* Clear out the final piece of the environment */
	mem_writeb(env_write, 0);
	return true;
}
