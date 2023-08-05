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

#include "shell.h"

#include <cstring>

#include "logging.h"
#include "string_utils.h"

// Permitted ASCII control characters in batch files
constexpr char Esc           = 27;
constexpr char UnitSeparator = 31;

[[nodiscard]] static bool found_label(std::string_view line, std::string_view label);

BatchFile::BatchFile(const HostShell& host, std::unique_ptr<ByteReader> input_reader,
                     const std::string_view entered_name,
                     const std::string_view cmd_line, const bool echo_on)
        : shell(host),
          cmd(entered_name, cmd_line),
          reader(std::move(input_reader)),
          echo(echo_on)
{}

bool BatchFile::ReadLine(char* lineout)
{
	std::string line = {};
	while (line.empty()) {
		line = GetLine();

		if (line.empty()) {
			return false;
		}

		const auto colon_index = line.find_first_of(':');
		if (colon_index != std::string::npos &&
		    colon_index == line.find_first_not_of("=\t ")) {
			// Line is a comment or label, so ignore it
			line.clear();
		}

		trim(line);
	}

	line = ExpandedBatchLine(line);
	strncpy(lineout, line.c_str(), CMD_MAXLINE);
	lineout[CMD_MAXLINE - 1] = '\0';
	return true;
}

std::string BatchFile::GetLine()
{
	char data        = 0;
	std::string line = {};

	while (data != '\n') {
		const auto result = reader->Read();

		// EOF or EOL?
		if (!result || *result == '\0') {
			break;
		}

		data = *result;

		/* Inclusion criteria:
		 *  - backspace for alien odyssey
		 *  - tab for batch files
		 *  - escape for ANSI
		 */
		if (data <= UnitSeparator && data != '\t' && data != '\b' &&
		    data != Esc && data != '\n' && data != '\r') {
			LOG_DEBUG("Encountered non-standard character: Dec %03u and Hex %#04x",
			          data,
			          data);
		} else {
			line += data;
		}
	}

	return line;
}

std::string BatchFile::ExpandedBatchLine(std::string_view line) const
{
	std::string expanded = {};

	auto percent_index = line.find_first_of('%');
	while (percent_index != std::string::npos) {
		expanded += line.substr(0, percent_index);
		line = line.substr(percent_index + 1);

		if (line.empty()) {
			break;
		} else if (line[0] == '%') {
			expanded += '%';
		} else if (line[0] == '0') {
			expanded += cmd.GetFileName();
		} else if (line[0] >= '1' && line[0] <= '9') {
			std::string arg;
			if (cmd.FindCommand(line[0] - '0', arg)) {
				expanded += arg;
			}
		} else {
			auto closing_percent = line.find_first_of('%');
			if (closing_percent == std::string::npos) {
				break;
			}
			std::string env_key(line.substr(0, closing_percent));

			// Get the key's corresponding value from the environment
			if (std::string env_val = {};
			    shell.GetEnvStr(env_key.c_str(), env_val)) {
				// append just the trailing value portion
				expanded += env_val.substr(env_key.length() +
				                           sizeof('='));
			}
			line = line.substr(closing_percent);
		}
		line          = line.substr(1);
		percent_index = line.find_first_of('%');
	}

	expanded += line;
	return expanded;
}

bool BatchFile::Goto(const std::string_view label)
{
	std::string line = " ";
	reader->Reset();

	while (!line.empty()) {
		line = GetLine();
		if (found_label(line, label)) {
			return true;
		}
	}

	return false;
}

void BatchFile::Shift()
{
	cmd.Shift(1);
}

static bool found_label(std::string_view line, const std::string_view label)
{
	const auto label_start  = line.find_first_not_of("=\t :");
	const auto label_prefix = line.substr(0, label_start);

	if (label_start == std::string::npos ||
	    std::count(label_prefix.begin(), label_prefix.end(), ':') != 1) {
		return false;
	}

	line = line.substr(label_start);

	const auto label_end = line.find_first_of("\t\r\n ");
	if (label_end != std::string::npos && label_end == label.size()) {
		line = line.substr(0, label_end);
	}

	return iequals(line, label);
}

void BatchFile::SetEcho(const bool echo_on)
{
	echo = echo_on;
}

bool BatchFile::Echo() const
{
	return echo;
}
