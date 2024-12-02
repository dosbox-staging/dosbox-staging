/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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
#include "ascii.h"
#include "callback.h"
#include "checks.h"
#include "dos_inc.h"
#include "shell.h"
#include "string_utils.h"

#include <algorithm>
#include <array>
#include <cctype>

CHECK_NARROWING();

// ANSI control sequences
static const std::string ansi_clear_screen = "\033[2J";

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

void MoreOutputBase::SetOptionClear(const bool enabled)
{
	has_option_clear = enabled;
}

void MoreOutputBase::SetOptionExtendedMode(const bool enabled)
{
	has_option_extended_mode = enabled;
}

void MoreOutputBase::SetOptionExpandFormFeed(const bool enabled)
{
	has_option_expand_form_feed = enabled;
}

void MoreOutputBase::SetOptionSquish(const bool enabled)
{
	has_option_squish = enabled;
}

void MoreOutputBase::SetOptionStartLine(const uint32_t line_num)
{
	option_start_line_num = line_num;
}

void MoreOutputBase::SetOptionTabSize(const uint8_t tab_size)
{
	assert(tab_size > 0);
	option_tab_size = tab_size;
}

void MoreOutputBase::SetOptionNoPaging(const bool enabled)
{
	has_option_no_paging = enabled;
}

void MoreOutputBase::SetLinesInStream(const uint32_t lines)
{
	lines_in_stream = lines;
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
	column_counter      = 0;
	screen_line_counter = 0;
	lines_to_display    = max_lines;
	lines_to_skip       = 0;
	lines_in_stream     = 0;
	start_line_num      = option_start_line_num;

	is_output_redirected = false;
	has_multiple_files   = false;
	should_end_on_ctrl_c = false;
	should_print_ctrl_c  = false;

	should_skip_pre_exit_prompt = false;

	tabs_remaining      = 0;
	new_lines_ramaining = 0;
}

