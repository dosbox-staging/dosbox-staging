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
#include "dos_locale.h"
#include "program_more_output.h"
#include "string_utils.h"

#include <iostream>

void KEYB::Run(void)
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_KEYB_HELP_LONG"));
		output.Display();
		return;
	}

	constexpr bool remove_if_found = true;
	const bool has_option_list = cmd->FindExist("/list", remove_if_found);
	const bool has_option_rom  = cmd->FindExist("/rom", remove_if_found);

	if (has_option_list && has_option_rom) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH_COMBO"));
		return;
	}

	if (has_option_list) {
		if (cmd->GetCount() > 0) {
			WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
			return;
		}

		ListKeyboardLayouts();
		return;
	}

	const auto params = cmd->GetArguments();
	if (params.empty()) {
		// No arguments: print code page and keyboard layout ID
		WriteOutSuccess();
		return;
	}

	if (params.size() > 3) {
		WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
		return;
	}

	// Fetch keyboard layout
	const auto& keyboard_layout = params[0];

	// Fetch CPI file name
	const std::string cpi_file = (params.size() >= 3) ? params[2] : "";

	if (has_option_rom && !cpi_file.empty()) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH_COMBO"));
		return;
	}

	// Fetch code page
	std::optional<uint16_t> code_page = {};
	if (params.size() >= 2) {
		const auto value = parse_int(params[1]);
		if (!value || (*value < 1) || (*value > UINT16_MAX)) {
			WriteOut(MSG_Get("PROGRAM_KEYB_INVALID_CODE_PAGE"));
			return;
		}

		code_page = static_cast<uint16_t>(*value);
	}

	const uint16_t requested_code_page = code_page ? *code_page : 0;

	uint16_t tried_code_page   = requested_code_page;
	const bool prefer_rom_font = !code_page || has_option_rom;

	const auto result = DOS_LoadKeyboardLayout(keyboard_layout,
	                                           tried_code_page,
	                                           cpi_file,
	                                           prefer_rom_font);
	if (result != KeyboardLayoutResult::OK) {
		WriteOutFailure(result, keyboard_layout, requested_code_page, tried_code_page);
		return;
	}

	WriteOutSuccess();
}

void KEYB::ListKeyboardLayouts()
{
	constexpr bool for_keyb_command = true;

	const auto message = DOS_GenerateListKeyboardLayoutsMessage(for_keyb_command);

	MoreOutputStrings output(*this);
	output.AddString(message.c_str());
	output.Display();
}

void KEYB::WriteOutFailure(const KeyboardLayoutResult error_code,
                           const std::string& layout,
                           const uint16_t requested_code_page,
                           const uint16_t tried_code_page)
{
	switch (error_code) {
	case KeyboardLayoutResult::LayoutFileNotFound:
		WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUT_FILE_NOT_FOUND"),
		         layout.c_str());
		break;
	case KeyboardLayoutResult::InvalidLayoutFile:
		WriteOut(MSG_Get("PROGRAM_KEYB_INVALID_LAYOUT_FILE"),
		         layout.c_str());
		break;
	case KeyboardLayoutResult::CpiFileNotFound:
		WriteOut(MSG_Get("PROGRAM_KEYB_CPI_FILE_NOT_FOUND"));
		break;
	case KeyboardLayoutResult::InvalidCpiFile:
		WriteOut(MSG_Get("PROGRAM_KEYB_INVALID_CPI_FILE"));
		break;
	case KeyboardLayoutResult::UnsupportedCpiFileDrDos:
		WriteOut(MSG_Get("PROGRAM_KEYB_CPI_FILE_DR_DOS"));
		break;
	case KeyboardLayoutResult::LayoutNotKnown:
		WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUT_NOT_KNOWN"),
		                layout.c_str());
		break;
        case KeyboardLayoutResult::NoLayoutForCodePage:
		WriteOut(MSG_Get("PROGRAM_KEYB_NO_LAYOUT_FOR_CODE_PAGE"),
		                layout.c_str(),
		                requested_code_page);
		break;
	case KeyboardLayoutResult::NoBundledCpiFileForCodePage:
		WriteOut(MSG_Get("PROGRAM_KEYB_NO_BUNDLED_CPI_FILE"), tried_code_page);
		break;
	case KeyboardLayoutResult::NoCodePageInCpiFile:
		WriteOut(MSG_Get("PROGRAM_KEYB_NO_CODE_PAGE_IN_FILE"),
		         tried_code_page);
		break;
	case KeyboardLayoutResult::IncompatibleMachine:
		WriteOut(MSG_Get("PROGRAM_KEYB_INCOMPATIBLE_MACHINE"));
		break;
	default:
		LOG_WARNING("KEYB:Invalid return code %x", enum_val(error_code));
		assert(false);
		break;
	}
}

