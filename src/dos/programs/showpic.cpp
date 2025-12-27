// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "showpic.h"

#include <algorithm>
#include <optional>

#include "cpu/callback.h"
#include "dos/dos.h"
#include "dos/dos_windows.h"
#include "dos/drives.h"
#include "hardware/pic.h"
#include "hardware/timer.h"
#include "hardware/video/vga.h"
#include "ints/int10.h"
#include "misc/dos_rwops.h"
#include "more_output.h"
#include "utils/checks.h"
#include "utils/rgb666.h"
#include "utils/rgb888.h"

#include <SDL.h>
#include <SDL_image.h>

CHECK_NARROWING();

void SHOWPIC::Run(void)
{
	// Print usage
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_SHOWPIC_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("PROGRAM_SHOWPIC_HELP_LONG"));
		output.Display();
		return;
	}

	if (svga_type != SvgaType::S3) {
		WriteOut(MSG_Get("PROGRAM_SHOWPIC_SVGA_S3_REQUIRED"));
		return;
	}

	// Check if Windows is running
	if (WINDOWS_IsStarted()) {
		WriteOut(MSG_Get("SHELL_CANT_RUN_UNDER_WINDOWS"));
		return;
	}

	// Load & show image
	const auto args = cmd->GetArguments();
	if (args.empty()) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		return;
	}

	const auto& filename = args[0];

	char path[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(filename.c_str(), path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}

	uint16_t handle = 0;
	if (!DOS_OpenFile(path, 0, &handle)) {
		WriteOut(MSG_Get("SHELL_FILE_NOT_FOUND"), path);
		return;
	}

	auto surface = IMG_Load_RW(create_sdl_rwops_for_dos_file(handle), 1);
	if (!surface) {
		WriteOut(MSG_Get("PROGRAM_SHOWPIC_LOAD_ERROR"), path);
		return;
	}

	auto palette = surface->format->palette;

	const auto is_paletted_image = (palette != nullptr);
	if (!is_paletted_image) {
		WriteOut(MSG_Get("PROGRAM_SHOWPIC_NOT_PALETTED_IMAGE"));
		return;
	}

	const auto last_video_mode = VGA_GetCurrentVideoMode().bios_mode_number;

	const auto maybe_video_mode = GetClosestVideoMode(surface->w,
	                                                  surface->h,
	                                                  palette->ncolors);
	if (!maybe_video_mode) {
		WriteOut(MSG_Get("PROGRAM_SHOWPIC_IMAGE_TOO_LARGE"));
		return;
	}
	const auto& video_mode = *maybe_video_mode;

	INT10_SetVideoMode(video_mode.mode);

	// Wait 10 ms to avoid screen flicker caused by writing into the video
	// memory before the mode change is completed.
	WaitForTicks(10);

	SetPalette(*palette);
	DisplayImage(*surface, video_mode.swidth, video_mode.sheight);
	WaitForKeypress();

	// To avoid flicker when switching back to text mode
	ClearScreen(video_mode.swidth, video_mode.sheight);

	INT10_SetVideoMode(last_video_mode);
}

void SHOWPIC::SetPalette(const SDL_Palette& palette) const
{
	// Ensure the 4 CGA colours are mapped to the first 4 VGA palette
	// indices and the 16 EGA colours to the first 16 indices
	for (uint8_t i = 0; i < NumCgaColors; ++i) {
		INT10_SetSinglePaletteRegister(i, i);
	}

	for (auto i = 0; i < palette.ncolors; ++i) {
		const auto c = palette.colors[i];

		// SDL_image normalises palette values to 24-bit RGB, but
		// standard VGA modes use 18-bit colours (6-bit per channel)
		const auto rgb666 = Rgb666::FromRgb888({c.r, c.g, c.b});

		INT10_SetSingleDACRegister(check_cast<uint8_t>(i),
		                           rgb666.red,
		                           rgb666.green,
		                           rgb666.blue);
	}
}