MoreOutputBase::UserDecision MoreOutputBase::DisplaySingleStream()
{
	state = State::Normal;

	std::string ansi_code  = {};
	bool is_state_ansi     = false;
	bool is_state_ansi_end = false;
	bool is_state_new_line = true;

	bool skipped_already_notified = false;
	bool should_squish_new_line   = false;

	auto previous_column = GetCursorColumn();
	auto decision        = UserDecision::NextFile;

	stream_line_counter = 0;
	last_fetched_code   = 0;
	tabs_remaining      = 0;

	while (true) {
		decision = PromptUserIfNeeded();
		if (decision == UserDecision::Cancel ||
		    decision == UserDecision::NextFile) {
			break;
		}
		if (decision == UserDecision::SkipNumLines) {
			skipped_already_notified = false;
			start_line_num = stream_line_counter + lines_to_skip;
			lines_to_skip  = 0;
		}

		// Read character
		char code = 0;
		bool is_last_character = false;
		if (!GetCharacter(code, is_last_character)) {
			decision = UserDecision::NextFile; // end of current file
			break;
		}

		// Update current state based on character code
		switch (state) {
		case State::AnsiEsc:
			if (code == '[') {
				state = State::AnsiSci;
			} else {
				state = State::AnsiEscEnd;
			}
			break;
		case State::AnsiSci:
			if (code >= '@' && code != Ascii::Delete) {
				state = State::AnsiSciEnd;
			}
			break;
		default:
			if (code == Ascii::Escape) {
				state = State::AnsiEsc;
			} else if (code == Ascii::CarriageReturn) {
				state = State::NewLineCR;
				// to handle LF/CR line endings
				code = Ascii::LineFeed;
			} else if (code == Ascii::LineFeed) {
				state = State::NewLineLF;
			} else {
				state = State::Normal;
			}
			break;
		}

		is_state_ansi_end = (state == State::AnsiEscEnd) ||
		                    (state == State::AnsiSciEnd);
		is_state_ansi     = (state == State::AnsiEsc) ||
		                    (state == State::AnsiSci) ||
		                    is_state_ansi_end;
		is_state_new_line = (state == State::NewLineCR) ||
		                    (state == State::NewLineLF);

		if (!is_state_new_line) {
			should_skip_pre_exit_prompt = false;
		}

		// Ignore everything before the starting line
		if (stream_line_counter < start_line_num) {
			if (!skipped_already_notified) {
				WriteOut(MSG_Get("PROGRAM_MORE_SKIPPED"));
				WriteOut("\n");
				++screen_line_counter;
				skipped_already_notified = true;
			}
			continue;
		}
		if (stream_line_counter == start_line_num &&
		    start_line_num && is_state_new_line) {
			continue;
		}

		// Handle squish mode
		if (should_squish_new_line && is_state_new_line &&
		    !new_lines_ramaining) {
			continue;
		}
		if (is_state_new_line && has_option_squish) {
			should_squish_new_line = (column_counter == 0);
		}
		if (!is_state_new_line) {
			should_squish_new_line = false;
		}

		// NOTE: Neither MS-DOS 6.22 nor FreeDOS supports ANSI
		// sequences within their MORE implementation. Our ANSI
		// handling also isn't perfect, but the code here
		// has to be fully synchronized with the scren output.
		// Therefore, ANSI sequences which move the cursor are
		// only partially supported.

		// A trick to make it more resistant to ANSI cursor movements
		const auto previous_row = GetCursorRow();
		if (screen_line_counter > previous_row) {
			screen_line_counter = previous_row;
		}

		// Print character, handle ANSI sequences
		if (is_state_ansi) {
			ansi_code.push_back(code);
		}
		WriteOut("%c", code);
		if (state == State::Normal) {
			++column_counter;
		} else if (is_state_ansi_end) {
			if (ansi_code == ansi_clear_screen) {
				column_counter      = 0;
				screen_line_counter = 0;
			}
			// TODO: consider handling also other ANSI control codes
			ansi_code.clear();
		}

		// Detect redirected command output
		const auto current_row    = GetCursorRow();
		const auto current_column = GetCursorColumn();
		if (is_state_new_line) {
			// New line should move the cursor to the first column
			// and to the next row (unless we are already on the last
			// line) - if not, the output must have been redirected
			if (current_row == previous_row && current_row < max_lines) {
				is_output_redirected = true;
			} else if (current_column) {
				is_output_redirected = true;
			}
		}
		if (!is_state_ansi && (current_column == previous_column) &&
		    is_extended_printable_ascii(code)) {
			// Alphanumeric character outside of ANSI sequence
			// always changes the current column - if not, the
			// output must have been redirected
			is_output_redirected = true;
		}

		// Detect 'new line' due to character passing the last column
		if (!current_column && previous_column &&
		    code != Ascii::CarriageReturn && code != Ascii::LineFeed) {
			// The cursor just moved to new line due to too small
			// screen width (line overflow). If this is followed by
			// a new line, ignore it, so that it is possible to i. e.
			// nicely display up to 80-character lines on a
			// standard 80 column screen
			state             = State::LineOverflow;
			is_state_new_line = true;
		}
		previous_column = current_column;

		// Update new line counter, decide if pause needed
		if (is_state_new_line) {
			column_counter = 0;
			++screen_line_counter;
		}
		if (is_last_character && state != State::LineOverflow) {
			// Skip further processing (including possible user
			// prompt) if we know no data is left and we haven't
			// switched to the new line due to overflow
			decision = UserDecision::NextFile;
			break;
		}
	}

	if ((!is_output_redirected && GetCursorColumn()) ||
	    (is_output_redirected && !is_state_new_line)) {
		WriteOut("\n");
		++screen_line_counter;
	}

	start_line_num = 0; // option_start_line_num only applies to first stream
	lines_in_stream = 0; // total number of lines has to be set for each stream
	return decision;
}

void MoreOutputBase::ClearScreenIfRequested()
{
	if (has_option_clear) {
		WriteOut(ansi_clear_screen.c_str());
		screen_line_counter = 0;
	}
}

