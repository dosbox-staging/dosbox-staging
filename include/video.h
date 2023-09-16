/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_VIDEO_H
#define DOSBOX_VIDEO_H

#include <string>

#include "fraction.h"
#include "setup.h"
#include "types.h"

#define REDUCE_JOYSTICK_POLLING

enum class RenderingBackend {
	Texture,
#if C_OPENGL
	OpenGl
#endif
};

typedef enum {
	GFX_CallBackReset,
	GFX_CallBackStop,
	GFX_CallBackRedraw
} GFX_CallBackFunctions_t;

enum class IntegerScalingMode {
	Off,
	Auto,
	Horizontal,
	Vertical,
};

// Graphics standards ordered by time of introduction (and roughly by
// their capabilities)
enum class GraphicsStandard { Hercules, Cga, Pcjr, Tga, Ega, Vga, Svga, Vesa };

const char* to_string(const GraphicsStandard g);

enum class ColorDepth {
	Monochrome,
	Composite,
	IndexedColor2,
	IndexedColor4,
	IndexedColor16,
	IndexedColor256,
	HighColor15Bit,
	HighColor16Bit,
	TrueColor24Bit
};

const char* to_string(const ColorDepth c);

struct VideoMode {
	// Only reliable for non-custom BIOS modes; for custom modes, it's the
	// mode used as a starting point to set up the tweaked mode, so it can
	// be literally anything.
	uint16_t bios_mode_number = 0;

	// True for graphics modes, false for text modes
	bool is_graphics_mode = false;

	// True for tweaked non-standard modes (e.g., Mode X on VGA).
	bool is_custom_mode = false;

	// Dimensions of the video mode. Note that for VGA adapters this does
	// *not* always match the actual physical output at the signal level but
	// represents the pixel-dimensions of the mode in the video memory.
	// E.g., the 320x200 13h VGA mode takes up 64,000 bytes in the video
	// memory, but is width and height doubled by the VGA hardware to
	// 640x400 at the signal level. Similarly, all 200-line CGA and EGA
	// modes are effectively emulated on VGA adapters and are output width
	// and height doubled.
	uint16_t width  = 0;
	uint16_t height = 0;

	// The intended pixel aspect ratio of the video mode. Note this is not
	// simply calculated by stretching 'width x height' to a 4:3 aspect
	// ratio rectangle; it can be literally anything.
	Fraction pixel_aspect_ratio = {};

	// - For graphics modes, the first graphics standard the mode was
	//   introduced in, unless there is ambiguity, in which case the emulated
	//   graphics adapter (e.g. in the case of PCjr and Tandy modes).
	// - For text modes, the graphics adapter in use.
	GraphicsStandard graphics_standard = {};

	// Colour depth of the video mode. Note this is *not* the same as the
	// storage bit-depth; e.g., some 24-bit true colour modes actually store
	// pixels at 32-bits with the upper 8-bits unused.
	ColorDepth color_depth = {};

	// True if this is a double-scanned mode on VGA (e.g. 200-line CGA and
	// EGA modes and most sub-400-line (S)VGA & VESA modes)
	bool is_double_scanned_mode = false;

	// True for all (S)VGA and VESA modes, and for 200-line EGA modes on VGA
	// that reprogram the default canonical 16-colour CGA palette BIOS to
	// custom 18-bit VGA colours.
	//
	// Useful for differentiating "true EGA" modes used for backwards
	// compatibility on VGA (i.e., to run EGA games) from "repurposed" EGA
	// modes (typical for demos and ports of Amiga action/platformer games;
	// many of these use the planar 320x200 16-colour EGA mode to achieve
	// faster smooth-scrolling, but with custom 18-bit VGA colours).
	bool has_vga_colors = false;

	constexpr bool operator==(const VideoMode& that) const
	{
		return (bios_mode_number == that.bios_mode_number &&
		        is_graphics_mode == that.is_graphics_mode &&
		        is_custom_mode == that.is_custom_mode &&
		        width == that.width && height == that.height &&
		        pixel_aspect_ratio == that.pixel_aspect_ratio &&
		        graphics_standard == that.graphics_standard &&
		        color_depth == that.color_depth &&
		        is_double_scanned_mode == that.is_double_scanned_mode &&
		        has_vga_colors == that.has_vga_colors);
	}

	constexpr bool operator!=(const VideoMode& that) const
	{
		return !operator==(that);
	}
};