std::optional<VideoModeBlock> SHOWPIC::GetClosestVideoMode(const int width,
                                                           const int height,
                                                           const int num_colors) const
{
	std::optional<VideoModeBlock> best_mode = {};

	int best_distance = INT_MAX;

	for (auto& mode : ModeList_VGA) {
		// Disallow 2-color monochrome modes
		constexpr auto Monochrome_640x200 = 0x06;
		constexpr auto Monochrome_640x350 = 0x0f;
		constexpr auto Monochrome_640x480 = 0x11;

		if (mode.mode == Monochrome_640x200 ||
		    mode.mode == Monochrome_640x350 ||
		    mode.mode == Monochrome_640x480) {
			continue;
		}

		if (num_colors <= 4) {
			// Allow 4, 16 and 256-color modes
			if (!(mode.type == M_CGA4 || mode.type == M_EGA ||
			      mode.type == M_VGA || mode.type == M_LIN4 ||
			      mode.type == M_LIN8)) {
				continue;
			}
		} else if (num_colors <= 16) {
			// Allow 16 and 256-color modes
			if (!(mode.type == M_EGA || mode.type == M_VGA ||
			      mode.type == M_LIN4 || mode.type == M_LIN8)) {
				continue;
			}
		} else {
			// Only allow 256-color modes
			if (!(mode.type == M_VGA || mode.type == M_LIN8)) {
				continue;
			}
		}

		// Modes with fewer colors have lower mode numbers, so we'll pick
		// the mode that has just enough colours to display the picture
		// (e.g., a CGA mode if the image has only up to 4 colours).

		if (mode.swidth == width && mode.sheight == height) {
			// Perfect match; no need to look further
			return mode;
		}

		if (mode.swidth < width || mode.sheight < height) {
			// Video mode dimensions smaller than the image dimensions
			continue;
		}

		// Pick the video mode with dimensions closest to our image
		// dimensions
		const auto new_distance = (mode.swidth - width) +
		                          (mode.sheight - height);

		if (new_distance < best_distance) {
			best_mode = mode;
			best_distance = new_distance;
		}
	}

	return best_mode;
}

void SHOWPIC::DisplayImage(const SDL_Surface& surface, const int screen_width,
                           const int screen_height) const
{
	auto pixel_data = reinterpret_cast<uint8_t*>(surface.pixels);

	// Center image to the screen
	assert(screen_width >= surface.w);
	assert(screen_height >= surface.h);

	const auto xoffs = (screen_width - surface.w) / 2;
	const auto yoffs = (screen_height - surface.h) / 2;

	for (auto y = 0; y < surface.h; ++y) {
		for (auto x = 0; x < surface.w; ++x) {
			INT10_PutPixel(check_cast<uint16_t>(xoffs + x),
			               check_cast<uint16_t>(yoffs + y),
			               0,
			               pixel_data[x]);
		}
		pixel_data += surface.pitch;
	}
}

void SHOWPIC::ClearScreen(const uint16_t screen_width, const uint16_t screen_height) const
{
	for (uint16_t y = 0; y < screen_height; ++y) {
		for (uint16_t x = 0; x < screen_width; ++x) {
			INT10_PutPixel(x, y, 0, 0);
		}
	}
}

void SHOWPIC::WaitForTicks(const uint32_t num_ticks) const
{
	const auto ticks_start = PIC_Ticks;

	while (PIC_Ticks - ticks_start < num_ticks) {
		if (CALLBACK_Idle()) {
			break;
		}
	}
}

void SHOWPIC::WaitForKeypress() const
{
	uint8_t c;
	uint16_t n = 1;
	DOS_ReadFile(STDIN, &c, &n);
}

void SHOWPIC::AddMessages()
{
	MSG_Add("PROGRAM_SHOWPIC_HELP", "Display an image file.\n");

	MSG_Add("PROGRAM_SHOWPIC_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]showpic[reset] [color=light-cyan]FILE[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]FILE[reset]  name of a BMP, GIF, IFF/LBM, PCX, or PNG image file to display\n"
	        "\n"
	        "Notes:\n"
	        "  - An S3 SVGA display adapter is required.\n"
	        "  - Only paletted images are supported.\n"
	        "  - Press any key to exit the program.\n"
	        "  - You can use the program to view raw PNG screenshots created by DOSBox\n"
	        "    Staging (except for screenshots taken with composite emulation enabled).\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]showpic[reset] [color=light-cyan]image1.png[reset]\n"
	        "  [color=light-green]showpic[reset] [color=light-cyan]d:\\pics\\gods.iff[reset]\n");

	MSG_Add("PROGRAM_SHOWPIC_SVGA_S3_REQUIRED",
	        "This program requires an S3 SVGA adapter.\n");

	MSG_Add("PROGRAM_SHOWPIC_LOAD_ERROR", "Error loading image '%s'\n");

	MSG_Add("PROGRAM_SHOWPIC_NOT_PALETTED_IMAGE",
	        "Only paletted images are supported.");

	MSG_Add("PROGRAM_SHOWPIC_IMAGE_TOO_LARGE", "Image dimensions are too large.");
}