MoreOutputBase::UserDecision MoreOutputBase::PromptUser()
{
	auto line_num = stream_line_counter;
	if (state != State::NewLineCR &&
	    state != State::NewLineLF) {
		++line_num;
	}

	bool prompt_type_line_num = false;

	auto display_prompt = [this, &line_num](const bool prompt_type_line_num) {
		// If using 40-column screen mode (or any custom one with less
		// columns than standard 80), use short prompts to avoid display
		// corruption
		const bool use_short_prompt = max_columns < 80;
		if (prompt_type_line_num) {
			WriteOut(MSG_Get("PROGRAM_MORE_PROMPT_LINE"), line_num);
		} else if (has_multiple_files) {
			WriteOut(MSG_Get(use_short_prompt
			                         ? "PROGRAM_MORE_PROMPT_SHORT"
			                         : "PROGRAM_MORE_PROMPT_MULTI"));
		} else {
			if (lines_in_stream) {
				const auto tmp = static_cast<float>(line_num) /
			                     static_cast<float>(lines_in_stream);
				WriteOut(MSG_Get(use_short_prompt
				                         ? "PROGRAM_MORE_PROMPT_SHORT_PERCENT"
				                         : "PROGRAM_MORE_PROMPT_PERCENT"),
				         std::min(static_cast<int>(tmp * 100), 100));
			} else {
				WriteOut(MSG_Get(
				        use_short_prompt
				                ? "PROGRAM_MORE_PROMPT_SHORT"
				                : "PROGRAM_MORE_PROMPT_SINGLE"));
			}
		}
	};

	auto erase_prompt = [this]() {
		WriteOut("\033[M"); // clear line
		auto counter = GetCursorColumn();
		while (counter--) {
			WriteOut("\033[D"); // cursor one position back
		}
	};

	screen_line_counter = 0;
	lines_to_display    = max_lines;
	lines_to_skip       = 0;

	if (is_output_redirected || has_option_no_paging) {
		// Don't ask user for anything if command output is redirected,
		// or if no-paging mode was requested, always continue
		return UserDecision::More;
	}

	if (GetCursorColumn()) {
		WriteOut("\n");
	}

	const auto column_start = GetCursorColumn();
	display_prompt(prompt_type_line_num);
	const auto column_end = GetCursorColumn();
	should_skip_pre_exit_prompt = true;

	if (column_start == column_end) {
		// Usually redirected output should be detected
		// till this point, but in a VERY special case
		// (only carriage return and ANSI sequences in
		// the input till now, cursor in one of the two
		// last rows, no file/device as a MORE command
		// argument) it will only be detected here
		WriteOut("\n");
		is_output_redirected = true;
		return UserDecision::More;
	}

	// Get user decision
	auto decision = UserDecision::Cancel;
	uint32_t num_lines = 0;
	while (true) {
		if (has_multiple_files) {
			decision = WaitForCancelContinueNext();
		} else {
			decision = WaitForCancelContinue();
		}

		if (decision == UserDecision::SwitchPrompt) {
			// User decided to switch the prompt type
			prompt_type_line_num = !prompt_type_line_num;
			erase_prompt();
			display_prompt(prompt_type_line_num);
			continue;
		}

		if (decision == UserDecision::MoreNumLines ||
		    decision == UserDecision::SkipNumLines) {
			erase_prompt();
			num_lines = GetNumLinesFromUser(decision);
			if (!num_lines && decision != UserDecision::Cancel) {
				erase_prompt();
				display_prompt(prompt_type_line_num);
				continue;
			}
		}

		if (decision == UserDecision::MoreOneLine) {
			lines_to_display = 1;
		} else if (decision == UserDecision::MoreNumLines) {
			lines_to_display = num_lines;
		} else if (decision == UserDecision::SkipNumLines) {
			lines_to_skip = num_lines;
		}

		// We have a valid decision
		break;
	}

	erase_prompt();

	if (decision == UserDecision::Cancel) {
		WriteOut(MSG_Get("PROGRAM_MORE_TERMINATE"));
		WriteOut("\n");
		++screen_line_counter;
	} else if (decision == UserDecision::NextFile) {
		WriteOut(MSG_Get("PROGRAM_MORE_NEXT_FILE"));
		WriteOut("\n");
		++screen_line_counter;
	}

	return decision;
}

MoreOutputBase::UserDecision MoreOutputBase::PromptUserIfNeeded()
{
	if (shutdown_requested) {
		return UserDecision::Cancel;
	}
	if (screen_line_counter >= lines_to_display) {
		return PromptUser();
	}
	return UserDecision::More;
}

