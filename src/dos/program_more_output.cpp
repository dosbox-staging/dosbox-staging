/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_more_output.h"

#include "../ints/int10.h"
#include "callback.h"
#include "checks.h"
#include "dos_inc.h"
#include "string_utils.h"

#include <algorithm>
#include <array>
#include <cctype>

CHECK_NARROWING();

// ASCII control characters
constexpr char code_ctrl_c = 0x03; // end of text
constexpr char code_lf     = 0x0a; // line feed
constexpr char code_cr     = 0x0d; // carriage return

// ***************************************************************************
// Base class, only for internal usage
// ***************************************************************************

MoreOutputBase::MoreOutputBase(Program &program) : program(program)
{
	// Retrieve screen size, prepare limits
	constexpr uint16_t min_lines   = 10;
	constexpr uint16_t min_columns = 40;
	max_lines   = std::max(min_lines, INT10_GetTextRows());
	max_columns = std::max(min_columns, INT10_GetTextColumns());
	// The prompt at the bottom will cause scrolling,
	// so reduce the maximum number of lines accordingly
	max_lines = static_cast<uint16_t>(max_lines - 1);
}

void MoreOutputBase::SetTabSize(const uint8_t new_tab_size)
{
	assert(new_tab_size > 0);
	tab_size = new_tab_size;
}

uint16_t MoreOutputBase::GetMaxLines() const
{
	return max_lines;
}

uint16_t MoreOutputBase::GetMaxColumns() const
{
	return max_columns;
}

void MoreOutputBase::PrepareInternals()
{
	line_counter = 0;

	has_multiple_files   = false;
	should_end_on_ctrl_c = false;
	should_print_ctrl_c  = false;
	was_prompt_recently  = false;

	tabs_remaining  = 0;

	skip_next_cr    = false;
	skip_next_lf    = false;
}

UserDecision MoreOutputBase::DisplaySingleStream()
{
	auto previous_column = GetCurrentColumn();
	auto decision        = UserDecision::Next;

	tabs_remaining = 0;
	skip_next_cr   = false;
	skip_next_lf   = false;

	was_prompt_recently = false;

	while (true) {
		if (shutdown_requested) {
			decision = UserDecision::Cancel;
			break;
		}

		// Read character
		char code = 0;
		bool is_last = false;
		if (!GetCharacter(code, is_last)) {
			decision = UserDecision::Next; // end of current file
			break;
		}

		// A trick to make it more resistant to ANSI cursor movements
		const auto current_row = GetCurrentRow();
		if (line_counter > current_row) {
			line_counter = current_row;
		}

		// Handle new line characters
		bool new_line = false;
		if (code == code_cr) {
			skip_next_lf = true;
			new_line     = true;
		} else if (code == code_lf) {
			skip_next_cr = true;
			new_line     = true;
		} else {
			skip_next_cr = false;
			skip_next_lf = false;
		}

		// Duplicate character on the output
		if (new_line) {
			code = '\n';
		}
		WriteOut("%c", code);

		// Detect 'new line' due to character passing the last column
		const auto current_column = GetCurrentColumn();
		bool line_overflow = false;
		if (!current_column && previous_column && code != code_cr &&
		    code != code_lf) {
			// The cursor just moved to new line due to too small
			// screen width (line overflow). If this is followed by
			// a new line, ignore it, so that it is possible to i. e.
			// nicely display up to 80-character lines on a standard
			// 80 column screen
			skip_next_cr  = true;
			skip_next_lf  = true;
			new_line      = true;
			line_overflow = true;
		}
		previous_column = current_column;

		// Update new line counter, decide if pause needed
		if (new_line && current_row) {
			++line_counter;
		}
		if (code != '\n') {
			was_prompt_recently = false;
		}
		if (is_last && !line_overflow) {
			// Skip further processing (including possible user prompt)
			// if we know no data is left and no line overflow produced
			// an 'artificial new line'
			decision = UserDecision::Next;
			break;
		}
		if (line_counter < max_lines) {
			continue;
		}

		// New line occured just enough times for a pause
		decision = PromptUser();
		if (decision == UserDecision::Cancel ||
		    decision == UserDecision::Next) {
			break;
		}
	}

	if (GetCurrentColumn()) {
		++line_counter;
		WriteOut("\n");
	}

	return decision;
}