void KEYB::WriteOutSuccess()
{
	constexpr size_t NormalSpacingSize = 2;
	constexpr size_t LargeSpacingSize  = 4;

	const std::string AnsiWhite  = "[color=white]";
	const std::string AnsiYellow = "[color=yellow]";
	const std::string AnsiReset  = "[reset]";

	const std::string hyphen_separation = " - ";

	const auto layout       = DOS_GetLoadedLayout();
	const bool show_scripts = !layout.empty();

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
	target_len += NormalSpacingSize;

	const auto code_page_diff = target_len - code_page_len;
	const auto layout_diff    = target_len - layout_len;
	const auto script_diff    = target_len - script_len;

	code_page_msg = AnsiWhite + code_page_msg + AnsiReset;
	layout_msg    = AnsiWhite + layout_msg + AnsiReset;

	code_page_msg.resize(code_page_msg.size() + code_page_diff, ' ');
	layout_msg.resize(layout_msg.size() + layout_diff, ' ');

	if (show_scripts) {
		script_msg = AnsiWhite + script_msg + AnsiReset;
		script_msg.resize(script_msg.size() + script_diff, ' ');
	}

	// Prepare message

	std::string message = "\n";

	auto print_message = [&]() {
		message += "\n";
		WriteOut(convert_ansi_markup(message).c_str());
	};

	// Start with code page and keyboard layout

	message += code_page_msg + std::to_string(dos.loaded_codepage);

	const auto code_page_font = DOS_GetCodePageFont();
	if (code_page_font != CodePageFont::Bundled) {
		message += " (";
		switch (code_page_font) {
		case CodePageFont::Rom:
			message += MSG_Get("PROGRAM_KEYB_ROM_FONT");
			break;
		case CodePageFont::Custom:
			message += MSG_Get("PROGRAM_KEYB_CUSTOM_FONT");
			break;
		default:
			message += "???";
			assert(false);
			break;
		}
		message += ")";
	}
	message += "\n";

	message += layout_msg;
	if (!show_scripts) {
		message += MSG_Get("PROGRAM_KEYB_NOT_LOADED");
	} else {
		const char Apostrophe = '\'';

		message += Apostrophe + layout + Apostrophe;
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
		table.push_back({DOS_GetKeyboardScriptName(*script1),
		                 DOS_GetShortcutKeyboardScript1()});
	}
	if (script2) {
		table.push_back({DOS_GetKeyboardScriptName(*script2),
		                 DOS_GetShortcutKeyboardScript2()});
	}
	if (script3) {
		table.push_back({DOS_GetKeyboardScriptName(*script3),
		                 DOS_GetShortcutKeyboardScript3()});
	}

	const bool show_shortcuts = (table.size() > 1);

	if (show_shortcuts) {
		size_t max_length = 0;
		for (const auto& entry : table) {
			max_length = std::max(max_length, entry.first.length());
		}

		for (auto& entry : table) {
			entry.first.resize(max_length, ' ');
		}
	}

	const auto margin = std::string(target_len, ' ');

	bool first_row = true;
	for (auto& entry : table) {
		if (first_row) {
			message += script_msg;
			first_row = false;
		} else {
			message += margin;
		}

		message += entry.first;
		if (show_shortcuts) {
			message += std::string(LargeSpacingSize, ' ');
			message += AnsiYellow + entry.second + AnsiReset;
		}
		message += "\n";
	}

	const auto warn = DOS_GetCodePageWarning(dos.loaded_codepage);
	if (warn) {
		message += "\n";
		message += format_str(MSG_Get("PROGRAM_KEYB_WARNING_CODE_PAGE"),
		                      dos.loaded_codepage);
		message += "\n";
		switch (*warn) {
		case CodePageWarning::DottedI:
			message += MSG_Get("PROGRAM_KEYB_WARNING_DOTTED_I");
			break;
		default: assert(false); break;
		}
		message += "\n";
	}

	print_message();
}