uint32_t MoreOutputBase::GetNumLinesFromUser(UserDecision& decision)
{
	constexpr size_t max_digits = 5;

	WriteOut(MSG_Get("PROGRAM_MORE_HOW_MANY_LINES"));
	WriteOut(" ");

	std::string number_str = {};
	while (!shutdown_requested) {
		CALLBACK_Idle();

		// Try to read the key
		uint16_t count = 1;
		uint8_t code   = 0;
		DOS_ReadFile(STDIN, &code, &count);

		if (count == 0 || code == Ascii::CtrlC) {
			// Terminate the whole displaying
			decision = UserDecision::Cancel;
			break;
		} else if (code == Ascii::Escape) {
			// User has resigned, no number of lines
			break;
		} else if (code == Ascii::CarriageReturn && !number_str.empty()) {
			// ENTER pressed, we have a valid number
			return static_cast<uint32_t>(std::stoi(number_str));
		} else if (code == Ascii::Backspace && !number_str.empty()) {
			// BACKSPACE pressed
			WriteOut("%c %c", code, code);
			number_str.pop_back();
		} else if (std::isdigit(code) && number_str.length() < max_digits &&
		           !(code == '0' && number_str.empty())) {
			// Add a new digit to the number
			WriteOut("%c", code);
			number_str.push_back(static_cast<char>(code));
		}
	}

	return 0;
}

MoreOutputBase::UserDecision MoreOutputBase::WaitForCancelContinue()
{
	auto decision = UserDecision::NextFile;
	while (decision == UserDecision::NextFile) {
		decision = WaitForCancelContinueNext();
	}

	return decision;
}

MoreOutputBase::UserDecision MoreOutputBase::WaitForCancelContinueNext()
{
	auto decision = UserDecision::Cancel;
	while (!shutdown_requested) {
		CALLBACK_Idle();

		// Try to read the key
		uint16_t count = 1;
		uint8_t  tmp   = 0;
		DOS_ReadFile(STDIN, &tmp, &count);
		const char code = static_cast<char>(tmp);

		if (shutdown_requested || count == 0 || ciequals(code, 'q') ||
		    code == Ascii::CtrlC || code == Ascii::Escape) {
			decision = UserDecision::Cancel;
			break;
		} else if (code == ' ') {
			decision = UserDecision::More;
			break;
		} else if (code == Ascii::CarriageReturn) {
			if (has_option_expand_form_feed) {
				decision = UserDecision::More;
			} else {
				decision = UserDecision::MoreOneLine;
			}
			break;
		} else if (ciequals(code, 'n') || // FreeDOS hotkey
		           ciequals(code, 'f')) { // Windows hotkey
			decision = UserDecision::NextFile;
			break;
		} else if (has_option_extended_mode) {
			if (code == '=') {
				decision = UserDecision::SwitchPrompt;
				break;
			} else if (!has_option_expand_form_feed && ciequals(code, 'p')) {
				decision = UserDecision::MoreNumLines;
				break;
			} else if (!has_option_expand_form_feed && ciequals(code, 's')) {
				decision = UserDecision::SkipNumLines;
				break;
			}
		}
	}

	return decision;
}