std::string to_string(const VideoMode& video_mode);


enum class PixelFormat : uint8_t {
	// Up to 256 colours, paletted;
	// stored as packed uint8 data
	Indexed8 = 8,

	// 32K high colour, 5 bits per red/blue/green component;
	// stored as packed uint16 data with highest bit unused
	//
	// Stored as array of uint16_t in host native endianess.
	// Each uint16_t is layed out as follows:
	// (msb)1X 5R 5G 5B(lsb)
	// Example:
	// uint16_t pixel = (red << 10) | (green << 5) | (blue << 0)
	//
	// SDL Equivalent: SDL_PIXELFORMAT_RGB555
	// FFmpeg Equivalent: AV_PIX_FMT_RGB555
	RGB555_Packed16 = 15,

	// 65K high colour, 5 bits for red/blue, 6 bit for green;
	// stored as packed uint16 data
	//
	// Stored as array of uint16_t in host native endianess.
	// Each uint16_t is layed out as follows:
	// (msb)5R 6G 5B(lsb)
	// Example:
	// uint16_t pixel = (red << 11) | (green << 5) | (blue << 0)
	//
	// SDL Equivalent: SDL_PIXELFORMAT_RGB565
	// FFmpeg Equivalent: AV_PIX_FMT_RGB565
	RGB565_Packed16 = 16,

	// 16.7M (24-bit) true colour, 8 bits per red/blue/green component;
	// stored as packed 24-bit data
	//
	// Stored as array of uint8_t in BGR memory order (endian agnostic)
	// Example:
	// uint8_t *pixels = image.image_data;
	// pixels[0] = blue; pixels[1] = green; pixels[2] = red;
	//
	// SDL Equivalent: SDL_PIXELFORMAT_BGR24
	// FFmpeg Equivalent: AV_PIX_FMT_BGR24
	BGR24_ByteArray = 24,

	// 16.7M (32-bit) true colour; 8 bits per red/blue/green component;
	// stored as packed uint32 data with highest 8 bits unused
	//
	// Stored as array of uint32_t in host native endianess.
	// Each uint32_t is layed out as follows:
	// (msb)8X 8R 8G 8B(lsb)
	// Example:
	// uint32_t pixel = (unused << 24) | (red << 16) | (green << 8) | (blue << 0)
	//
	// SDL Equivalent: SDL_PIXELFORMAT_XRGB8888
	// FFmpeg Equivalent: AV_PIX_FMT_0RGB32
	XRGB8888_Packed32 = 32
};

const char* to_string(const PixelFormat pf);

uint8_t get_bits_per_pixel(const PixelFormat pf);

// Extra information about a bitmap image that represents a single frame of
// DOS video output.
//
// E.g. for the 320x200 256-colour 13h VGA mode with double-scanning
// and pixel-doubling enabled:
//
//   - width = 320 (will be pixel-doubled post-render via double_width)
//   - height = 400 (2*200 lines because we're rendering scan-doubled)
//   - pixel_aspect_ratio = 5/6 (1:1.2) (because the PAR is meant for the
//     final image, post the optional width & height doubling)
//   - double_width = true (pixel-doubling)
//   - double_height = false (we're rendering scan-doubled)
//
struct ImageInfo {
	// The image data has this many pixels per image row (so this is the
	// image width prior to optional width-doubling).
	uint16_t width = 0;

	// The image data has this many rows (so this is the image height prior
	// to optional height-doubling).
	uint16_t height = 0;

	// If true, the final image should be doubled horizontally via a scaler
	// before outputting it (e.g. to achieve pixel-doubling).
	bool double_width = false;

	// If true, the final image should be doubled vertically via a scaler
	// before outputting it (e.g. to achieve fake double-scanning).
	bool double_height = false;

	// If true, we're dealing with a double-scanned VGA mode that was
	// force-rendered as single-scanned.
	bool force_single_scan = false;

	// Pixel aspect ratio to be applied to the final image, *after*
	// optional width and height doubling, so it appears as intended.
	// (`video_mode.pixel_aspect_ratio` holds the "nominal" pixel
	// aspect ratio of the source video mode, which can be different).
	Fraction pixel_aspect_ratio = {};

	// Pixel format of the image data.
	PixelFormat pixel_format = {};

