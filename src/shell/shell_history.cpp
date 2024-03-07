/*
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include <fstream>

#include "checks.h"
#include "control.h"
#include "dosbox.h"
#include "fs_utils.h"
#include "string_utils.h"

CHECK_NARROWING();

static constexpr int HistoryMaxLineLength = 256;
static constexpr int HistoryMaxNumLines = 500;

static std_fs::path get_shell_history_path();
static bool command_is_exit(std::string_view command);

void ShellHistory::Append(std::string command, uint16_t code_page)
{
	auto to_utf8_str = [code_page](const std::string& dos_str) {
		std::string utf8_str = {};
		dos_to_utf8(dos_str, utf8_str, code_page);
		return utf8_str;
	};
	
	trim(command);
	
	auto utf8_command = to_utf8_str(command);

	if (!command_is_exit(command) && !command.empty() &&
	    (commands.empty() || commands.back() != utf8_command)) {
		commands.emplace_back(std::move(utf8_command));
	}
}

std::vector<std::string> ShellHistory::GetCommands(uint16_t code_page) const
{
	auto to_dos_str = [code_page](const std::string& utf8_str) {
		std::string dos_str = {};
		utf8_to_dos(utf8_str, dos_str, UnicodeFallback::Simple, code_page);
		return dos_str;
	};

	std::vector<std::string> dos_encoded_commands{};
	std::transform(commands.begin(),
	               commands.end(),
	               std::back_inserter(dos_encoded_commands),
	               to_dos_str);

	return dos_encoded_commands;
}

ShellHistory::ShellHistory() : path(get_shell_history_path())
{
	// Must check arguments directly as control->SwitchToSecureMode()
	// will not be called until the first shell is run
	if (control->arguments.securemode) {
		return;
	}
	if (path.empty()) {
		return;
	}
	auto history_file = std::ifstream(path);
	if (!history_file) {
		path.clear();
		return;
	}

	std::string line;
	while (getline(history_file, line)) {
		trim(line);
		auto len = line.length();
		if (len > 0 && len <= HistoryMaxLineLength) {
			commands.emplace_back(line);
		}
	}
}

ShellHistory::~ShellHistory()
{
	// Secure mode can be enabled from the shell during runtime.
	// On exit, we must check this value instead.
	if (control->SecureMode()) {
		return;
	}
	if (path.empty()) {
		return;
	}
	auto history_file = std::ofstream(path);
	if (!history_file) {
		LOG_WARNING("SHELL: Unable to update history file: '%s'",
		            path.string().c_str());
		return;
	}

	if (commands.size() > HistoryMaxNumLines) {
		commands.erase(commands.begin(), commands.end() - HistoryMaxNumLines);
	}

	for (const auto& command : commands) {
		history_file << command << '\n';
	}
}

static std_fs::path get_shell_history_path()
{
	const auto* section = dynamic_cast<Section_prop*>(control->GetSection("dos"));
	assert(section);

	const auto* path = section->Get_path("shell_history_file"); //-V522
	if (path == nullptr) {
		return {};
	}

	return path->realpath;
}

static bool command_is_exit(std::string_view command)
{
	static constexpr auto Delimiters = std::string_view(",;= \t");
	static constexpr auto Exit       = std::string_view("exit");

	auto command_start = command.find_first_not_of(Delimiters);
	if (command_start >= command.size() ||
	    !ciequals(command[command_start], Exit.front())) {
		return false;
	}
	command = command.substr(command_start);

	auto command_end = command.find_first_of(Delimiters);
	if (command_end <= command.size()) {
		command = command.substr(0, command_end);
	}

	return iequals(Exit, command);
}