UserDecision MoreOutputBase::PromptUser()
{
	line_counter = 0;

	if (GetCurrentColumn()) {
		WriteOut("\n");
	}

	if (has_multiple_files) {
		WriteOut(MSG_Get("PROGRAM_MORE_PROMPT_MULTI"));
	} else {
		WriteOut(MSG_Get("PROGRAM_MORE_PROMPT_SINGLE"));
	}

	auto decision = UserDecision::Cancel;

	if (has_multiple_files) {
		decision = DOS_WaitForCancelContinueNext();
	} else {
		decision = DOS_WaitForCancelContinue();
	}

	if (decision == UserDecision::Cancel || decision == UserDecision::Next) {
		WriteOut(" ");
		WriteOut(MSG_Get("PROGRAM_MORE_TERMINATE"));
		WriteOut("\n");
		++line_counter;
	} else {
		// We are going to continue - erase the prompt
		WriteOut("\033[M"); // clear line
		auto counter = GetCurrentColumn();
		while (counter--) {
			WriteOut("\033[D"); // cursor one position back
		}
	}

	was_prompt_recently = true;
	return decision;
}

bool MoreOutputBase::GetCharacter(char &code, bool &is_last)
{
	is_last = false;
	if (!tabs_remaining) {
		while (true) {
			if (!GetCharacterRaw(code, is_last)) {
				return false; // end of data
			}

			if (should_end_on_ctrl_c && code == code_ctrl_c) {
				if (should_print_ctrl_c) {
					WriteOut("^C");
				}
				return false; // end by CTRL+C
			}

			// Skip CR/LF characters if requested
			if (skip_next_cr && code == code_cr) {
				skip_next_cr = false;
			} else if (skip_next_lf && code == code_lf) {
				skip_next_lf = false;
			} else {
				break;
			}
		}

		// If TAB found, replace it with given number of spaces
		if (code == '\t') {
			tabs_remaining = tab_size;
			is_tab_last    = is_last;
			is_last        = false;
		}
	}

	if (tabs_remaining) {
		code = ' ';
		--tabs_remaining;
		is_last = is_tab_last && !tabs_remaining;
	}

	return true;
}

uint8_t MoreOutputBase::GetCurrentColumn()
{
	const auto page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	return CURSOR_POS_COL(page);
}

uint8_t MoreOutputBase::GetCurrentRow()
{
	const auto page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	return CURSOR_POS_ROW(page);
}

// ***************************************************************************
// Output file/device/stream content via MORE
// ***************************************************************************

MoreOutputFiles::MoreOutputFiles(Program &program) : MoreOutputBase(program) {}

void MoreOutputFiles::AddFile(const std::string &file_path, const bool is_device)
{
	input_files.emplace_back();
	auto &entry = input_files.back();

	entry.path      = file_path;
	entry.is_device = is_device;
}

void MoreOutputFiles::Display()
{
	if (SuppressWriteOut("")) {
		input_files.clear();
		return;
	}

	PrepareInternals();

	has_multiple_files  = input_files.size() > 1;
	should_print_ctrl_c = input_files.empty();

	// Show STDIN or input file(s) content
	if (input_files.empty()) {
		DisplayInputStream();
	} else {
		DisplayInputFiles();
	}

	input_files.clear();
	WriteOut("\n");
}

std::string MoreOutputFiles::GetShortPath(const std::string &file_path,
                                          const char *msg_id)
{
	assert(msg_id);

	// We need to make sure the path and file name fits within
	// the designated space - if not, we have to shorten it.

	// The shortest name we should be able to display is:
	// - 3 dots (ellipsis)
	// - 1 path separator
	// - 8 characters of name
	// - 1 dot
	// - 3 characters of extension
	// This gives 16 characters.
	// We need to keep the last column free (reduces max length by 1).
	// Format string contains '%s' (increases max length by 2).
	constexpr size_t min = 16;
	const auto max = GetMaxColumns() - std::strlen(MSG_Get(msg_id)) + 1;
	const auto max_len = std::max(min, max);

	// Nothing to do if file name matches the constraint
	if (file_path.length() <= max_len) {
		return file_path;
	}

	// We need to shorten the name - try to strip part of the path
	static const std::string ellipsis = "...";
	auto shortened = file_path;
	while (shortened.length() > max_len &&
	       std::count(shortened.begin(), shortened.end(), '\\') > 1) {
		// Strip one level of path at a time
		const auto pos = shortened.find('\\', shortened.find('\\') + 1);
		shortened      = ellipsis + shortened.substr(pos);
	}

	// If still too long, just cut away the beginning
	const auto len = shortened.length();
	if (len > max_len) {
		shortened = ellipsis + shortened.substr(len - max_len + 3);
	}

	return shortened;
}