	// Details about the source video mode.
	//
	// This is usually different than the details of image data. E.g., for
	// the 320x200 256-colour 13h VGA mode it always contains the following,
	// regardless of whether double-scanning and pixel-doubling is enabled
	// at the rendering level:
	//   - width = 320
	//   - height = 200
	//   - pixel_aspect_ratio = 5/6 (1:1.2)
	VideoMode video_mode = {};

	constexpr bool operator==(const ImageInfo& that) const
	{
		return (width == that.width && height == that.height &&
		        double_width == that.double_width &&
		        double_height == that.double_height &&
		        force_single_scan == that.force_single_scan &&
		        pixel_aspect_ratio == that.pixel_aspect_ratio &&
		        pixel_format == that.pixel_format &&
		        video_mode == that.video_mode);
	}

	constexpr bool operator!=(const ImageInfo& that) const
	{
		return !operator==(that);
	}
};

enum class InterpolationMode { Bilinear, NearestNeighbour };

typedef void (*GFX_CallBack_t)(GFX_CallBackFunctions_t function);

constexpr uint8_t GFX_CAN_8      = 1 << 0;
constexpr uint8_t GFX_CAN_15     = 1 << 1;
constexpr uint8_t GFX_CAN_16     = 1 << 2;
constexpr uint8_t GFX_CAN_32     = 1 << 3;
constexpr uint8_t GFX_DBL_H      = 1 << 4; // double-width  flag
constexpr uint8_t GFX_DBL_W      = 1 << 5; // double-height flag
constexpr uint8_t GFX_CAN_RANDOM = 1 << 6; // interface can also do random acces

// return code of:
// - true means event loop can keep running.
// - false means event loop wants to quit.
bool GFX_Events();

// Let the presentation layer safely call no-op functions.
// Useful during output initialization or transitions.
void GFX_DisengageRendering();

uint8_t GFX_GetBestMode(const uint8_t flags);
uint32_t GFX_GetRGB(const uint8_t red, const uint8_t green, const uint8_t blue);

struct ShaderInfo;

void GFX_SetShader(const ShaderInfo& shader_info, const std::string& shader_source);

void GFX_SetIntegerScalingMode(const IntegerScalingMode mode);
IntegerScalingMode GFX_GetIntegerScalingMode();

InterpolationMode GFX_GetInterpolationMode();

struct VideoMode;
class Fraction;

uint8_t GFX_SetSize(const int width, const int height,
                    const Fraction& render_pixel_aspect_ratio, const uint8_t flags,
                    const VideoMode& video_mode, GFX_CallBack_t callback);

void GFX_ResetScreen(void);
void GFX_RequestExit(const bool requested);
void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);
bool GFX_StartUpdate(uint8_t * &pixels, int &pitch);
void GFX_EndUpdate( const uint16_t *changedLines );
void GFX_LosingFocus();
void GFX_RegenerateWindow(Section *sec);

enum class MouseHint {
    None,                    // no hint to display
    NoMouse,                 // no mouse mode
    CapturedHotkey,          // mouse captured, use hotkey to release
    CapturedHotkeyMiddle,    // mouse captured, use hotkey or middle-click to release
    ReleasedHotkey,          // mouse released, use hotkey to capture
    ReleasedHotkeyMiddle,    // mouse released, use hotkey or middle-click to capture
    ReleasedHotkeyAnyButton, // mouse released, use hotkey or any click to capture
    SeamlessHotkey,          // seamless mouse, use hotkey to capture
    SeamlessHotkeyMiddle,    // seamless mouse, use hotkey or middle-click to capture
};

void GFX_CenterMouse();
void GFX_SetMouseHint(const MouseHint requested_hint_id);
void GFX_SetMouseCapture(const bool requested_capture);
void GFX_SetMouseVisibility(const bool requested_visible);
void GFX_SetMouseRawInput(const bool requested_raw_input);

// Detects the presence of a desktop environment or window manager
bool GFX_HaveDesktopEnvironment();

#if defined(REDUCE_JOYSTICK_POLLING)
void MAPPER_UpdateJoysticks(void);
#endif

struct SDL_Rect;

SDL_Rect GFX_CalcViewport(const int canvas_width, const int canvas_height,
                          const int draw_width, const int draw_height,
                          const Fraction& render_pixel_aspect_ratio);

SDL_Rect GFX_GetCanvasSize();

RenderingBackend GFX_GetRenderingBackend();

#endif
