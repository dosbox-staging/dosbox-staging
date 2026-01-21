// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shell/shell.h"

#include <algorithm>
#include <cstring>

#include "misc/logging.h"
#include "utils/string_utils.h"

[[nodiscard]] static bool found_label(std::string_view line, std::string_view label);

BatchFile::BatchFile(const Environment& host, std::unique_ptr<LineReader> input_reader,
                     const std::string_view entered_name,
                     const std::string_view cmd_line, const bool echo_on)
        : shell(host),
          cmd(entered_name, cmd_line),
          reader(std::move(input_reader)),
          echo(echo_on)
{}

bool BatchFile::ReadLine(char* lineout)
{
	auto is_comment_or_label = [](const std::string& line) {
		const auto colon_index = line.find_first_of(':');
		if (colon_index != std::string::npos &&
		    colon_index == line.find_first_not_of("=\t ")) {
			return true;
		}
		return false;
	};

	auto line = GetLine();
	while (line && (line->empty() || is_comment_or_label(*line))) {
		line = GetLine();
	}

	if (!line) {
		return false;
	}

	line = ExpandedBatchLine(*line);
	strncpy(lineout, line->c_str(), CMD_MAXLINE);
	lineout[CMD_MAXLINE - 1] = '\0';
	return true;
}

std::optional<std::string> BatchFile::GetLine()
{
	auto invalid_character = [](char c) {
		const auto data = static_cast<uint8_t>(c);

		/* Inclusion criteria:
		 *  - backspace for alien odyssey
		 *  - tab for batch files
		 *  - escape for ANSI
		 */
		constexpr uint8_t Esc           = 27;
		constexpr uint8_t UnitSeparator = 31;
		if (data <= UnitSeparator && data != '\t' && data != '\b' &&
		    data != Esc && data != '\n' && data != '\r') {
			LOG_DEBUG("Encountered non-standard character: Dec %03u and Hex %#04x",
			          data,
			          data);
			return true;
		}
		return false;
	};

	auto line = reader->Read();
	if (!line) {
		return {};
	}
	line->erase(std::remove_if(line->begin(), line->end(), invalid_character),
	            line->end());
	trim(*line);
	return *line;
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

			if (const auto env_val = shell.GetEnvironmentValue(
			            line.substr(0, closing_percent))) {
				expanded += *env_val;
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
	reader->Reset();

	while (auto line = GetLine()) {
		if (found_label(*line, label)) {
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
