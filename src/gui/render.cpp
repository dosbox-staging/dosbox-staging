/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
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

#include "dosbox.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <mutex>

#include "../capture/capture.h"
#include "control.h"
#include "fraction.h"
#include "mapper.h"
#include "math_utils.h"
#include "render.h"
#include "setup.h"
#include "shader_manager.h"
#include "shell.h"
#include "string_utils.h"
#include "support.h"
#include "vga.h"
#include "video.h"

Render_t render;
ScalerLineHandler_t RENDER_DrawLine;

static ShaderManager& get_shader_manager()
{
	static auto shader_manager = ShaderManager();
	return shader_manager;
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

static void render_callback(GFX_CallBackFunctions_t function);

static void check_palette()
{
	// Clean up any previous changed palette data
	if (render.pal.changed) {
		memset(render.pal.modified, 0, sizeof(render.pal.modified));
		render.pal.changed = false;
	}
	if (render.pal.first > render.pal.last) {
		return;
	}
	Bitu i;
	switch (render.scale.outMode) {
	case scalerMode8: break;
	case scalerMode15:
	case scalerMode16:
		for (i = render.pal.first; i <= render.pal.last; i++) {
			uint8_t r = render.pal.rgb[i].red;
			uint8_t g = render.pal.rgb[i].green;
			uint8_t b = render.pal.rgb[i].blue;

			uint16_t new_pal = GFX_GetRGB(r, g, b);
			if (new_pal != render.pal.lut.b16[i]) {
				render.pal.changed     = true;
				render.pal.modified[i] = 1;
				render.pal.lut.b16[i]  = new_pal;
			}
		}
		break;
	case scalerMode32:
	default:
		for (i = render.pal.first; i <= render.pal.last; i++) {
			uint8_t r = render.pal.rgb[i].red;
			uint8_t g = render.pal.rgb[i].green;
			uint8_t b = render.pal.rgb[i].blue;

			uint32_t new_pal = GFX_GetRGB(r, g, b);
			if (new_pal != render.pal.lut.b32[i]) {
				render.pal.changed     = true;
				render.pal.modified[i] = 1;
				render.pal.lut.b32[i]  = new_pal;
			}
		}
		break;
	}

	// Setup pal index to startup values
	render.pal.first = 256;
	render.pal.last  = 0;
}

void RENDER_SetPalette(const uint8_t entry, const uint8_t red,
                       const uint8_t green, const uint8_t blue)
{
	render.pal.rgb[entry].red   = red;
	render.pal.rgb[entry].green = green;
	render.pal.rgb[entry].blue  = blue;

	if (render.pal.first > entry) {
		render.pal.first = entry;
	}
	if (render.pal.last < entry) {
		render.pal.last = entry;
	}
}

static void empty_line_handler(const void*) {}

static void start_line_handler(const void* s)
{
	if (s) {
		auto src = static_cast<const uintptr_t*>(s);
		auto cache = reinterpret_cast<uintptr_t*>(render.scale.cacheRead);
		for (Bits x = render.src_start; x > 0;) {
			const auto src_ptr = reinterpret_cast<const uint8_t*>(src);
			const auto src_val = read_unaligned_size_t(src_ptr);
			if (src_val != cache[0]) {
				if (!GFX_StartUpdate(render.scale.outWrite,
				                     render.scale.outPitch)) {
					RENDER_DrawLine = empty_line_handler;
					return;
				}
				render.scale.outWrite += render.scale.outPitch *
				                         Scaler_ChangedLines[0];
				RENDER_DrawLine = render.scale.lineHandler;
				RENDER_DrawLine(s);
				return;
			}
			x--;
			src++;
			cache++;
		}
	}
	render.scale.cacheRead += render.scale.cachePitch;
	Scaler_ChangedLines[0] += Scaler_Aspect[render.scale.inLine];
	render.scale.inLine++;
	render.scale.outLine++;
}

static void finish_line_handler(const void* s)
{
	if (s) {
		auto src = static_cast<const uintptr_t*>(s);
		auto cache = reinterpret_cast<uintptr_t*>(render.scale.cacheRead);
		for (Bits x = render.src_start; x > 0;) {
			cache[0] = src[0];
			x--;
			src++;
			cache++;
		}
	}
	render.scale.cacheRead += render.scale.cachePitch;
}

static void clear_cache_handler(const void* src)
{
	const uint32_t* srcLine = (const uint32_t*)src;
	uint32_t* cacheLine     = (uint32_t*)render.scale.cacheRead;
	Bitu width              = render.scale.cachePitch / 4;
	for (Bitu x = 0; x < width; x++) {
		cacheLine[x] = ~srcLine[x];
	}
	render.scale.lineHandler(src);
}

bool RENDER_StartUpdate()
{
	if (render.updating) {
		return false;
	}
	if (!render.active) {
		return false;
	}
	if (render.scale.inMode == scalerMode8) {
		check_palette();
	}
	render.scale.inLine     = 0;
	render.scale.outLine    = 0;
	render.scale.cacheRead  = (uint8_t*)&scalerSourceCache;
	render.scale.outWrite   = nullptr;
	render.scale.outPitch   = 0;
	Scaler_ChangedLines[0]  = 0;
	Scaler_ChangedLineIndex = 0;

	// Clearing the cache will first process the line to make sure it's
	// never the same
	if (render.scale.clearCache) {
		// LOG_MSG("Clearing cache");

		// Will always have to update the screen with this one anyway,
		// so let's update already
		if (!GFX_StartUpdate(render.scale.outWrite, render.scale.outPitch)) {
			return false;
		}
		render.fullFrame        = true;
		render.scale.clearCache = false;
		RENDER_DrawLine         = clear_cache_handler;
	} else {
		if (render.pal.changed) {
			// Assume pal changes always do a full screen update
			// anyway
			if (!GFX_StartUpdate(render.scale.outWrite,
			                     render.scale.outPitch)) {
				return false;
			}
			RENDER_DrawLine  = render.scale.linePalHandler;
			render.fullFrame = true;
		} else {
			RENDER_DrawLine = start_line_handler;
			if (CAPTURE_IsCapturingImage() ||
			    CAPTURE_IsCapturingVideo()) {
				render.fullFrame = true;
			} else {
				render.fullFrame = false;
			}
		}
	}
	render.updating = true;
	return true;
}

static void halt_render()
{
	RENDER_DrawLine = empty_line_handler;
	GFX_EndUpdate(nullptr);
	render.updating = false;
	render.active   = false;
}

void RENDER_EndUpdate(bool abort)
{
	if (!render.updating) {
		return;
	}

	RENDER_DrawLine = empty_line_handler;

	if (CAPTURE_IsCapturingImage() || CAPTURE_IsCapturingVideo()) {
		bool double_width  = false;
		bool double_height = false;
		if (render.src.double_width != render.src.double_height) {
			if (render.src.double_width) {
				double_width = true;
			}
			if (render.src.double_height) {
				double_height = true;
			}
		}

		RenderedImage image = {};

		image.params               = render.src;
		image.params.double_width  = double_width;
		image.params.double_height = double_height;
		image.pitch                = render.scale.cachePitch;
		image.image_data           = (uint8_t*)&scalerSourceCache;
		image.palette_data         = (uint8_t*)&render.pal.rgb;

		const auto frames_per_second = static_cast<float>(render.fps);

		CAPTURE_AddFrame(image, frames_per_second);
	}

	if (render.scale.outWrite) {
		GFX_EndUpdate(abort ? nullptr : Scaler_ChangedLines);
	} else {
		// If we made it here, then there's nothing new to render.
		GFX_EndUpdate(nullptr);
	}
	render.updating = false;
}

static Bitu make_aspect_table(Bitu height, double scaley, Bitu miny)
{
	Bitu i;
	double lines    = 0;
	Bitu linesadded = 0;

	for (i = 0; i < height; i++) {
		lines += scaley;
		if (lines >= miny) {
			Bitu templines = (Bitu)lines;
			lines -= templines;
			linesadded += templines;
			Scaler_Aspect[i] = templines;
		} else {
			Scaler_Aspect[i] = 0;
		}
	}
	return linesadded;
}

static Section_prop* get_render_section()
{
	assert(control);

	auto render_section = static_cast<Section_prop*>(
	        control->GetSection("render"));
	assert(render_section);

	return render_section;
}

void RENDER_Reinit()
{
	RENDER_Init(get_render_section());
}

static void render_reset()
{
	static std::mutex render_reset_mutex;

	if (render.src.width == 0 || render.src.height == 0) {
		return;
	}

	// Despite rendering being a single-threaded sequence, the Reset() can
	// be called from the rendering callback, which might come from a video
	// driver operating in a different thread or process.
	std::lock_guard<std::mutex> guard(render_reset_mutex);

	uint16_t render_width_px = render.src.width;
	bool double_width        = render.src.double_width;
	bool double_height       = render.src.double_height;

	uint8_t gfx_flags, xscale, yscale;
	ScalerSimpleBlock_t* simpleBlock = &ScaleNormal1x;

	// Don't do software scaler sizes larger than 4k
	uint16_t maxsize_current_input = SCALER_MAXWIDTH / render_width_px;
	if (render.scale.size > maxsize_current_input) {
		render.scale.size = maxsize_current_input;
	}

	if (double_height && double_width) {
		simpleBlock = &ScaleNormal2x;
	} else if (double_width) {
		simpleBlock = &ScaleNormalDw;
	} else if (double_height) {
		simpleBlock = &ScaleNormalDh;
	} else {
		simpleBlock = &ScaleNormal1x;
	}

	if ((render_width_px * simpleBlock->xscale > SCALER_MAXWIDTH) ||
	    (render.src.height * simpleBlock->yscale > SCALER_MAXHEIGHT)) {
		simpleBlock = &ScaleNormal1x;
	}

	gfx_flags = simpleBlock->gfxFlags;
	xscale    = simpleBlock->xscale;
	yscale    = simpleBlock->yscale;
	//		LOG_MSG("Scaler:%s",simpleBlock->name);

	constexpr auto src_pixel_bytes = sizeof(uintptr_t);

	switch (render.src.pixel_format) {
	case PixelFormat::Indexed8:
	case PixelFormat::RGB555_Packed16:
	case PixelFormat::RGB565_Packed16:
		render.src_start = (render.src.width * 2) / src_pixel_bytes;
		gfx_flags        = (gfx_flags & ~GFX_CAN_8);
		break;
	case PixelFormat::BGR24_ByteArray:
		render.src_start = (render.src.width * 3) / src_pixel_bytes;
		gfx_flags        = (gfx_flags & ~GFX_CAN_8);
		break;
	case PixelFormat::BGRX32_ByteArray:
		render.src_start = (render.src.width * 4) / src_pixel_bytes;
		gfx_flags        = (gfx_flags & ~GFX_CAN_8);
		break;
	}

	gfx_flags = GFX_GetBestMode(gfx_flags);

	if (!gfx_flags) {
		if (simpleBlock == &ScaleNormal1x) {
			E_Exit("Failed to create a rendering output");
		}
	}
	render_width_px *= xscale;
	const auto render_height_px = make_aspect_table(render.src.height,
	                                                yscale,
	                                                yscale);

	// Set up scaler variables
	if (double_height) {
		gfx_flags |= GFX_DBL_H;
	}
	if (double_width) {
		gfx_flags |= GFX_DBL_W;
	}

	if (GFX_GetRenderingBackend() == RenderingBackend::OpenGl) {
		GFX_SetShader(get_shader_manager().GetCurrentShaderInfo(),
		              get_shader_manager().GetCurrentShaderSource());
	}

	const auto render_pixel_aspect_ratio = render.src.pixel_aspect_ratio;

	gfx_flags = GFX_SetSize(render_width_px,
	                        render_height_px,
	                        render_pixel_aspect_ratio,
	                        gfx_flags,
	                        render.src.video_mode,
	                        &render_callback);

	if (gfx_flags & GFX_CAN_8) {
		render.scale.outMode = scalerMode8;
	} else if (gfx_flags & GFX_CAN_15) {
		render.scale.outMode = scalerMode15;
	} else if (gfx_flags & GFX_CAN_16) {
		render.scale.outMode = scalerMode16;
	} else if (gfx_flags & GFX_CAN_32) {
		render.scale.outMode = scalerMode32;
	} else {
		E_Exit("Failed to create a rendering output");
	}

	const auto lineBlock = gfx_flags & GFX_CAN_RANDOM ? &simpleBlock->Random
	                                                  : &simpleBlock->Linear;
	switch (render.src.pixel_format) {
	case PixelFormat::Indexed8:
		render.scale.lineHandler = (*lineBlock)[0][render.scale.outMode];
		render.scale.linePalHandler = (*lineBlock)[5][render.scale.outMode];
		render.scale.inMode     = scalerMode8;
		render.scale.cachePitch = render.src.width * 1;
		break;
	case PixelFormat::RGB555_Packed16:
		render.scale.lineHandler = (*lineBlock)[1][render.scale.outMode];
		render.scale.linePalHandler = nullptr;
		render.scale.inMode         = scalerMode15;
		render.scale.cachePitch     = render.src.width * 2;
		break;
	case PixelFormat::RGB565_Packed16:
		render.scale.lineHandler = (*lineBlock)[2][render.scale.outMode];
		render.scale.linePalHandler = nullptr;
		render.scale.inMode         = scalerMode16;
		render.scale.cachePitch     = render.src.width * 2;
		break;
	case PixelFormat::BGR24_ByteArray:
		render.scale.lineHandler = (*lineBlock)[3][render.scale.outMode];
		render.scale.linePalHandler = nullptr;
		render.scale.inMode         = scalerMode32;
		render.scale.cachePitch     = render.src.width * 3;
		break;
	case PixelFormat::BGRX32_ByteArray:
		render.scale.lineHandler = (*lineBlock)[4][render.scale.outMode];
		render.scale.linePalHandler = nullptr;
		render.scale.inMode         = scalerMode32;
		render.scale.cachePitch     = render.src.width * 4;
		break;
	default:
		E_Exit("RENDER: Invalid pixel_format %u",
		       static_cast<uint8_t>(render.src.pixel_format));
	}

	render.scale.blocks    = render.src.width / SCALER_BLOCKSIZE;
	render.scale.lastBlock = render.src.width % SCALER_BLOCKSIZE;
	render.scale.inHeight  = render.src.height;

	// Reset the palette change detection to it's initial value
	render.pal.first   = 0;
	render.pal.last    = 255;
	render.pal.changed = false;
	memset(render.pal.modified, 0, sizeof(render.pal.modified));

	// Finish this frame using a copy only handler
	RENDER_DrawLine       = finish_line_handler;
	render.scale.outWrite = nullptr;

	// Signal the next frame to first reinit the cache
	render.scale.clearCache = true;
	render.active           = true;
}

static void render_callback(GFX_CallBackFunctions_t function)
{
	if (function == GFX_CallBackStop) {
		halt_render();
		return;
	} else if (function == GFX_CallBackRedraw) {
		render.scale.clearCache = true;
		return;
	} else if (function == GFX_CallBackReset) {
		GFX_EndUpdate(nullptr);
		render_reset();
	} else {
		E_Exit("Unhandled GFX_CallBackReset %d", function);
	}
}

void RENDER_SetSize(const ImageInfo& image_info, const double frames_per_second)
{
	halt_render();

	if (image_info.width == 0 || image_info.height == 0 ||
	    image_info.width > SCALER_MAXWIDTH ||
	    image_info.height > SCALER_MAXHEIGHT) {
		return;
	}

	render.src = image_info;
	render.fps = frames_per_second;

	render_reset();
}

static bool force_vga_single_scan   = false;
static bool force_no_pixel_doubling = false;

// Double-scan VGA modes and pixel-double all video modes by default unless:
//
//  1) Single scanning or no pixel doubling is requested by the OpenGL shader.
//  2) The interpolation mode is nearest-neighbour in texture output mode.
//
// The default `interpolation/sharp.glsl` and `interpolation/nearest.glsl`
// shaders requests both single scanning and no pixel doubling because it scales
// pixels as flat adjacent rectangles. This not only produces identical output
// versus double scanning and pixel doubling, but also provides finer integer
// scaling steps (especially important on sub-4K screens), plus improves
// performance on low-end systems like the Raspberry Pi.
//
// The same reasoning applies to nearest-neighbour interpolation in texture
// output mode.
//
static void setup_scan_and_pixel_doubling()
{
	switch (GFX_GetRenderingBackend()) {
	case RenderingBackend::Texture: {
		const auto nearest_neighbour_on = (GFX_GetTextureInterpolationMode() ==
		                                   InterpolationMode::NearestNeighbour);

		force_vga_single_scan   = nearest_neighbour_on;
		force_no_pixel_doubling = nearest_neighbour_on;
	} break;

	case RenderingBackend::OpenGl: {
		const auto shader_info = get_shader_manager().GetCurrentShaderInfo();
		force_vga_single_scan = shader_info.settings.force_single_scan;
		force_no_pixel_doubling = shader_info.settings.force_no_pixel_doubling;
	} break;

	default: assertm(false, "Invalid RenderindBackend value");
	}

	VGA_AllowVgaScanDoubling(!force_vga_single_scan);
	VGA_AllowPixelDoubling(!force_no_pixel_doubling);
}

bool RENDER_MaybeAutoSwitchShader([[maybe_unused]] const DosBox::Rect canvas_size_px,
                                  [[maybe_unused]] const VideoMode& video_mode,
                                  [[maybe_unused]] const bool reinit_render)
{
	// We always expect a valid canvas and DOS video mode
	assert(!canvas_size_px.IsEmpty());
	assert(video_mode.width > 0 && video_mode.height > 0);

	if (GFX_GetRenderingBackend() != RenderingBackend::OpenGl) {
		return false;
	}

	get_shader_manager().NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_shader_name = get_shader_manager().GetCurrentShaderInfo().name;

	const auto changed_shader = (new_shader_name != render.current_shader_name);

	if (changed_shader) {
		if (reinit_render) {
			RENDER_Reinit();

			// We can't set the new shader name here yet because
			// then the "shader changed" reinit path wouldn't be
			// trigger in RENDER_Init()
		} else {
			setup_scan_and_pixel_doubling();

			// We must set the new shader name here as we're
			// bypassing a full render reinit (RENDER_Init() is the
			// only other place where 'render.current_shader_name'
			// can be set).
			render.current_shader_name = new_shader_name;
		}
	}
	return changed_shader;
}

void RENDER_NotifyEgaModeWithVgaPalette()
{
	// If we're getting these notifications on non-VGA cards, that's a
	// programming error.
	assert(machine == MCH_VGA);

	auto video_mode = VGA_GetCurrentVideoMode();

	if (!video_mode.has_vga_colors) {
		video_mode.has_vga_colors = true;

		// We are potentially auto-switching to a VGA shader now.
		constexpr auto reinit_render = true;

		RENDER_MaybeAutoSwitchShader(GFX_GetCanvasSizeInPixels(),
		                             video_mode,
		                             reinit_render);
	}
}

std::deque<std::string> RENDER_GenerateShaderInventoryMessage()
{
	return get_shader_manager().GenerateShaderInventoryMessage();
}

void RENDER_AddMessages()
{
	ShaderManager::AddMessages();
}

static void reload_shader([[maybe_unused]] const bool pressed)
{
	if (GFX_GetRenderingBackend() != RenderingBackend::OpenGl) {
		return;
	}

	if (!pressed) {
		return;
	}

	render.force_reload_shader = true;
	RENDER_Reinit();

	// The shader settings might have been changed (e.g. force_single_scan,
	// force_no_pixel_doubling), so force re-rendering the image using the
	// new settings. Without this, the altered settings would only take
	// effect on the next video mode change.
	VGA_SetupDrawing(0);
}

constexpr auto MonochromePaletteAmber      = "amber";
constexpr auto MonochromePaletteGreen      = "green";
constexpr auto MonochromePaletteWhite      = "white";
constexpr auto MonochromePalettePaperwhite = "paperwhite";

static MonochromePalette to_monochrome_palette_enum(const char* setting)
{
	if (strcasecmp(setting, MonochromePaletteAmber) == 0) {
		return MonochromePalette::Amber;
	}
	if (strcasecmp(setting, MonochromePaletteGreen) == 0) {
		return MonochromePalette::Green;
	}
	if (strcasecmp(setting, MonochromePaletteWhite) == 0) {
		return MonochromePalette::White;
	}
	if (strcasecmp(setting, MonochromePalettePaperwhite) == 0) {
		return MonochromePalette::Paperwhite;
	}
	assertm(false, "Invalid monochrome_palette setting");
	return {};
}

static const char* to_string(const enum MonochromePalette palette)
{
	switch (palette) {
	case MonochromePalette::Amber: return MonochromePaletteAmber;
	case MonochromePalette::Green: return MonochromePaletteGreen;
	case MonochromePalette::White: return MonochromePaletteWhite;
	case MonochromePalette::Paperwhite: return MonochromePalettePaperwhite;
	default: assertm(false, "Invalid MonochromePalette value"); return {};
	}
}

static AspectRatioCorrectionMode aspect_ratio_correction_mode = {};

static AspectRatioCorrectionMode get_aspect_ratio_correction_mode_setting()
{
	const std::string mode = get_render_section()->Get_string("aspect");

	if (has_true(mode) || mode == "auto") {
		return AspectRatioCorrectionMode::Auto;

	} else if (has_false(mode) || mode == "square-pixels") {
		return AspectRatioCorrectionMode::SquarePixels;

	} else if (mode == "stretch") {
		return AspectRatioCorrectionMode::Stretch;

	} else {
		LOG_WARNING("RENDER: Invalid 'aspect' setting: '%s', using 'auto'",
		            mode.c_str());
		return AspectRatioCorrectionMode::Auto;
	}
}

AspectRatioCorrectionMode RENDER_GetAspectRatioCorrectionMode()
{
	return aspect_ratio_correction_mode;
}

static IntegerScalingMode get_integer_scaling_mode_setting()
{
	const std::string mode = get_render_section()->Get_string("integer_scaling");

	if (has_false(mode)) {
		return IntegerScalingMode::Off;

	} else if (mode == "auto") {
		return IntegerScalingMode::Auto;

	} else if (mode == "horizontal") {
		return IntegerScalingMode::Horizontal;

	} else if (mode == "vertical") {
		return IntegerScalingMode::Vertical;

	} else {
		LOG_WARNING("RENDER: Invalid 'integer_scaling' setting: '%s', using 'auto'",
		            mode.c_str());
		return IntegerScalingMode::Auto;
	}
}

static void set_default_viewport_setting()
{
	const auto string_prop = get_render_section()->GetStringProp("viewport");
	string_prop->SetValue("fit");
}

static void log_invalid_viewport_setting_warning(
        const std::string& pref,
        const std::optional<const std::string> extra_info = {})
{
	LOG_WARNING("DISPLAY: Invalid 'viewport' setting: '%s'"
	            "%s%s, using 'fit'",
	            pref.c_str(),
	            (extra_info ? ". " : ""),
	            (extra_info ? extra_info->c_str() : ""));
}

std::optional<std::pair<int, int>> parse_int_dimensions(const std::string_view s)
{
	const auto parts = split(s, "x");
	if (parts.size() == 2) {
		const auto w = parse_int(parts[0]);
		const auto h = parse_int(parts[1]);
		if (w && h) {
			return {{*w, *h}};
		}
	}
	return {};
}

static std::optional<ViewportSettings> parse_fit_viewport_modes(const std::string& pref)
{
	if (pref == "fit") {
		ViewportSettings viewport = {};
		viewport.mode             = ViewportMode::Fit;
		return viewport;

	} else if (const auto width_and_height = parse_int_dimensions(pref)) {
		const auto [w, h] = *width_and_height;

		const auto desktop = GFX_GetDesktopSize();

		const bool is_out_of_bounds = (w <= 0 || w > desktop.w ||
		                               h <= 0 || h > desktop.h);
		if (is_out_of_bounds) {
			const auto extra_info = format_str(
			        "Viewport size is outside of the %dx%d desktop bounds",
			        iroundf(desktop.w),
			        iroundf(desktop.h));

			log_invalid_viewport_setting_warning(pref, extra_info);
			return {};
		}

		ViewportSettings viewport = {};
		viewport.mode             = ViewportMode::Fit;

		const DosBox::Rect limit = {w, h};
		viewport.fit.limit_size  = limit;

		const auto limit_px = limit.Copy().ScaleSize(GFX_GetDpiScaleFactor());

		LOG_MSG("DISPLAY: Limiting viewport size to %dx%d logical units "
		        "(%dx%d pixels)",
		        iroundf(limit.w),
		        iroundf(limit.h),
		        iroundf(limit_px.w),
		        iroundf(limit_px.h));

		return viewport;

	} else if (const auto percentage = parse_percentage_with_optional_percent_sign(
	                   pref)) {
		const auto p = *percentage;

		const auto desktop = GFX_GetDesktopSize();

		const bool is_out_of_bounds = (p < 1.0f || p > 100.0f);
		if (is_out_of_bounds) {
			const auto extra_info = "Desktop percentage is outside of the 1-100%% range";

			log_invalid_viewport_setting_warning(pref, extra_info);
			return {};
		}

		ViewportSettings viewport  = {};
		viewport.mode              = ViewportMode::Fit;
		viewport.fit.desktop_scale = p / 100.0f;

		const auto limit = desktop.Copy().ScaleSize(*viewport.fit.desktop_scale);
		const auto limit_px = limit.Copy().ScaleSize(GFX_GetDpiScaleFactor());

		LOG_MSG("DISPLAY: Limiting viewport size to %2.4g%% of the "
		        "desktop (%dx%d logical units, %dx%d pixels)",
		        p,
		        iroundf(limit.w),
		        iroundf(limit.h),
		        iroundf(limit_px.w),
		        iroundf(limit_px.h));

		return viewport;

	} else {
		log_invalid_viewport_setting_warning(pref);
		return {};
	}
}

static constexpr auto MinRelativeScaleFactor = 0.2f; // 20%
static constexpr auto MaxRelativeScaleFactor = 3.0f; // 300%

static std::optional<ViewportSettings> parse_relative_viewport_modes(const std::string& pref)
{
	const auto parts = split(pref);

	if (parts.size() == 3 && parts[0] == "relative") {
		const auto maybe_width_percentage =
		        parse_percentage_with_optional_percent_sign(parts[1]);

		const auto maybe_height_percentage =
		        parse_percentage_with_optional_percent_sign(parts[2]);

		if (!maybe_width_percentage) {
			const auto extra_info = "Invalid horizontal scale";
			log_invalid_viewport_setting_warning(pref, extra_info);
			return {};
		}
		if (!maybe_height_percentage) {
			const auto extra_info = "Invalid vertical scale";
			log_invalid_viewport_setting_warning(pref, extra_info);
			return {};
		}

		const auto width_scale  = *maybe_width_percentage / 100.f;
		const auto height_scale = *maybe_height_percentage / 100.f;

		auto is_within_bounds = [&](const float scale) {
			return (scale >= MinRelativeScaleFactor &&
			        scale <= MaxRelativeScaleFactor);
		};

		if (!is_within_bounds(width_scale)) {
			const auto extra_info = format_str(
			        "Horizontal scale must be within the %g-%g%% range",
			        MinRelativeScaleFactor * 100.0f,
			        MaxRelativeScaleFactor * 100.0f);

			log_invalid_viewport_setting_warning(pref, extra_info);
			return {};
		}
		if (!is_within_bounds(height_scale)) {
			LOG_TRACE("****1");
			const auto extra_info = format_str(
			        "Vertical scale must be within the %g-%g%% range",
			        MinRelativeScaleFactor * 100.0f,
			        MaxRelativeScaleFactor * 100.0f);

			log_invalid_viewport_setting_warning(pref, extra_info);
			return {};
		}

		ViewportSettings viewport      = {};
		viewport.mode                  = ViewportMode::Relative;
		viewport.relative.width_scale  = width_scale;
		viewport.relative.height_scale = height_scale;

		LOG_MSG("DISPLAY: Scaling viewport by %2.4g%% horizontally "
		        "and %2.4g%% vertically ",
		        width_scale * 100.f,
		        height_scale * 100.f);

		return viewport;

	} else {
		log_invalid_viewport_setting_warning(pref);
		return {};
	}
}

static std::optional<ViewportSettings> parse_viewport_settings(const std::string& pref)
{
	if (pref.starts_with("relative")) {
		return parse_relative_viewport_modes(pref);
	} else {
		return parse_fit_viewport_modes(pref);
	}
}

static ViewportSettings viewport_settings = {};

static ViewportSettings get_default_viewport_settings()
{
	ViewportSettings viewport = {};

	viewport      = {};
	viewport.mode = ViewportMode::Fit;

	return viewport;
}

DosBox::Rect RENDER_CalcRestrictedViewportSizeInPixels(const DosBox::Rect& canvas_size_px)
{
	const auto dpi_scale = GFX_GetDpiScaleFactor();

	switch (viewport_settings.mode) {
	case ViewportMode::Fit: {
		auto viewport_size_px = [&] {
			if (viewport_settings.fit.limit_size) {
				return viewport_settings.fit.limit_size->Copy().ScaleSize(
				        dpi_scale);

			} else if (viewport_settings.fit.desktop_scale) {
				auto desktop_size_px = GFX_GetDesktopSize().ScaleSize(
				        dpi_scale);

				return desktop_size_px.ScaleSize(
				        *viewport_settings.fit.desktop_scale);
			} else {
				// The viewport equals the canvas size
				// in Fit mode without parameters
				return canvas_size_px;
			}
		}();

		if (canvas_size_px.Contains(viewport_size_px)) {
			return viewport_size_px;
		} else {
			return viewport_size_px.Intersect(canvas_size_px);
		}
	}

	case ViewportMode::Relative: {
		const auto restricted_canvas_size_px = DosBox::Rect{4, 3}.ScaleSizeToFit(
		        canvas_size_px);

		return restricted_canvas_size_px.Copy()
		        .ScaleWidth(viewport_settings.relative.width_scale)
		        .ScaleHeight(viewport_settings.relative.height_scale);
	}

	default: assertm(false, "Invalid ViewportMode value"); return {};
	}
}

std::string RENDER_GetCgaColorsSetting()
{
	return get_render_section()->Get_string("cga_colors");
}

static void init_render_settings(Section_prop& secprop)
{
	constexpr auto always        = Property::Changeable::Always;
	constexpr auto deprecated    = Property::Changeable::Deprecated;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	auto* int_prop = secprop.Add_int("frameskip", deprecated, 0);
	int_prop->Set_help(
	        "Consider capping frame rates using the 'host_rate' setting.");

	auto* string_prop = secprop.Add_string("glshader", always, "crt-auto");
	string_prop->SetOptionHelp(
	        "Set an adaptive CRT monitor emulation shader or a regular GLSL shader in OpenGL\n"
	        "output modes ('crt-auto' by default). Adaptive CRT shader options:\n"
	        "  crt-auto:               A CRT shader that prioritises developer intent and\n"
	        "                          how people experienced the game at the time of\n"
	        "                          release (default). The appropriate shader variant is\n"
	        "                          automatically selected based the graphics standard of\n"
	        "                          the current video mode and the viewport size,\n"
	        "                          irrespective of the 'machine' setting. This means\n"
	        "                          that even on an emulated VGA card you'll get\n"
	        "                          authentic single-scanned EGA monitor emulation with\n"
	        "                          visible \"thick scanlines\" in EGA games.\n"
	        "  crt-auto-machine:       Similar to 'crt-auto', but this picks a fixed CRT\n"
	        "                          monitor appropriate for the video adapter configured\n"
	        "                          via the 'machine' setting. E.g., CGA and EGA games\n"
	        "                          will appear double-scanned on an emulated VGA\n"
	        "                          adapter.\n"
	        "  crt-auto-arcade:        Emulation of an arcade or home computer monitor less\n"
	        "                          sharp than a typical PC monitor with thick scanlines\n"
	        "                          in low-resolution modes. This fantasy option does not\n"
	        "                          exist in real life, but it can be a lot of fun,\n"
	        "                          especially with DOS ports of Amiga games.\n"
	        "  crt-auto-arcade-sharp:  A sharper variant of the arcade shader for those who\n"
	        "                          like the thick scanlines but want to retain the\n"
	        "                          horizontal sharpness of a typical PC monitor.\n"
	        "\n"
	        "Other shader options include (non-exhaustive list):\n"
	        "  sharp:                  Upscale the image treating the pixels as rectangles.\n"
	        "                          Results in a sharp image with minimum blur.\n"
	        "  bilinear:               Upscale the image using bilinear interpolation\n"
	        "                          (results in a blurry image).\n"
	        "  nearest:                Upscale the image using nearest-neighbour\n"
	        "                          interpolation (also known as \"no bilinear\"). Results\n"
	        "                          in the sharpest possible image at the expense of\n"
	        "                          uneven pixels (this is less of an issue on high\n"
	        "                          resolution screens).\n"
	        "\n"
	        "Start DOSBox Staging with the '--list-glshaders' command line option to see the\n"
	        "full list of available shaders. You can also use an absolute or relative path to\n"
	        "a file. In all cases, you may omit the shader's '.glsl' file extension.");
	string_prop->SetEnabledOptions({
#if C_OPENGL
		"glshader",
#endif
	});

	string_prop = secprop.Add_string("aspect", always, "auto");
	string_prop->Set_help(
	        "Set the aspect ratio correction mode ('auto' by default):\n"
	        "  auto, on:            Apply aspect ratio correction for modern square-pixel\n"
	        "                       flat-screen displays, so DOS video modes with non-square\n"
	        "                       pixels appear as they would on a 4:3 display aspect\n"
	        "                       ratio CRT monitor the majority of DOS games were\n"
	        "                       designed for. This setting only affects video modes that\n"
	        "                       use non-square pixels, such as 320x200 or 640x400;.\n"
	        "                       square-pixelmodes (e.g., 320x240, 640x480, and 800x600),\n"
	        "                       are displayed as-is.\n"
	        "  square-pixels, off:  Don't apply aspect ratio correction; all DOS video modes\n"
	        "                       are displayed with square pixels. Most 320x200 games\n"
	        "                       will appear squashed, but a minority of titles (e.g.,\n"
	        "                       DOS ports of PAL Amiga games) need square pixels to\n"
	        "                       appear as the artists intended.\n"
	        "  stretch:             Calculate the aspect ratio from the viewport's\n"
	        "                       dimensions. Combined with 'viewport', this mode is\n"
	        "                       useful to force arbitrary aspect ratios (e.g.,\n"
	        "                       stretching DOS games to fullscreen on 16:9 displays) and\n"
	        "                       to emulate the horizontal and vertical stretch controls\n"
	        "                       of CRT monitors.");

	string_prop->Set_values({"auto", "on", "square-pixels", "off", "stretch"});

	string_prop = secprop.Add_string("integer_scaling", always, "auto");
	string_prop->Set_help(
	        "Constrain the horizontal or vertical scaling factor to the largest integer\n"
	        "value so the image still fits into the viewport ('auto' by default). The\n"
	        "configured aspect ratio is always maintained according to the 'aspect' and\n"
	        "'viewport' settings, which may result in a non-integer scaling factor in the\n"
	        "other dimension. If the image is larger than the viewport, the integer scaling\n"
	        "constraint is auto-disabled (same as 'off'). Possible values:\n"
	        "  auto:        'vertical' mode auto-enabled for adaptive CRT shaders only\n"
	        "               (see 'glshader'), otherwise 'off' (default).\n"
	        "  vertical:    Constrain the vertical scaling factor to integer values.\n"
	        "               This is the recommended setting for CRT shaders to avoid uneven\n"
	        "               scanlines and interference artifacts.\n"
	        "  horizontal:  Constrain the horizontal scaling factor to integer values.\n"
	        "  off:         No integer scaling constraint is applied; the image fills the\n"
	        "               viewport while maintaining the configured aspect ratio.");

	string_prop->Set_values({"auto", "vertical", "horizontal", "off"});

	string_prop = secprop.Add_string("viewport", always, "fit");
	string_prop->Set_help(
	        "Set the viewport size ('fit' by default). This is the maximum drawable area;\n"
	        "the video output is always contained within the viewport while taking the\n"
	        "configured aspect ratio into account (see 'aspect'). Possible values:\n"
	        "  fit:             Fit the viewport into the available window/screen (default).\n"
	        "                   There might be padding (black areas) around the image with\n"
	        "                   'integer_scaling' enabled.\n"
	        "  WxH:             Set a fixed viewport size in WxH format in logical units\n"
	        "                   (e.g., 960x720). The specified size must not be larger than\n"
	        "                   the desktop. If it's larger than the window size, it's\n"
	        "                   scaled to fit within the window.\n"
	        "  N%%:              Similar to 'WxH' but the size is specified as a percentage\n"
	        "                   of the desktop size.\n"
	        "  relative H%% V%%:  The viewport is set to a 4:3 aspect ratio rectangle fit into\n"
	        "                   the available window/screen, then it's scaled by the H and V\n"
	        "                   horizontal and vertical scaling factors (valid range is from\n"
	        "                   20%% to 300%%). The resulting viewport is allowed to extend\n"
	        "                   beyond the window/screen. Useful to force arbitrary display\n"
	        "                   aspect ratios with 'aspect = stretch' and to zoom into the\n"
	        "                   image. This effectively emulates the horizontal and vertical\n"
	        "                   stretch controls of CRT monitors.\n"
	        "Notes:\n"
	        "  - Using 'relative' mode with 'integer_scaling' enabled could lead to\n"
	        "    surprising (but correct) results.\n"
	        "  - You can use the 'Stretch Axis', 'Inc Stretch', and 'Dec Stretch' hotkey\n"
	        "    actions to set the stretch in 'relative' mode in real-time.");

	string_prop = secprop.Add_string("monochrome_palette",
	                                 always,
	                                 MonochromePaletteAmber);
	string_prop->Set_help(
	        "Set the palette for monochrome display emulation ('amber' by default).\n"
	        "Works only with the 'hercules' and 'cga_mono' machine types.\n"
	        "Note: You can also cycle through the available palettes via hotkeys.");

	string_prop->Set_values({MonochromePaletteAmber,
	                         MonochromePaletteGreen,
	                         MonochromePaletteWhite,
	                         MonochromePalettePaperwhite});

	string_prop = secprop.Add_string("cga_colors", only_at_start, "default");
	string_prop->Set_help(
	        "Set the interpretation of CGA RGBI colours ('default' by default). Affects all\n"
	        "machine types capable of displaying CGA or better graphics. Built-in presets:\n"
	        "  default:       The canonical CGA palette, as emulated by VGA adapters\n"
	        "                 (default).\n"
	        "  tandy <bl>:    Emulation of an idealised Tandy monitor with adjustable brown\n"
	        "                 level. The brown level can be provided as an optional second\n"
	        "                 parameter (0 - red, 50 - brown, 100 - dark yellow;\n"
	        "                 defaults to 50). E.g. tandy 100\n"
	        "  tandy-warm:    Emulation of the actual colour output of an unknown Tandy\n"
	        "                 monitor.\n"
	        "  ibm5153 <c>:   Emulation of the actual colour output of an IBM 5153 monitor\n"
	        "                 with a unique contrast control that dims non-bright colours\n"
	        "                 only. The contrast can be optionally provided as a second\n"
	        "                 parameter (0 to 100; defaults to 100), e.g. ibm5153 60\n"
	        "  agi-amiga-v1, agi-amiga-v2, agi-amiga-v3:\n"
	        "                 Palettes used by the Amiga ports of Sierra AGI games.\n"
	        "  agi-amigaish:  A mix of EGA and Amiga colours used by the Sarien\n"
	        "                 AGI-interpreter.\n"
	        "  scumm-amiga:   Palette used by the Amiga ports of LucasArts EGA games.\n"
	        "  colodore:      Commodore 64 inspired colours based on the Colodore palette.\n"
	        "  colodore-sat:  Colodore palette with 20%% more saturation.\n"
	        "  dga16:         A modern take on the canonical CGA palette with dialed back\n"
	        "                 contrast.\n"
	        "You can also set custom colours by specifying 16 space or comma separated\n"
	        "colour values, either as 3 or 6-digit hex codes (e.g. #f00 or #ff0000 for full\n"
	        "red), or decimal RGB triplets (e.g. (255, 0, 255) for magenta). The 16 colours\n"
	        "are ordered as follows:\n"
	        "  black, blue, green, cyan, red, magenta, brown, light-grey, dark-grey,\n"
	        "  light-blue, light-green, light-cyan, light-red, light-magenta, yellow, white.\n"
	        "Their default values, shown here in 6-digit hex code format, are:\n"
	        "  #000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa\n"
	        "  #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff");

	string_prop = secprop.Add_string("scaler", deprecated, "none");
	string_prop->Set_help(
	        "Software scalers are deprecated in favour of hardware-accelerated options:\n"
	        "  - If you used the normal2x/3x scalers, set the desired 'windowresolution'\n"
	        "    or 'viewport' instead, or consider using 'integer_scaling'.\n"
	        "  - If you used an advanced scaler, consider one of the 'glshader'\n"
	        "    options instead.");
}

enum { Horiz, Vert };

static auto current_stretch_axis       = Horiz;
static constexpr auto StretchIncrement = 0.01f;

static void log_stretch_hotkeys_viewport_mode_warning()
{
	LOG_WARNING("RENDER: Viewport stretch hotkeys are only supported in 'relative' "
	            "viewport mode");
}

static void toggle_stretch_axis(const bool pressed)
{
	if (!pressed) {
		return;
	}
	if (viewport_settings.mode != ViewportMode::Relative) {
		log_stretch_hotkeys_viewport_mode_warning();
		return;
	}

	if (current_stretch_axis == Horiz) {
		current_stretch_axis = Vert;
		LOG_INFO("RENDER: Vertical viewport stretch axis selected");
	} else {
		current_stretch_axis = Horiz;
		LOG_INFO("RENDER: Horizontal viewport stretch axis selected");
	}
}

static void adjust_viewport_stretch(const float increment)
{
	if (viewport_settings.mode != ViewportMode::Relative) {
		log_stretch_hotkeys_viewport_mode_warning();
		return;
	}

	auto& r = viewport_settings.relative;

	// Snap to whole percents when using the adjustment controls
	r.width_scale = roundf(r.width_scale * 100.f) / 100.f;

	if (current_stretch_axis == Horiz) {
		r.width_scale += increment;

		r.width_scale = clamp(r.width_scale,
		                      MinRelativeScaleFactor,
		                      MaxRelativeScaleFactor);
	} else {
		r.height_scale += increment;

		r.height_scale = clamp(r.height_scale,
		                       MinRelativeScaleFactor,
		                       MaxRelativeScaleFactor);
	}

	LOG_INFO("RENDER: Current viewport setting: 'relative %d%% %d%%'",
	         iroundf(r.width_scale * 100.0f),
	         iroundf(r.height_scale * 100.0f));

	VGA_SetupDrawing(0);
}

static void increase_viewport_stretch(const bool pressed)
{
	if (pressed) {
		adjust_viewport_stretch(StretchIncrement);
	}
}

static void decrease_viewport_stretch(const bool pressed)
{
	if (pressed) {
		adjust_viewport_stretch(-StretchIncrement);
	}
}

void RENDER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	Section_prop* sec = conf->AddSection_prop("render",
	                                          &RENDER_Init,
	                                          changeable_at_runtime);

	MAPPER_AddHandler(toggle_stretch_axis,
	                  SDL_SCANCODE_UNKNOWN,
	                  0,
	                  "stretchax",
	                  "Stretch Axis");

	MAPPER_AddHandler(increase_viewport_stretch,
	                  SDL_SCANCODE_UNKNOWN,
	                  0,
	                  "incstretch",
	                  "Inc Stretch");

	MAPPER_AddHandler(decrease_viewport_stretch,
	                  SDL_SCANCODE_UNKNOWN,
	                  0,
	                  "decstretch",
	                  "Dec Stretch");

	assert(sec);
	init_render_settings(*sec);
}

