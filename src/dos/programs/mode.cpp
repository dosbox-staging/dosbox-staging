// SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mode.h"

#include <algorithm>
#include <set>
#include <string>

#include "misc/ansi_code_markup.h"
#include "utils/checks.h"
#include "ints/int10.h"
#include "utils/math_utils.h"
#include "more_output.h"
#include "cpu/registers.h"
#include "shell/shell.h"
#include "utils/string_utils.h"

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

// `mode_str` must be in WxH format (lowercase `x`)
void MODE::SetDisplayMode(const std::string& mode_str)
{
	if (!is_valid_video_mode(mode_str)) {
		WriteOut(MSG_Get("PROGRAM_MODE_INVALID_DISPLAY_MODE"),
		         mode_str.c_str());
		return;
	}

	switch (machine) {
	case MachineType::Hercules:
		if (mode_str == "80x25") {
			INT10_SetVideoMode(0x07);
		} else {
			WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE"),
			         mode_str.c_str());
		}
		break;

	case MachineType::CgaMono:
	case MachineType::CgaColor:
	case MachineType::Tandy:
	case MachineType::Pcjr:
		if (mode_str == "40x25") {
			INT10_SetVideoMode(0x01);

		} else if (mode_str == "80x25") {
			INT10_SetVideoMode(0x03);

		} else {
			WriteOut(MSG_Get("PROGRAM_MODE_UNSUPPORTED_DISPLAY_MODE"),
			         mode_str.c_str());
		}
		break;

	case MachineType::Ega:
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

	case MachineType::Vga:
		if (svga_type == SvgaType::S3) {
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

void MODE::HandleSetDisplayMode()
{
	// Try to parse mode set via COLS=<cols> and LINES=<lines>
	constexpr auto RemoveArg = true;
	std::string cols_str;
	std::string lines_str;

	cmd->FindStringBegin("cols=", cols_str, RemoveArg);
	cmd->FindStringBegin("lines=", lines_str, RemoveArg);

	std::optional<int> maybe_cols  = {};
	std::optional<int> maybe_lines = {};

	if (!cols_str.empty()) {
		if (const auto maybe_int = parse_int(cols_str); maybe_int) {
			maybe_cols = maybe_int;
		} else {
			WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
			return;
		}
	}
	if (!lines_str.empty()) {
		if (const auto maybe_int = parse_int(lines_str); maybe_int) {
			maybe_lines = maybe_int;
		} else {
			WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
			return;
		}
	}

	if (maybe_cols || maybe_lines) {
		// Either COLS= or LINES= was found; attempt to set the display
		// mode

		const auto mode_str = format_str("%dx%d",
		                                 maybe_cols.value_or(80),
		                                 maybe_lines.value_or(25));
		SetDisplayMode(mode_str);
		return;
	}

	// Try to use the next remaining argument as the mode string
	if (cmd->GetCount() == 0) {
		return;
	}
	std::string mode_str = cmd->GetArguments()[0];
	lowcase(mode_str);

	// We accept either 'X' or ',' as separator, so these are all valid:
	//   80X25
	//   80x25
	//   80,25
	mode_str = replace(mode_str, ',', 'x');

	// Map symbolic mode names to COLxLINES format. We don't bother with
	// setting monochrome modes on CGA adapters (BW40, BW80, and MONO
	// options); we just want to set a standard 40/80 column mode for
	// compatibility with batch scripts (e.g., game installers).
	if (mode_str == "40" || mode_str == "bw40" || mode_str == "co40") {
		mode_str = "40x25";

	} else if (mode_str == "80" || mode_str == "bw80" ||
	           mode_str == "co80" || mode_str == "mono") {
		mode_str = "80x25";
	}

	SetDisplayMode(mode_str);
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

bool MODE::HandleSetTypematicRate()
{
	constexpr auto RemoveArg = true;
	std::string rate_str;
	std::string delay_str;

	cmd->FindStringBegin("rate=", rate_str, RemoveArg);
	cmd->FindStringBegin("delay=", delay_str, RemoveArg);

	const auto maybe_rate  = parse_int(rate_str);
	const auto maybe_delay = parse_int(delay_str);

	if (!maybe_rate && !maybe_delay) {
		return true;
	}

	if (maybe_rate && maybe_delay) {
		constexpr auto MinRate  = 1;
		constexpr auto MaxRate  = 32;
		constexpr auto MinDelay = 1;
		constexpr auto MaxDelay = 4;

		const auto rate = MaxRate - std::clamp(*maybe_rate, MinRate, MaxRate);
		const auto delay = std::clamp(*maybe_delay, MinDelay, MaxDelay) - 1;

		set_typematic_rate(rate, delay);
		return true;
	}

	WriteOut(MSG_Get("PROGRAM_MODE_INVALID_TYPEMATIC_RATE"));
	return false;
}

void MODE::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MODE_HELP_LONG"));
		output.Display();
		return;
	}

	if (cmd->GetCount() == 0) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		return;
	}

	// Ignore CON or CON: if it's the first argument
	auto maybe_device = cmd->GetArguments()[0];
	lowcase(maybe_device);
	if (maybe_device == "con" || maybe_device == "con:") {
		cmd->Shift(1);
	}

	if (!HandleSetTypematicRate()) {
		// Exit if there was an error
		return;
	}

	HandleSetDisplayMode();
}

