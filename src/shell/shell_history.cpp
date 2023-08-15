/*
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

static constexpr int HistoryMaxLineSize = 256;
static constexpr int HistoryMaxNumLines = 500;

static std_fs::path get_shell_history_path();

ShellHistory& ShellHistory::Instance()
{
	static auto history = ShellHistory();
	return history;
}

void ShellHistory::Append(std::string command)
{
	trim(command);
	if (!iequals(command, "exit")) {
		commands.emplace_back(std::move(command));
	}
}

const std::vector<std::string>& ShellHistory::GetCommands() const
{
	return commands;
}

ShellHistory::ShellHistory()
        : path(get_shell_history_path()),
          secure_mode(control->SecureMode())
{
	if (secure_mode) {
		return;
	}
	if (path.empty()) {
		return;
	}
	auto history_file = std::ifstream(path);
	if (!history_file) {
		return;
	}

	std::string line;
	while (getline(history_file, line)) {
		trim(line);
		auto len = line.length();
		if (len > 0 && len <= HistoryMaxLineSize) {
			commands.emplace_back(std::move(line));
		}
	}
}

ShellHistory::~ShellHistory()
{
	if (secure_mode) {
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
	if (section != nullptr) {
		const auto* path = section->Get_path("shell_history_file");
		if (path != nullptr) {
			return path->realpath;
		}
	}
	return {};
}
