// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ls.h"

#include <string>

#include "misc/ansi_code_markup.h"
#include "utils/checks.h"
#include "ints/int10.h"
#include "more_output.h"
#include "shell/shell.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

void LS::Run()
{
	// Handle command line

	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_LS_HELP_LONG"));
		output.Display();
		return;
	}

	const bool has_option_all = cmd->FindExistRemoveAll("/a");

	auto patterns = cmd->GetArguments();

	// Make sure no other switches are supplied

	std::string tmp_str = {};
	if (cmd->FindStringBeginCaseSensitive("/", tmp_str)) {
		tmp_str = std::string("/") + tmp_str;
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp_str.c_str());
		return;
	}

	// Check for unsupported wildcard patterns

	for (const auto& pattern : patterns) {
		const auto first_wildcard = std::min(pattern.find('*'),
		                                     pattern.find('?'));
		if (first_wildcard == std::string::npos) {
			continue;
		}

		const auto last_slash     = pattern.rfind('/');
		const auto last_backslash = pattern.rfind('\\');

		if ((last_backslash != std::string::npos &&
		     last_backslash > first_wildcard) ||
		    (last_slash != std::string::npos &&
		     last_slash > first_wildcard)) {

		    WriteOut(MSG_Get("PROGRAM_LS_UNHANDLED_WILDCARD_PATTERN"),
		             pattern.c_str());
		    return;
		}
	}

	// Prepare search parameters

	FatAttributeFlags search_attr = FatAttributeFlags::NotVolume;
	if (!has_option_all) {
		search_attr.system = false;
		search_attr.hidden = false;
	}

	if (patterns.empty()) {
		patterns.emplace_back();
	}
	for (auto& pattern : patterns) {
		pattern = to_search_pattern(pattern.c_str());
	}

	// Search for files/directories according to pattern

	std::vector<DOS_DTA::Result> dir_contents = {};

	const RealPt original_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	for (const auto& pattern : patterns) {
		if (DOSBOX_IsShutdownRequested()) {
			break;
		}

		if (!DOS_FindFirst(pattern.c_str(), search_attr)) {
			continue;
		}

		do {
			DOS_DTA::Result result = {};
			dta.GetResult(result);
			if (result.IsDummyDirectory()) {
				continue;
			}
			dir_contents.push_back(result);
		} while (!DOSBOX_IsShutdownRequested() && DOS_FindNext());
	}

	dos.dta(original_dta);
	if (DOSBOX_IsShutdownRequested()) {
		return;
	}

	if (dir_contents.empty()) {
		WriteOut(MSG_Get("SHELL_NO_FILES_SUBDIRS_TO_DISPLAY"));
		return;
	}

	// Sort directory contents

	constexpr bool reverse_order = false;
	DOS_Sort(dir_contents,
	         ResultSorting::ByName,
	         reverse_order,
	         ResultGrouping::NonFilesFirst);

	// Remove duplicates (might arise from multiple patterns)

	const auto tmp = std::move(dir_contents);
	dir_contents.clear();
	for (const auto& entry : tmp) {
		if (dir_contents.empty() ||
		    dir_contents.back().IsDirectory() != entry.IsDirectory() ||
		    dir_contents.back().name != entry.name) {
			dir_contents.push_back(entry);
		}
	}

	// Display search results

	constexpr uint8_t separation = 2; // chars separating columns

	const auto name_widths   = GetFileNameLengths(dir_contents, separation);
	const auto column_widths = GetColumnWidths(name_widths, separation + 1);

	const size_t num_columns = column_widths.size();

	const auto ansi_blue  = convert_ansi_markup("[color=light-blue]");
	const auto ansi_green = convert_ansi_markup("[color=light-green]");
	const auto ansi_reset = convert_ansi_markup("[reset]");

	auto write_color = [&](const std::string& ansi_color,
	                       const std::string& txt,
	                       const uint8_t width) {
		assert(width > txt.size());
		const auto margin_size = static_cast<int>(width - txt.size());
		const auto output      = ansi_color + txt + ansi_reset;
		WriteOut("%s%-*s", output.c_str(), margin_size, "");
	};

	size_t column_count = 0;
	for (const auto& entry : dir_contents) {
		auto name = entry.name;
		const auto column_width = column_widths[column_count++ % num_columns];

		if (entry.IsDirectory()) {
			upcase(name);
			write_color(ansi_blue, name, column_width);
		} else {
			lowcase(name);
			if (is_executable_filename(name)) {
				write_color(ansi_green, name, column_width);
			} else {
				WriteOut("%-*s", column_width, name.c_str());
			}
		}

		if (column_count % num_columns == 0) {
			WriteOut_NoParsing("\n");
		}
	}
}

