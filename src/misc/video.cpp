// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video.h"

#include "misc/support.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

const char* to_string(const GraphicsStandard g)
{
	switch (g) {
	case GraphicsStandard::Hercules: return "Hercules";
	case GraphicsStandard::Cga: return "CGA";
	case GraphicsStandard::Pcjr: return "PCjr";
	case GraphicsStandard::Tga: return "Tandy";
	case GraphicsStandard::Ega: return "EGA";
	case GraphicsStandard::Vga: return "VGA";
	case GraphicsStandard::Svga: return "SVGA";
	case GraphicsStandard::Vesa: return "VESA";
	default: assertm(false, "Invalid GraphicsStandard"); return "";
	}
}

const char* to_string(const ColorDepth c)
{
	switch (c) {
	case ColorDepth::Monochrome: return "monochrome";
	case ColorDepth::Composite: return "composite";
	case ColorDepth::IndexedColor2: return "2-colour";
	case ColorDepth::IndexedColor4: return "4-colour";
	case ColorDepth::IndexedColor16: return "16-colour";
	case ColorDepth::IndexedColor256: return "256-colour";
	case ColorDepth::HighColor15Bit: return "15-bit high colour";
	case ColorDepth::HighColor16Bit: return "16-bit high colour";
	case ColorDepth::TrueColor24Bit: return "24-bit true colour";
	default: assertm(false, "Invalid ColorDepth"); return "";
	}
}

// Return a human-readable description of the video mode, e.g.:
//   - "CGA 640x200 16-colour text mode 03h"
//   - "EGA 640x350 16-colour graphics mode 10h"
//   - "VGA 720x400 16-colour text mode 03h"
//   - "VGA 320x200 256-colour graphics mode 13h"
//   - "VGA 360x240 256-colour graphics mode"
//   - "VESA 800x600 256-colour graphics mode 103h"
std::string to_string(const VideoMode& video_mode)
{
	const char* mode_type = (video_mode.is_graphics_mode ? "graphics mode"
	                                                     : "text mode");

	const auto mode_number = (video_mode.is_custom_mode
	                                  ? ""
	                                  : format_str(" %02Xh",
	                                               video_mode.bios_mode_number));

	return format_str("%s %dx%d %s %s%s",
	                  to_string(video_mode.graphics_standard),
	                  video_mode.width,
	                  video_mode.height,
	                  to_string(video_mode.color_depth),
	                  mode_type,
	                  mode_number.c_str());
}

const char* to_string(const PixelFormat pf)
{
	switch (pf) {
	case PixelFormat::Indexed8: return "Indexed8";
	case PixelFormat::RGB555_Packed16: return "RGB555_Packed16";
	case PixelFormat::RGB565_Packed16: return "RGB565_Packed16";
	case PixelFormat::BGR24_ByteArray: return "BGR24_ByteArray";
	case PixelFormat::BGRX32_ByteArray: return "BGRX32_ByteArray";
	default: assertm(false, "Invalid pixel format"); return {};
	}
}

uint8_t get_bits_per_pixel(const PixelFormat pf)
{
	return enum_val(pf);
}