bool MoreOutputBase::GetCharacter(char& code, bool& is_last_character)
{
	if (stream_line_counter == UINT32_MAX) {
		LOG_WARNING("DOS: MORE - stream too long");
		return false;
	}

	is_last_character = false;
	if (!tabs_remaining && !new_lines_ramaining) {
		bool should_skip_cr = (state == State::NewLineLF ||
		                       state == State::LineOverflow);
		bool should_skip_lf = (state == State::NewLineCR ||
		                       state == State::LineOverflow);

		while (true) {
			if (!GetCharacterRaw(code, is_last_character)) {
				return false; // end of data
			}

			if (should_end_on_ctrl_c && code == Ascii::CtrlC) {
				if (should_print_ctrl_c) {
					WriteOut("^C");
				}
				return false; // end by CTRL+C
			}

			// Update counter of lines in the input stream
			if ((last_fetched_code != Ascii::LineFeed &&
			     code == Ascii::CarriageReturn) ||
			    (last_fetched_code != Ascii::CarriageReturn &&
			     code == Ascii::LineFeed)) {
				++stream_line_counter;
			}
			last_fetched_code = code;

			// Skip one CR/LF characters for certain states
			if (code == Ascii::CarriageReturn && should_skip_cr) {
				should_skip_cr = false;
				continue;
			}
			if (code == Ascii::LineFeed && should_skip_lf) {
				should_skip_lf = false;
				continue;
			}

			break;
		}

		// If TAB found, replace it with spaces,
		// till we reach appropriate column
		if (code == '\t') {
			// TAB found, replace it with spaces,
			// till we reach appropriate column
			tabs_remaining    = option_tab_size;
			is_replacing_last = is_last_character;
			is_last_character = false;
		} else if (code == Ascii::FormFeed &&
		           (stream_line_counter >= start_line_num) &&
		           has_option_expand_form_feed) {
			// FormFeed found and appropriate option is set,
			// replace it with new lines until page is complete
			const auto tmp = std::max(0, max_lines - screen_line_counter - 1);
			new_lines_ramaining = static_cast<uint16_t>(tmp);
			lines_to_display    = max_lines;
			is_replacing_last   = is_last_character;
			is_last_character   = false;
		}
	}

	if (tabs_remaining) {
		code = ' ';
		--tabs_remaining;
		if ((column_counter + 1) % option_tab_size == 0) {
			tabs_remaining = 0;
		}
		is_last_character = is_replacing_last && !tabs_remaining;
	} else if (new_lines_ramaining) {
		code = '\n';
		--new_lines_ramaining;
		is_last_character = is_replacing_last && new_lines_ramaining;
	}

	return true;
}

uint8_t MoreOutputBase::GetCursorColumn()
{
	const auto page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	return CURSOR_POS_COL(page);
}

uint8_t MoreOutputBase::GetCursorRow()
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
}

std::string MoreOutputFiles::GetShortPath(const std::string &file_path,
                                          const char *msg_id)
{
	assert(msg_id);

	// We need to make sure the path and file name fits within
	// the designated space - if not, we have to shorten it.

	const auto max_len = GetMaxColumns() - std::strlen(MSG_Get(msg_id)) + 1;
	return shorten_path(file_path, max_len);
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

	// Since this CAN be STDIN input (there is no way to check),
	// CTRL+C shall quit
	should_end_on_ctrl_c = true;
	ClearScreenIfRequested();
	DisplaySingleStream();
}

