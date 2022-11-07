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

	void SetTabSize(const uint8_t new_tab_size);

	virtual void Display() = 0;

protected:

	uint16_t GetMaxLines() const;
	uint16_t GetMaxColumns() const;

	void PrepareInternals();
	UserDecision DisplaySingleStream();
	UserDecision PromptUser();

	virtual bool GetCharacterRaw(char &code, bool &is_last) = 0;

	uint16_t line_counter = 0; // how many lines printed out since last user prompt

	bool was_prompt_recently  = false; // if next user prompt can be skipped
	bool has_multiple_files   = false; // if more than 1 file has to be displayed
	bool should_end_on_ctrl_c = false; // reaction on CTRL+C in the input
	bool should_print_ctrl_c  = false; // if Ctrl+C on input should print '^C'

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

	static uint8_t GetCurrentColumn();
	static uint8_t GetCurrentRow();
	bool GetCharacter(char &code, bool &is_last);

	uint16_t max_lines   = 0; // max number of lines to display between user prompts
	uint16_t max_columns = 0;

	uint8_t tab_size       = 8; // how many spaces to print for a TAB
	uint8_t tabs_remaining = 0; // how many spaces still to be printed for the current TAB
	bool    is_tab_last    = false;

	bool skip_next_cr = false;
	bool skip_next_lf = false;
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

	bool GetCharacterRaw(char &code, bool &is_last) override;

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
	bool GetCharacterRaw(char &code, bool &is_last) override;

	std::string input_strings = {};
	size_t input_position     = 0;
};

#endif
