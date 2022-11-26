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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_PROGRAM_MORE_OUTPUT_H
#define DOSBOX_PROGRAM_MORE_OUTPUT_H

#include "programs.h"

#include <string>

// ***************************************************************************
// Base class, only for internal usage
// ***************************************************************************

class MoreOutputBase {
public:
	MoreOutputBase(Program &program);
	virtual ~MoreOutputBase() = default;

	void SetOptionClear(const bool enabled);
	void SetOptionExtendedMode(const bool enabled);
	void SetOptionExpandFormFeed(const bool enabled);
	void SetOptionSquish(const bool enabled);
	void SetOptionStartLine(const uint32_t line_num);
	void SetOptionTabSize(const uint8_t tab_size);

	virtual void Display() = 0;

protected:
	enum class UserDecision {
		Cancel,
		More,
		MoreOneLine,
		MoreNumLines,
		SkipNumLines,
		NextFile,
		SwitchPrompt
	};

	void SetLinesInStream(const uint32_t lines);

	// Get cursor position from BIOS
	static uint8_t GetCursorColumn();
	static uint8_t GetCursorRow();

	uint16_t GetMaxLines() const;
	uint16_t GetMaxColumns() const;
	void ClearScreenIfRequested();

	void PrepareInternals();
	UserDecision DisplaySingleStream();
	UserDecision PromptUser();
	UserDecision PromptUserIfNeeded();

	virtual bool GetCharacterRaw(char &code, bool &is_last) = 0;

	enum class State {
		Normal,
		AnsiEsc,     // ANSI escape code started
		AnsiEscEnd,  // last character of ANSI escape code
		AnsiSci,     // ANSI control sequence started
		AnsiSciEnd,  // last character of ANSI control sequence
		NewLineCR,   // not ANSI, character code is Carriage Return
		NewLineLF,   // not ANSI, character code is Line Feed
		LineOverflow // line too long, cursor skipped to the next one
	};

	State state = State::Normal;

	uint16_t column_counter = 0;
	// How many lines printed out since last user prompt / start of stream
	uint16_t screen_line_counter = 0;
	uint32_t stream_line_counter = 0;

	bool is_output_redirected = false;
	bool has_multiple_files   = false; // if more than 1 file has to be displayed
	bool should_end_on_ctrl_c = false; // reaction on CTRL+C in the input
	bool should_print_ctrl_c  = false; // if Ctrl+C on input should print '^C'

	// If true, we can safely skip a 'dummy' prompt, which normally
	// prevents the DOS prompt after the command execution to hide
	// lines not yet read by the user
	bool should_skip_pre_exit_prompt = false;

	// Wrappers for Program:: methods

	template <typename... Arguments>
	void WriteOut(const char *format, Arguments... arguments)
	{
		program.WriteOut(format, arguments...);
	}

	bool SuppressWriteOut(const char *format)
	{
		return program.SuppressWriteOut(format);
	}

private:
	Program &program;

	bool GetCharacter(char& code, bool& is_last_character);

	uint32_t GetNumLinesFromUser(UserDecision& decision);
	UserDecision WaitForCancelContinue();
	UserDecision WaitForCancelContinueNext();

	uint16_t max_lines   = 0; // max lines to display between user prompts
	uint16_t max_columns = 0;

	// Command line options
	bool has_option_clear  = false; // true = clear screen at start of each stream
	bool has_option_squish = false; // true = squish multiple empty lines into one
	bool has_option_extended_mode    = false;
	bool has_option_expand_form_feed = false;
	uint8_t option_tab_size = 8; // how many spaces to print for a TAB
	uint32_t option_start_line_num = 0;

	// Line number to start displaying from
	uint32_t start_line_num = 0;
	// Number of lines to display before the user prompt
	uint32_t lines_to_display = 0;
	// Number of lines to skip before displaying the input
	uint32_t lines_to_skip = 0;
	// Total number of lines in the input, 0 for unknown
	uint32_t lines_in_stream = 0;

	// How many spaces still to be printed for the current TAB
	uint8_t tabs_remaining = 0;
	// How many new lines lines still to be printed instead of FormFeed
	uint16_t new_lines_ramaining = 0;
	// Is the character we are replacing the last one in the file
	bool is_replacing_last = false;

	char last_fetched_code = 0; // code of previously fetched character, or 0
};

// ***************************************************************************
// Output file/device/stream content via MORE
// ***************************************************************************

class MoreOutputFiles final : public MoreOutputBase {
public:
	MoreOutputFiles(Program &program);

	void AddFile(const std::string &file_path, const bool is_device);
	void Display() override;

private:
	void DisplayInputFiles();
	void DisplayInputStream();

	std::string GetShortPath(const std::string &file_path, const char *msg_id);

	bool GetCharacterRaw(char& code, bool& is_last_character) override;

	struct InputFile {
		std::string path = "";
		bool is_device   = false; // whether this is a regular file or a device
	};

	std::vector<InputFile> input_files = {};
	uint16_t input_handle = 0; // DOS handle of the input stream
};

// ***************************************************************************
// Output string content via MORE
// ***************************************************************************

class MoreOutputStrings final : public MoreOutputBase {
public:
	MoreOutputStrings(Program &program);

	void AddString(const char *format, ...);
	void Display() override;

private:
	bool GetCharacterRaw(char& code, bool& is_last_character) override;

	std::string input_strings = {};
	size_t input_position     = 0;
};

#endif