void MoreOutputFiles::DisplayInputFiles()
{
	UserDecision decision = UserDecision::More;
	WriteOut("\n");

	bool should_skip_clear_screen = false;
	for (const auto &input_file : input_files) {
		decision = PromptUserIfNeeded();
		if (decision == UserDecision::Cancel) {
			break;
		}
		if (!should_skip_clear_screen) {
			ClearScreenIfRequested();
		}
		should_skip_clear_screen = false;

		// Print new line and detect command output
		// redirection; has to be called after printing
		// something not ending with a newline
		auto new_line_and_detect_redirect = [&]() {
			if (!GetCursorColumn()) {
				is_output_redirected = true;
			}
			WriteOut("\n");
			if (GetCursorColumn()) {
				is_output_redirected = true;
			}
		};

		if (!DOS_OpenFile(input_file.path.c_str(), 0, &input_handle)) {
			LOG_WARNING("DOS: MORE - could not open '%s'",
			            input_file.path.c_str());
			const auto short_path = GetShortPath(input_file.path,
			                                     "PROGRAM_MORE_OPEN_ERROR");
			WriteOut(MSG_Get("PROGRAM_MORE_OPEN_ERROR"),
			         short_path.c_str());
			new_line_and_detect_redirect();
			++screen_line_counter;
			if (UserDecision::Cancel ==
			    (decision = PromptUserIfNeeded())) {
				break;
			}
			should_skip_clear_screen = true;
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

		new_line_and_detect_redirect();
		++screen_line_counter;
		if (UserDecision::Cancel == (decision = PromptUserIfNeeded())) {
			break;
		}

		// If input from a device, CTRL+C shall quit
		should_end_on_ctrl_c = input_file.is_device;

		const auto d = DisplaySingleStream();
		DOS_CloseFile(input_handle);
		if (d == UserDecision::Cancel) {
			break;
		}
	}

	// End message and command prompt is going to appear; ensure the
	// scrolling won't make top lines disappear before user reads them
	const int free_rows_threshold = 2;
	if (!should_skip_pre_exit_prompt && !is_output_redirected &&
	    GetMaxLines() - screen_line_counter < free_rows_threshold) {
		decision = PromptUser();
	}

	if (decision != UserDecision::Cancel) {
		WriteOut(MSG_Get("PROGRAM_MORE_END"));
		WriteOut("\n");
	}
	WriteOut("\n");
}

bool MoreOutputFiles::GetCharacterRaw(char& code, bool& is_last_character)
{
	// Skip detecting if it is the last character for file/stream
	// mode - this is often problematic (like with STDIN input)
	// and wouldn't bring any user experience improvements due to
	// our 'end of input' message displayed at the end.
	is_last_character = false;

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

void MoreOutputStrings::ProcessEndOfLines()
{
	// Change the last CR/LF or LF/CR to a single end of the line symbol,
	// makes it easier to calculate 'is_last_character' value
	const auto length = input_strings.size();
	if (length >= 2) {
		const auto code1 = input_strings[length - 2];
		const auto code2 = input_strings[length - 1];
		if ((code1 == Ascii::LineFeed && code2 == Ascii::CarriageReturn) ||
		    (code1 == Ascii::CarriageReturn && code2 == Ascii::LineFeed)) {
			input_strings.pop_back();
		}
	}
}

void MoreOutputStrings::CountLines()
{
	// Calculate total number of lines
	if (!input_strings.empty()) {
		uint32_t lines = 1;

		bool ignore_next_cr = false;
		bool ignore_next_lf = false;
		for (const auto code : input_strings) {
			if ((code == Ascii::CarriageReturn && ignore_next_cr) ||
			    (code == Ascii::LineFeed && ignore_next_lf)) {
				ignore_next_cr = false;
				ignore_next_lf = false;
				continue;
			}

			if (code == Ascii::CarriageReturn) {
				ignore_next_lf = true;
				++lines;
			} else if (code == Ascii::LineFeed) {
				ignore_next_cr = true;
				++lines;
			}

			if (lines == UINT32_MAX) {
				LOG_WARNING("DOS: MORE - suspiciously long string to display");
				break;
			}
		}
		if (input_strings.back() == Ascii::CarriageReturn ||
		    input_strings.back() == Ascii::LineFeed) {
			--lines;
		}
		SetLinesInStream(lines);
	}

	ProcessEndOfLines();
}

bool MoreOutputStrings::CommonPrepare()
{
	if (SuppressWriteOut("")) {
		input_strings.clear();
		return false;
	}

	if (!is_continuation) {
		PrepareInternals();

		input_position = 0;

		has_multiple_files   = false;
		should_end_on_ctrl_c = false;
		is_continuation      = false;
		is_output_terminated = false;

		WriteOut("\n");
		ClearScreenIfRequested();
	}

	return true;
}

void MoreOutputStrings::Display()
{
	if (!CommonPrepare()) {
		return;
	}

	if (!is_continuation) {
		CountLines();
	}

	if (!is_output_terminated) {
		DisplaySingleStream();
	}

	is_continuation      = false;
	is_output_terminated = false;

	input_strings.clear();
	input_position = 0;
}

bool MoreOutputStrings::DisplayPartial()
{
	if (!CommonPrepare()) {
		is_continuation      = true;
		is_output_terminated = true;
		return false;
	}

	if (!is_output_terminated) {
		if (UserDecision::Cancel == DisplaySingleStream()) {
			is_output_terminated = true;
		}
	}

	is_continuation = true;

	input_strings.clear();
	input_position = 0;

	return !is_output_terminated;
}

bool MoreOutputStrings::GetCharacterRaw(char& code, bool& is_last_character)
{
	is_last_character = false;

	if (input_position >= input_strings.size()) {
		is_last_character = true;
		return false;
	}

	code = input_strings[input_position++];
	if (input_position == input_strings.size()) {
		is_last_character = true;
	}

	return true;
}
