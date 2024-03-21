/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2002-2024  The DOSBox Team
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

#include "program_keyb.h"

#include "ansi_code_markup.h"
#include "dos_keyboard_layout.h"
#include "dos_locale.h"
#include "program_more_output.h"
#include "string_utils.h"

void KEYB::Run(void)
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_KEYB_HELP_LONG"));
		output.Display();
		return;
	}

	// No arguments: print code page and keybaord layout ID
	if (!cmd->FindCommand(1, temp_line)) {
		WriteOutSuccess();
		return;
	}

	// One argument: The language/country. We'll infer the codepage
	int32_t tried_cp        = -1; // default to auto
	KeyboardErrorCode rcode = KEYB_LAYOUTNOTFOUND;
	if (cmd->GetCount() == 1) {
		rcode = DOS_LoadKeyboardLayoutFromLanguage(temp_line.c_str());
	}

	// Two or more arguments: language/country and a specific codepage
	else if (std::string cp_string; cmd->FindCommand(2, cp_string)) {
		// second parameter is codepage number
		tried_cp = atoi(cp_string.c_str());

		// Possibly a third parameter, the codepage file
		std::string cp_filename = "auto";
		(void)cmd->FindCommand(3, cp_filename); // fallback to auto

		rcode = DOS_LoadKeyboardLayout(temp_line.c_str(),
		                               tried_cp,
		                               cp_filename.c_str());
	}

	// Switch if loading the layout succeeded
	if (rcode == KEYB_NOERROR) {
		rcode = DOS_SwitchKeyboardLayout(temp_line.c_str(), tried_cp);
	}

	// Print out the result
	if (rcode == KEYB_NOERROR) {
		WriteOutSuccess();
	} else {
		WriteOutFailure(rcode, temp_line, tried_cp);
	}
}

void KEYB::WriteOutFailure(const KeyboardErrorCode error_code,
		           const std::string &layout,
		           const int code_page)
{
	switch (error_code)
	{
	case KEYB_FILENOTFOUND:
		WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"), layout.c_str());
		break;
	case KEYB_INVALIDFILE:
		WriteOut(MSG_Get("PROGRAM_KEYB_INVALIDFILE"), layout.c_str());
		break;
	case KEYB_LAYOUTNOTFOUND:
		WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUTNOTFOUND"),
		         layout.c_str(),
		         code_page);
		break;
	case KEYB_INVALIDCPFILE:
		WriteOut(MSG_Get("PROGRAM_KEYB_INVCPFILE"), layout.c_str());
		break;
	default:
		LOG(LOG_DOSMISC, LOG_ERROR)
		("KEYB:Invalid returncode %x", static_cast<uint8_t>(error_code));
		break;
	}
}