void MoreOutputFiles::DisplayInputStream()
{
	// We need to be able to read STDIN for key presses, but it is most
	// likely redirected - so clone the handle, and reconstruct real STDIN
	// from STDERR (idea from FreeDOS implementation,
	// https://github.com/FDOS/more/blob/master/src/more.c)
	if (!DOS_DuplicateEntry(STDIN, &input_handle) ||
	    !DOS_ForceDuplicateEntry(STDERR, STDIN)) {
		LOG_ERR("DOS: Unable to prepare handles in MORE");
		return;
	}

	WriteOut("\n");

	// Since this CAN be STDIN input (there is no way to check),
	// CTRL+C shall quit
	should_end_on_ctrl_c = true;
	DisplaySingleStream();
}

void MoreOutputFiles::DisplayInputFiles()
{
	WriteOut("\n");

	bool first = true;
	for (const auto &input_file : input_files) {
		if (!first && !was_prompt_recently &&
			UserDecision::Cancel == PromptUser()) {
			break;
		}
		first = false;

		if (!DOS_OpenFile(input_file.path.c_str(), 0, &input_handle)) {
			LOG_WARNING("DOS: MORE - could not open '%s'",
			            input_file.path.c_str());
			const auto short_path = GetShortPath(input_file.path,
			                                     "PROGRAM_MORE_OPEN_ERROR");
			WriteOut(MSG_Get("PROGRAM_MORE_OPEN_ERROR"),
			         short_path.c_str());
			WriteOut("\n");
			++line_counter;
			continue;
		}

		if (input_file.is_device) {
			const auto short_path = GetShortPath(input_file.path,
			                                     "PROGRAM_MORE_NEW_DEVICE");
			WriteOut(MSG_Get("PROGRAM_MORE_NEW_DEVICE"),
			         short_path.c_str());
		} else {
			const auto short_path = GetShortPath(input_file.path,
			                                     "PROGRAM_MORE_NEW_FILE");
			WriteOut(MSG_Get("PROGRAM_MORE_NEW_FILE"),
			         short_path.c_str());
		}
		WriteOut("\n");
		++line_counter;

		// If input from a device, CTRL+C shall quit
		should_end_on_ctrl_c = input_file.is_device;

		const auto decision = DisplaySingleStream();
		DOS_CloseFile(input_handle);
		if (decision == UserDecision::Cancel) {
			break;
		}
	}

	// End message and command prompt is going to appear; ensure the
	// scrolling won't make top lines disappear before user reads them
	const int free_rows_threshold = 2;
	if (!was_prompt_recently &&
		GetMaxLines() - line_counter < free_rows_threshold) {
		PromptUser();
	}

	WriteOut(MSG_Get("PROGRAM_MORE_END"));
	WriteOut("\n");
}

bool MoreOutputFiles::GetCharacterRaw(char &code, bool &is_last)
{
	// Skip detecting if it is the last character for file/stream
	// mode - this is often problematic (like with STDIN input)
	// and wouldn't bring any user experience improvements due to
	// our 'end of input' message displayed at the end.
	is_last = false;

	uint16_t count = 1;
	DOS_ReadFile(input_handle, reinterpret_cast<uint8_t *>(&code), &count);

	if (!count) {
		return false; // end of stream
	}

	return true;
}

// ***************************************************************************
// Output string content via MORE
// ***************************************************************************

MoreOutputStrings::MoreOutputStrings(Program &program) : MoreOutputBase(program)
{}

void MoreOutputStrings::AddString(const char *format, ...)
{
	constexpr size_t buf_len = 16 * 1024;

	char buf[buf_len];
	va_list msg;

	va_start(msg, format);
	vsnprintf(buf, buf_len - 1, format, msg);
	va_end(msg);

	input_strings += std::string(buf);
}

void MoreOutputStrings::Display()
{
	if (SuppressWriteOut("")) {
		input_strings.clear();
		return;
	}

	PrepareInternals();

	input_position = 0;

	has_multiple_files   = false;
	should_end_on_ctrl_c = false;

	// Change the last CR/LF or LF/CR to a single
	// end of the line symbol, so that 'is_last'
	// can be calculated easilty
	const auto length = input_strings.size();
	if (length >= 2) {
		const auto code1 = input_strings[length - 2];
		const auto code2 = input_strings[length - 1];
		if ((code1 == code_lf && code2 == code_cr) ||
		    (code1 == code_cr && code2 == code_lf)) {
			input_strings.pop_back();
		}
	}

	WriteOut("\n");
	DisplaySingleStream();

	input_strings.clear();
}

bool MoreOutputStrings::GetCharacterRaw(char &code, bool &is_last)
{
	is_last = false;

	if (input_position >= input_strings.size()) {
		is_last = true;
		return false;
	}

	code = input_strings[input_position++];
	if (input_position == input_strings.size()) {
		is_last = true;
	}

	return true;
}
