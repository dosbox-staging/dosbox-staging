// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/auto_image_adjustments.h"

#include "private/shader_manager.h"

#include "utils/checks.h"

CHECK_NARROWING();

// Our monochrome palettes assume standard sRGB, so 6500 K sRGB white point
// and no CRT colour profile emulation will result in the intended colours.
//
static constexpr AutoImageAdjustments monochrome_settings = {
        0.0f,                  // black level
        CrtColorProfile::None, // colour profile
        6500.0f                // colour temperature
};

// Composite PC monitors from the 1980s were basically repurposed small NTSC
// TVs, so SMPT-C phosphors and the standard 6500 K NTSC white point is the
// most appropriate.
//
static constexpr AutoImageAdjustments composite_settings = {
        0.53f,                   // black level
        CrtColorProfile::SmpteC, // colour profile
        6500.0f                  // colour temperature
};

// High-resolution early CGA and EGA monitors from the 1980s used high colour
// temperatures to maximise the brightness of these relatively dim display (it
// was hard to produce high-resolution monitors for the display of
// 80-character text with sufficient brightness in the early days). The 9300K
// white point also softens the garish look of the CGA/EGA palette.
//
static constexpr AutoImageAdjustments cga_settings = {
        0.65f,                // black level
        CrtColorProfile::P22, // colour profile
        9300.0f               // colour temperature
};

static constexpr AutoImageAdjustments ega_settings = {
        0.60f,                // black level
        CrtColorProfile::P22, // colour profile
        9300.0f               // colour temperature
};

// VGA monitors from the 1990s started to converge towards warmer colour
// temperatures. They were still on the cold, blueish looking side, though,
// but as there was no real standard, colour temperatures were all over the
// place (the sRGB standard that stipulates 6500 K for consumer computer
// monitors only came out in 1999, after the end of the DOS era). Between
// about 7500 and 9000 K is typical of this era and the exact value varies by
// monitor (colour accuracy was not a consideration for consumer VGA monitors
// at all).
//
static constexpr AutoImageAdjustments vga_settings = {
        0.0f,                 // black level
        CrtColorProfile::P22, // colour profile
        7800.0f               // colour temperature
};

// This emulates the colours of a Commodore 1084S and Philips CM8833-II 15 kHz
// home computer monitor.
//
static constexpr AutoImageAdjustments arcade_settings = {
        0.50f,                    // black level
        CrtColorProfile::Philips, // colour profile
        6500.0f                   // colour temperature
};

std::optional<AutoImageAdjustments> AutoImageAdjustmentsManager::GetSettings(
        const VideoMode& video_mode) const
{
	using enum ShaderMode;

	switch (ShaderManager::GetInstance().GetCurrentShaderMode()) {
	case Single:
		// If no adaptive CRT shader is active, use the machine type to
		// derive the appropriate colour settings.
		return GetAutoMachineSettings(video_mode);

	case AutoGraphicsStandard:
		return GetAutoGraphicsStandardSettings(video_mode);

	case AutoMachine: return GetAutoMachineSettings(video_mode);

	case AutoArcade:
	case AutoArcadeSharp: return arcade_settings;

	default: assertm(false, "Invalid ShaderMode value"); return {};
	}
}

AutoImageAdjustments AutoImageAdjustmentsManager::GetAutoMachineSettings(
        const VideoMode& video_mode) const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return composite_settings;
	}

	using enum MachineType;

	switch (machine) {
	case Hercules:
	case CgaMono: return monochrome_settings;

	case CgaColor:
	case Pcjr: return cga_settings;

	case Tandy:
	case Ega: return ega_settings;

	case Vga: return vga_settings;
	default: assertm(false, "Invalid MachineType value"); return {};
	}
}

AutoImageAdjustments AutoImageAdjustmentsManager::GetAutoGraphicsStandardSettings(
        const VideoMode& video_mode) const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return composite_settings;
	}

	using enum GraphicsStandard;

	switch (video_mode.graphics_standard) {
	case Hercules: return monochrome_settings;

	case Cga:
	case Pcjr: return cga_settings;

	case Tga: return ega_settings;

	case Ega:
		// Use VGA settings for VGA games that use EGA modes with an
		// 18-bit VGA palette.
		return video_mode.has_vga_colors ? vga_settings : ega_settings;

	case Vga:
	case Svga:
	case Vesa: return vga_settings;

	default: assertm(false, "Invalid GraphicsStandard value"); return {};
	}
}