void KEYB::WriteOutSuccess()
{
	constexpr size_t normal_spacing_size = 2;
	constexpr size_t large_spacing_size  = 4;

	const std::string ansi_white  = "[color=white]";
	const std::string ansi_yellow = "[color=yellow]";
	const std::string ansi_reset  = "[reset]";

	const std::string quote_start_end   = "'";
	const std::string hyphen_separation = " - ";

	const std::string layout = DOS_GetLoadedLayout();
	const bool show_scripts  = !layout.empty();

	// Prepare strings based on translation

	std::string code_page_msg = MSG_Get("PROGRAM_KEYB_CODE_PAGE");
	std::string layout_msg    = MSG_Get("PROGRAM_KEYB_KEYBOARD_LAYOUT");
	std::string script_msg    = MSG_Get("PROGRAM_KEYB_KEYBOARD_SCRIPT");

	const auto code_page_len = code_page_msg.length();
	const auto layout_len    = layout_msg.length();
	const auto script_len    = script_msg.length();

	auto target_len = std::max(code_page_len, layout_len);
	if (show_scripts) {
		target_len = std::max(target_len, script_len);
	}
	target_len += normal_spacing_size;

	const auto code_page_diff = target_len - code_page_len;
	const auto layout_diff    = target_len - layout_len;
	const auto script_diff    = target_len - script_len;

	code_page_msg = ansi_white + code_page_msg + ansi_reset;
	layout_msg    = ansi_white + layout_msg + ansi_reset;

	code_page_msg.resize(code_page_msg.size() + code_page_diff, ' ');
	layout_msg.resize(layout_msg.size() + layout_diff, ' ');

	if (show_scripts) {
		script_msg = ansi_white + script_msg + ansi_reset;
		script_msg.resize(script_msg.size() + script_diff, ' ');
	}

	// Prepare message

	std::string message = {};

	auto print_message = [&]() {
		message += "\n";
		WriteOut(convert_ansi_markup(message).c_str());
	};

	// Start with code page and keyboard layout

	message += code_page_msg + std::to_string(dos.loaded_codepage) + "\n";

	message += layout_msg;
	if (!show_scripts) {
		message += MSG_Get("PROGRAM_KEYB_NOT_LOADED");
	} else {
		message += quote_start_end + layout + quote_start_end;
		message += hyphen_separation + DOS_GetKeyboardLayoutName(layout);
	}
	message += "\n";

	if (!show_scripts) {
		print_message();
		return;
	}

	// If we have a keyboard layout, add script(s) information

	const auto script1 = DOS_GetKeyboardLayoutScript1(layout);
	const auto script2 = DOS_GetKeyboardLayoutScript2(layout, dos.loaded_codepage);
	const auto script3 = DOS_GetKeyboardLayoutScript3(layout, dos.loaded_codepage);

	assert(script1); // main script should be available, always

	std::vector<std::pair<std::string, std::string>> table = {};

	if (script1) {
		table.push_back({ DOS_GetKeyboardScriptName(*script1),
			          DOS_GetShortcutKeyboardScript1() });
	}
	if (script2) {
		table.push_back({ DOS_GetKeyboardScriptName(*script2),
			          DOS_GetShortcutKeyboardScript2() });
	}
	if (script3) {
		table.push_back({ DOS_GetKeyboardScriptName(*script3),
			          DOS_GetShortcutKeyboardScript3() });
	}

	const bool show_shortcuts = (table.size() > 1);

	if (show_shortcuts) {
		size_t max_length = 0;
		for (const auto &entry : table) {
			max_length = std::max(max_length, entry.first.length());
		}

		for (auto &entry : table) {
			entry.first.resize(max_length, ' ');
		}
	}

	const auto margin = std::string(target_len, ' ');

	bool first_row = true;
	for (auto &entry : table) {
		if (first_row) {
			message += script_msg;
			first_row = false;
		} else {
			message += margin;
		}

		message += entry.first;
		if (show_shortcuts) {
			message += std::string(large_spacing_size, ' ');
			message += ansi_yellow + entry.second + ansi_reset;
		}
		message += "\n";
	}

	print_message();
}

void KEYB::AddMessages()
{
	MSG_Add("PROGRAM_KEYB_HELP_LONG",
	        "Configure a keyboard for a specific language.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]keyb[reset] [color=light-cyan][LANGCODE][reset]\n"
	        "  [color=light-green]keyb[reset] [color=light-cyan]LANGCODE[reset] [color=white]CODEPAGE[reset] [color=white][CODEPAGEFILE][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]LANGCODE[reset]      language code or keyboard layout ID\n"
	        "  [color=white]CODEPAGE[reset]      code page number, such as [color=white]437[reset] and [color=white]850[reset]\n"
	        "  [color=white]CODEPAGEFILE[reset]  file containing information for a code page\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]keyb[reset] without an argument shows the currently loaded keyboard layout\n"
	        "  and code page. It will change to [color=light-cyan]LANGCODE[reset] if provided, optionally with a\n"
	        "  [color=white]CODEPAGE[reset] and an additional [color=white]CODEPAGEFILE[reset] to load the specified code page\n"
	        "  number and code page file if provided. This command is especially useful if\n"
	        "  you use a non-US keyboard, and [color=light-cyan]LANGCODE[reset] can also be set in the configuration\n"
	        "  file under the [dos] section using the \"keyboardlayout = [color=light-cyan]LANGCODE[reset]\" setting.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]KEYB[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]uk[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]sp[reset] [color=white]850[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]de[reset] [color=white]858[reset] mycp.cpi\n");
	// Success/status message
	MSG_Add("PROGRAM_KEYB_CODE_PAGE",       "Code page");
	MSG_Add("PROGRAM_KEYB_KEYBOARD_LAYOUT", "Keyboard layout");
	MSG_Add("PROGRAM_KEYB_KEYBOARD_SCRIPT", "Keyboard script");
	MSG_Add("PROGRAM_KEYB_NOT_LOADED",      "not loaded");
	// Error messages
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND",
		"Keyboard file %s not found.\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE",
		"Keyboard file %s invalid.\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND",
		"No layout in %s for codepage %i.\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE",
		"None or invalid codepage file for layout %s.\n");
}
