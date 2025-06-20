/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2025  The DOSBox Staging Team
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

#include "program_mode.h"

#include <algorithm>
#include <set>
#include <string>

#include "../ints/int10.h"
#include "ansi_code_markup.h"
#include "checks.h"
#include "math_utils.h"
#include "program_more_output.h"
#include "regs.h"
#include "shell.h"
#include "string_utils.h"

CHECK_NARROWING();

// clang-format off
const std::map<std::string, uint16_t> video_mode_map_svga_s3 = {
        { "40x25", 0x001}, { "80x25", 0x003}, { "80x28", 0x070},
        { "80x30", 0x071}, { "80x34", 0x072}, { "80x43", 0x073},
        { "80x50", 0x074}, { "80x60", 0x043}, {"132x25", 0x109},
        {"132x28", 0x230}, {"132x30", 0x231}, {"132x34", 0x232},
        {"132x43", 0x10a}, {"132x50", 0x10b}, {"132x60", 0x10c}
};

const std::map<std::string, uint16_t> video_mode_map_svga_other = {
        { "40x25", 0x01}, { "80x25", 0x03}, { "80x28", 0x70},
        { "80x30", 0x71}, { "80x34", 0x72}, { "80x43", 0x73},
        { "80x50", 0x74}
};
// clang-format on

static bool is_valid_video_mode(const std::string& mode)
{
	// All modes are supported on the S3 SVGA adapter
	return video_mode_map_svga_s3.contains(mode);
}

static void set_typematic_rate(const int rate_idx, const int delay_idx)
{
	// Set Keyboard Typematic Rate
	reg_ah = 3;

	// set typematic rate/delay
	reg_al = 5;

	// typematic rate (repeats per second)
	//   0 = 30.0
	//   1 = 26.7
	//   2 = 24.0
	//   ...
	//  29 =  2.3
	//  30 =  2.1
	//  31 =  2.0
	reg_bl = check_cast<uint8_t>(rate_idx);

	// repeat delay
	//   0 =  250 ms
	//   2 =  750 ms
	//   1 =  500 ms
	//   3 = 1000 ms
	reg_bh = check_cast<uint8_t>(delay_idx);

	// Keyboard BIOS Services
	CALLBACK_RunRealInt(0x16);
}

static void set_8x8_font()
{
	// Load and activate ROM font
	reg_ah = 0x11;

	// 8x8 ROM font
	reg_al = 0x12;

	// Load font block 0
	reg_bl = 0;

	CALLBACK_RunRealInt(0x10);
}

void MODE::HandleSetDisplayMode(const std::string& _mode_str)
{
	auto mode_str = _mode_str;

	// These formats are all valid:
	//   80X25
	//   80x25
	//   80,25
	lowcase(mode_str);
	mode_str = replace(mode_str, ',', 'x');

	if (!is_valid_video_mode(mode_str)) {
		WriteOut(MSG_Get("PROGRAM_MODE_INVALID_DISPLAY_MODE"),
		         mode_str.c_str());
		return;
	}

	switch (machine) {
	case MCH_HERC:
		if (mode_str == "80x25") {
			INT10_SetVideoMode(0x07);
		} else {
			WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE"),
			         mode_str.c_str());
		}
		break;

	case MCH_CGA:
	case MCH_TANDY:
	case MCH_PCJR:
		if (mode_str == "40x25") {
			INT10_SetVideoMode(0x01);

		} else if (mode_str == "80x25") {
			INT10_SetVideoMode(0x03);

		} else {
			WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE"),
			         mode_str.c_str());
		}
		break;

	case MCH_EGA:
		if (mode_str == "40x25") {
			INT10_SetVideoMode(0x01);

		} else if (mode_str == "80x25") {
			INT10_SetVideoMode(0x03);

		} else if (mode_str == "80x43") {
			INT10_SetVideoMode(0x03);
			set_8x8_font();

		} else {
			WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE"),
			         mode_str.c_str());
		}
		break;

	case MCH_VGA:
		if (svgaCard == SVGA_S3Trio) {
			if (const auto it = video_mode_map_svga_s3.find(mode_str);
			    it != video_mode_map_svga_s3.end()) {

				const auto& mode = it->second;

				if (VESA_IsVesaMode(mode)) {
					if (INT10_FindSvgaVideoMode(mode)) {
						VESA_SetSVGAMode(mode);
					} else {
						WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_VESA_MODE"),
						         mode_str.c_str());
					}
				} else {
					INT10_SetVideoMode(mode);
				}
			} else {
				WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE"),
				         mode_str.c_str());
			}

		} else {
			// SVGA non-S3
			if (const auto it = video_mode_map_svga_other.find(mode_str);
			    it != video_mode_map_svga_other.end()) {

				const auto& mode = it->second;
				INT10_SetVideoMode(mode);
			} else {
				WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE"),
				         mode_str.c_str());
			}
		}
		break;

	default: assertm(false, "Invalid machine type");
	}
}