void KEYB::AddMessages()
{
	MSG_Add("PROGRAM_KEYB_HELP_LONG",
	        "Configure a keyboard layout and screen font.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]keyb[reset]\n"
	        "  [color=light-green]keyb[reset] /list\n"
	        "  [color=light-green]keyb[reset] [color=light-cyan]LAYOUT[reset] [[color=white]CODEPAGE[reset]] /rom\n"
	        "  [color=light-green]keyb[reset] [color=light-cyan]LAYOUT[reset] [[color=white]CODEPAGE[reset] [[color=white]CPIFILE[reset]]]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]LAYOUT[reset]    keyboard layout code\n"
	        "  [color=white]CODEPAGE[reset]  code page number, such as [color=white]437[reset] or [color=white]850[reset]\n"
	        "  [color=white]CPIFILE[reset]   screen font file, in CPI or CPX format\n"
	        "  /list     displays available keyboard layout codes\n"
	        "  /rom      if possible, uses screen font from display adapter ROM\n"
	        "\n"
	        "Notes:\n"
	        "  - Running [color=light-green]keyb[reset] without an argument shows the currently loaded keyboard layout\n"
	        "    and code page.\n"
	        "  - The [color=white]CPIFILE[reset], if specified, must contain screen font for the given [color=white]CODEPAGE[reset].\n"
	        "  - If no custom [color=white]CPIFILE[reset] is specified, it tries to find a suitable screen font\n"
	        "    in one of the bundled CPI files.\n"
	        "  - If [color=white]CODEPAGE[reset] is not specified, it tries use the screen font from the display\n"
	        "    adapter ROM first, just like when using /rom switch.\n"
	        "  - Only EGA or better display adapters allow to change the screen font; MDA,\n"
	        "    CGA, or Hercules always use the ROM screen font.\n"
	        "  - The 'us' keyboard layout can be used with any code page; all the other\n"
	        "    layouts work with selected code pages only.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]KEYB[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]uk[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]sp[reset] [color=white]850[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]de[reset] [color=white]858[reset] mycp.cpi\n");
	// Success/status message
	MSG_Add("PROGRAM_KEYB_CODE_PAGE", "Code page");
	MSG_Add("PROGRAM_KEYB_ROM_FONT", "ROM font");
	MSG_Add("PROGRAM_KEYB_CUSTOM_FONT", "custom font");
	MSG_Add("PROGRAM_KEYB_KEYBOARD_LAYOUT", "Keyboard layout");
	MSG_Add("PROGRAM_KEYB_KEYBOARD_SCRIPT", "Keyboard script");
	MSG_Add("PROGRAM_KEYB_NOT_LOADED", "not loaded");
	// Warnings
	MSG_Add("PROGRAM_KEYB_WARNING_CODE_PAGE",
	        "[color=light-red]Warning:[reset] It is recommended to avoid using code page %d!");
	MSG_Add("PROGRAM_KEYB_WARNING_DOTTED_I",
	        "It replaces the standard ASCII dotless letter 'I' with a national dotted\n"
	        "variant and moves the original character elsewhere. There is no way to fully\n"
	        "handle this without risking compatibility problems with existing software!");
	// Error messages
	MSG_Add("PROGRAM_KEYB_INVALID_CODE_PAGE", "Invalid code page.\n");
	MSG_Add("PROGRAM_KEYB_LAYOUT_FILE_NOT_FOUND",
	        "File with keyboard layout '%s' not found.\n");
	MSG_Add("PROGRAM_KEYB_INVALID_LAYOUT_FILE",
	        "Invalid file with keyboard layout '%s'.\n");
	MSG_Add("PROGRAM_KEYB_CPI_FILE_NOT_FOUND",
	        "Code page information file not found.\n");
	MSG_Add("PROGRAM_KEYB_INVALID_CPI_FILE", "Invalid code page file.\n");
	MSG_Add("PROGRAM_KEYB_CPI_FILE_DR_DOS",
	        "Code page information file has unsupported DR-DOS format.\n");
	MSG_Add("PROGRAM_KEYB_LAYOUT_NOT_KNOWN", "Keyboard layout '%s' not known.\n");
	MSG_Add("PROGRAM_KEYB_NO_LAYOUT_FOR_CODE_PAGE",
	        "No keyboard layout '%s' for code page %d.\n");
	MSG_Add("PROGRAM_KEYB_NO_BUNDLED_CPI_FILE",
	        "No bundled code page information file for code page %d.\n");
	MSG_Add("PROGRAM_KEYB_NO_CODE_PAGE_IN_FILE",
	        "No code page %d in the code page information file.\n");
	MSG_Add("PROGRAM_KEYB_INCOMPATIBLE_MACHINE",
	        "Can't change the code page; EGA machine or better is required.\n");
}