std::vector<uint8_t> LS::GetFileNameLengths(const std::vector<DOS_DTA::Result>& dir_contents,
                                            const uint8_t padding)
{
	std::vector<uint8_t> name_lengths = {};
	name_lengths.reserve(dir_contents.size());

	for (const auto& entry : dir_contents) {
		const auto len = entry.name.length();
		name_lengths.push_back(static_cast<uint8_t>(len + padding));
	}

	return name_lengths;
}

std::vector<uint8_t> LS::GetColumnWidths(const std::vector<uint8_t>& name_widths,
                                         const uint8_t min_column_width)
{
	assert(min_column_width > 0);

	// Actual terminal width (number of text columns) using current text
	// mode; in practice it's either 40, 80, or 132.
	const auto screen_width = INT10_GetTextColumns();

	// Use screen_width-1 because we never want to print line up to the
	// actual limit; this would cause unnecessary line wrapping
	const auto max_columns = static_cast<size_t>((screen_width - 1) / min_column_width);
	std::vector<uint8_t> column_widths(max_columns);

	// This function returns true when column number is too high to fit
	// words into a terminal width.  If it returns false, then the first
	// column_count integers in column_widths vector describe the column
	// widths.
	auto too_many_columns = [&](const size_t column_count) {
		std::fill(column_widths.begin(), column_widths.end(), 0);
		if (column_count <= 1) {
			return false;
		}

		uint8_t max_line_width = 0; // tally of the longest line
		size_t current_column  = 0; // current column

		for (const auto width : name_widths) {
			const uint8_t old_width = column_widths[current_column];
			const uint8_t new_width = std::max(old_width, width);

			column_widths[current_column] = new_width;
			max_line_width = check_cast<uint8_t>(max_line_width +
			                                     new_width - old_width);

			if (max_line_width >= screen_width) {
				return true;
			}

			current_column = (current_column + 1) % column_count;
		}
		return false;
	};

	size_t column_count = max_columns;
	while (too_many_columns(column_count)) {
		column_count--;
		column_widths.pop_back();
	}

	return column_widths;
}

void LS::AddMessages()
{
	MSG_Add("PROGRAM_LS_UNHANDLED_WILDCARD_PATTERN",
	        "Unhandled wildcard pattern - '%s'\n");

	MSG_Add("PROGRAM_LS_HELP_LONG",
	        "Display directory contents in wide list format.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]ls[reset] [[color=light-cyan]PATTERN[reset] [[color=light-cyan]PATTERN[reset], ...]] [[color=light-cyan]PATH[reset] [[color=light-cyan]PATH[reset], ...]] [/a]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]PATTERN[reset]  either an exact filename or an inexact filename with wildcards, which\n"
	        "           are the asterisk (*) and the question mark (?)\n"
	        "  [color=light-cyan]PATH[reset]     exact path in a mounted DOS drive to list contents\n"
	        "  /a       list all files and directories, including hidden and system\n"
	        "\n"
	        "Notes:\n"
	        "  The command will list directories in [color=light-blue]blue[reset], executable DOS programs\n"
	        "  (*.com, *.exe, *.bat) in [color=light-green]green[reset], and other files in the normal color.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]ls[reset] [color=light-cyan]file.txt[reset]\n"
	        "  [color=light-green]ls[reset] [color=light-cyan]c*.ba?[reset]\n");
}