void MODE::AddMessages()
{
	MSG_Add("PROGRAM_MODE_HELP_LONG",
	        "Set the display mode or the keyboard's typematic rate.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mode[reset] [[color=light-cyan]DEVICE[reset]] [color=white]COLS[reset]\n"
	        "  [color=light-green]mode[reset] [[color=light-cyan]DEVICE[reset]] [color=white]COLS,LINES[reset]\n"
	        "  [color=light-green]mode[reset] [[color=light-cyan]DEVICE[reset]] [color=white]MODENAME[reset]\n"
	        "  [color=light-green]mode[reset] [[color=light-cyan]DEVICE[reset]] cols=[color=white]COLS[reset] lines=[color=white]LINES[reset]\n"
	        "  [color=light-green]mode[reset] [[color=light-cyan]DEVICE[reset]] rate=[color=white]RATE[reset] delay=[color=white]DELAY[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DEVICE[reset]    CON or CON: (optional)\n"
	        "  [color=white]COLS[reset]      number of characters (columns) per line (40, 80, or 132)\n"
	        "  [color=white]LINES[reset]     number of lines on the screen (25, 28, 30, 34, 43, 50, or 60)\n"
	        "  [color=white]MODENAME[reset]  symbolic mode name (CO40, CO80, BW40, BW80, or MONO)\n"
	        "  [color=white]RATE[reset]      key repeat rate, from [color=white]1[reset] (slowest) to [color=white]32[reset] (fastest)\n"
	        "  [color=white]DELAY[reset]     key repeat delay, from [color=white]1[reset] (shortest) to [color=white]4[reset] (longest)\n"
	        "\n"
	        "Notes:\n"
	        "  - Valid display modes per graphics adapter:\n"
	        "      Hercules           80x25\n"
	        "      CGA, PCjr, Tandy   40x25, 80x25\n"
	        "      EGA                40x25, 80x25, 80x43\n"
	        "      SVGA (non-S3)      40x25, 80x25, 80x28, 80x30, 80x34, 80x43, 80x50\n"
	        "      SVGA (S3)          40x25, all 80 and 132-column modes\n"
	        "\n"
	        "  - The 132x28, 132x30, and 132x34 modes are only available if `vesa_modes` is\n"
	        "    set to `all`.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]mode[reset] [color=white]132x50[reset]\n"
	        "  [color=light-green]mode[reset] CON [color=white]80x43[reset]\n"
	        "  [color=light-green]mode[reset] [color=white]co80[reset]\n"
	        "  [color=light-green]mode[reset] CON: cols=[color=white]80[reset] lines=[color=white]43[reset]\n"
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