void RENDER_SyncMonochromePaletteSetting(const enum MonochromePalette palette)
{
	set_section_property_value("render", "monochrome_palette", to_string(palette));
}

static bool handle_shader_changes()
{
	if (GFX_GetRenderingBackend() != RenderingBackend::OpenGl) {
		return false;
	}

	auto& shader_manager = get_shader_manager();

	constexpr auto glshader_setting_name = "glshader";

	if (GFX_GetRenderingBackend() == RenderingBackend::OpenGl) {
		const auto section     = get_render_section();
		const auto shader_name = shader_manager.MapShaderName(
		        section->Get_string(glshader_setting_name));

		shader_manager.NotifyGlshaderSettingChanged(shader_name);

		const auto string_prop = section->GetStringProp(glshader_setting_name);
		string_prop->SetValue(shader_name);
	}
	const auto new_shader_name = shader_manager.GetCurrentShaderInfo().name;

	const auto shader_changed = render.force_reload_shader ||
	                            (new_shader_name != render.current_shader_name);

	if (render.force_reload_shader) {
		shader_manager.ReloadCurrentShader();
	}

	render.force_reload_shader = false;
	render.current_shader_name = new_shader_name;

	return shader_changed;
}

void RENDER_Init(Section* sec)
{
	Section_prop* section = static_cast<Section_prop*>(sec);
	assert(section);

	// For restarting the renderer
	static auto running = false;

	// Store previous values of settings that should trigger a reinit
	const auto prev_scale_size              = render.scale.size;
	const auto prev_force_vga_single_scan   = force_vga_single_scan;
	const auto prev_force_no_pixel_doubling = force_no_pixel_doubling;
	const auto prev_integer_scaling_mode    = GFX_GetIntegerScalingMode();
	const auto prev_viewport_settings       = viewport_settings;
	const auto prev_aspect_ratio_correction_mode = aspect_ratio_correction_mode;

	render.pal.first = 256;
	render.pal.last  = 0;

	// Get aspect ratio correction mode & force square pixels if requested
	aspect_ratio_correction_mode = get_aspect_ratio_correction_mode_setting();

	if (const auto& settings = parse_viewport_settings(
	            section->Get_string("viewport"));
	    settings) {
		viewport_settings = *settings;
	} else {
		viewport_settings = get_default_viewport_settings();
		set_default_viewport_setting();
	}

	// Set monochrome palette
	const auto mono_palette = to_monochrome_palette_enum(
	        section->Get_string("monochrome_palette").c_str());
	VGA_SetMonochromePalette(mono_palette);

	// Only use the default 1x rendering scaler
	render.scale.size = 1;

	GFX_SetIntegerScalingMode(get_integer_scaling_mode_setting());

	auto shader_changed = handle_shader_changes();

	setup_scan_and_pixel_doubling();

	const auto needs_reinit =
	        ((aspect_ratio_correction_mode != prev_aspect_ratio_correction_mode) ||
	         (viewport_settings != prev_viewport_settings) ||
	         (render.scale.size != prev_scale_size) ||
	         (GFX_GetIntegerScalingMode() != prev_integer_scaling_mode) ||
	         shader_changed ||
	         (prev_force_vga_single_scan != force_vga_single_scan) ||
	         (prev_force_no_pixel_doubling != force_no_pixel_doubling));

	if (running && needs_reinit) {
		render_callback(GFX_CallBackReset);
		VGA_SetupDrawing(0);
	}
	if (!running) {
		render.updating = true;
	}

	running = true;

	MAPPER_AddHandler(reload_shader,
	                  SDL_SCANCODE_F2,
	                  PRIMARY_MOD,
	                  "reloadshader",
	                  "Reload Shader");
}
