/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#include "program_edit.h"

#include "program_more_output.h"

#include "checks.h"
#include "string_utils.h"

#include "tui/tui_application.h"
#include "tui/tui_menubar.h"
#include "tui/tui_screen.h"
#include "tui/tui_texteditor.h"

CHECK_NARROWING();


struct EditorConfig {
	bool has_arg_read_only     = false; // XXX
	bool has_arg_black_white   = false;
	bool has_arg_high_res      = false; // XXX
	bool has_arg_binary        = false; // XXX
	uint8_t arg_binary_row_len = 0;     // XXX
};

class EditScreen : public TuiScreen {
public:
	EditScreen(TuiApplication &application,
		   const std::vector<std::string> &file_names,
		   const EditorConfig &config);
private:
	std::shared_ptr<TuiMenuBar>    widget_menu_bar  = {};
	std::shared_ptr<TuiTextEditor> widget_text_editor = {};
};

EditScreen::EditScreen(TuiApplication &application,
                       const std::vector<std::string> &file_names, // XXX
	               const EditorConfig &config) : // XXX
	TuiScreen(application)
{
	application.SetBlackWhite(config.has_arg_black_white);

	widget_menu_bar    = Add<TuiMenuBar>();
	widget_text_editor = Add<TuiTextEditor>();

	widget_text_editor->SetPositionXY({0, 1});
	widget_text_editor->SetSizeXY({GetSizeX(), GetSizeY() - 1});

	SetFocus(widget_text_editor);
}

void EDIT::Run()
{
	constexpr int max_binary_row_length = 32; // XXX document this in help (check original edit.com?)

	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_EDIT_HELP_LONG"));
		output.Display();
		return;
	}

	EditorConfig config = {};

	// TODO: once LFN is implemented, support /s option (force using short
	// file names)

	constexpr bool remove_if_found = true;

	config.has_arg_read_only   = cmd->FindExist("/r", remove_if_found);
	config.has_arg_black_white = cmd->FindExist("/b", remove_if_found);
	config.has_arg_high_res    = cmd->FindExist("/h", remove_if_found);

	std::string tmp_str = {};
	std::optional<int> tmp_val = {};
	if (cmd->FindStringBegin("/", tmp_str, remove_if_found)) {
		tmp_val = parse_int(tmp_str);
		if (!tmp_val || *tmp_val == 0 || *tmp_val > max_binary_row_length ||
		    !is_digits(tmp_str)) {
			tmp_str = std::string("/") + tmp_str;
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp_str.c_str());
			return;
		}
		config.has_arg_binary = true;
		config.arg_binary_row_len = static_cast<uint8_t>(*tmp_val);
	}

	// Make sure no other switches are supplied
	if (cmd->FindStringBegin("/", tmp_str)) {
		tmp_str = std::string("/") + tmp_str;
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp_str.c_str());
		return;
	}

	// Retrieve file names from command line
	auto file_names = cmd->GetArguments();
	// XXX parse file pattern

	TuiApplication::Run<EditScreen>(*this, file_names, config);
}

void EDIT::AddMessages()
{
	MSG_Add("PROGRAM_EDIT_HELP_LONG",
	        "Edits text or binary files.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]edit[reset] \\[/b] [/h] [/r] [/[color=white]nnn[reset]] [[color=cyan]PATTERN[reset]...]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]PATTERN[reset] is either a path to a single file or a path with wildcards,\n"
	        "          which are the asterisk (*) and the question mark (?).\n"
	        "  /b      forces black&white display.\n"
	        "  /h      uses highest resolution scren mode posible.\n"
	        "  /r      opens files in read-only mode.\n"
	        "  /[color=white]nnn[reset]    opens files as binaries, displaying [color=white]nnn[reset] bytes in a row.\n"
	        "\n"
	        "Notes:\n"
	        "  Number of simultaneously opened files is limited to 20.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]edit[reset] /b [color=cyan]C:\\GAMELIST.TXT[reset]  ; opens a text file to edit\n");


	// XXX
}