bool MODE::HandleSetTypematicRate()
{
	constexpr auto remove = false;

	std::string rate_str  = {};
	std::string delay_str = {};

	if (!cmd->FindStringBegin("rate=", rate_str, remove) ||
	    !cmd->FindStringBegin("delay=", delay_str, remove)) {
		return false;
	}

	const auto maybe_rate  = parse_int(rate_str);
	const auto maybe_delay = parse_int(delay_str);

	if (maybe_rate && maybe_delay) {
		constexpr auto MinRate  = 1;
		constexpr auto MaxRate  = 32;
		constexpr auto MinDelay = 1;
		constexpr auto MaxDelay = 4;

		const auto rate = MaxRate - std::clamp(*maybe_rate, MinRate, MaxRate);
		const auto delay = std::clamp(*maybe_delay, MinDelay, MaxDelay) - 1;

		set_typematic_rate(rate, delay);
	} else {
		WriteOut(MSG_Get("PROGRAM_MODE_INVALID_TYPEMATIC_RATE"));
	}
	return true;
}

void MODE::Run()
{
	// Handle command line

	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MODE_HELP_LONG"));
		output.Display();
		return;
	}

	auto is_set_display_mode_command = [](const std::string& command) {
		// Set display mode commands start with digits (e.g., 80x43)
		constexpr auto MinLength = sizeof("80x43") - 1;
		return isdigit(command[0]) && command.size() >= MinLength;
	};

	if (cmd->GetCount() == 0) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));

	} else if (cmd->GetCount() == 1) {
		const auto args    = cmd->GetArguments();
		const auto command = args[0];

		if (is_set_display_mode_command(command)) {
			const auto& mode = command;
			HandleSetDisplayMode(mode);
		} else {
			WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
		}

	} else if (cmd->GetCount() >= 2) {
		// To allow 'MODE CON: RATE=32 DELAY=1' too with minimal effort
		if (!HandleSetTypematicRate()) {
			WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
		}
	}
}

void MODE::AddMessages()
{
	MSG_Add("PROGRAM_MODE_HELP_LONG",
	        "Set the display mode or the keyboard's typematic rate.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mode[reset] [color=white]COLSxLINES[reset]\n"
	        "  [color=light-green]mode[reset] [color=white]COLS,LINES[reset]\n"
	        "  [color=light-green]mode[reset] rate=[color=white]RATE[reset] delay=[color=white]DELAY[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]COLS[reset]      number of characters (columns) per line (40, 80, or 132)\n"
	        "  [color=white]LINES[reset]     number of lines on the screen (25, 28, 30, 34, 43, 50, or 60)\n"
	        "  [color=white]RATE[reset]      key repeat rate, from [color=white]1[reset] to [color=white]32[reset] (1 = slowest, 32 = fastest)\n"
	        "  [color=white]DELAY[reset]     key repeat delay, from [color=white]1[reset] to [color=white]4[reset] (1 = shortest, 4 = longest)\n"
	        "\n"
	        "Notes:\n"
	        "  - Valid [color=white]COLSxLINES[reset] combinations per graphics adapter type:\n"
	        "      Hercules           80x25\n"
	        "      CGA, PCjr, Tandy   40x25, 80x25\n"
	        "      EGA                40x25, 80x25, 80x43\n"
	        "      SVGA (non-S3)      40x25, 80x25, 80x28, 80x30, 80x34, 80x43, 80x50\n"
	        "      SVGA (S3)          40x25, all 80 and 132-column modes\n"
	        "\n"
	        "  - The 132x28, 132x30, and 132x34 modes are only available if `vesa_modes`\n"
	        "    is set to `all`.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]mode[reset] [color=white]132x50\n"
	        "  [color=light-green]mode[reset] [color=white]80x43[reset]\n"
	        "  [color=light-green]mode[reset] rate=[color=white]32[reset] delay=[color=white]1[reset]");

	MSG_Add("PROGRAM_MODE_INVALID_DISPLAY_MODE",
	        "Invalid display mode: [color=white]%s[reset]");

	MSG_Add("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE",
	        "Display mode [color=white]%s[reset] is not supported on this graphics adapter.");

	MSG_Add("PROGRAM_MODE_UNSUPPORTED_VESA_MODE",
	        "VESA display mode [color=white]%s[reset] is not supported; set `vesa_modes = all` to enable it.");

	MSG_Add("PROGRAM_MODE_INVALID_TYPEMATIC_RATE",
	        "Invalid typematic rate setting.");
}
