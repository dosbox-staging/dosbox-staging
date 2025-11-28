// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <mutex>

#include "capture/capture.h"
#include "config/config.h"
#include "config/setup.h"
#include "gui/common.h"
#include "gui/mapper.h"
#include "gui/render/render.h"
#include "gui/render/render_backend.h"
#include "hardware/video/vga.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "misc/video.h"
#include "shell/shell.h"
#include "utils/checks.h"
#include "utils/fraction.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

Render render;
ScalerLineHandler_t RENDER_DrawLine;

static void render_callback(GFX_CallbackFunctions_t function);

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

			// TODO This can't possibly work with the OpenGL
			// renderer at the very least which will always return a
			// 32-bit XRGB value.
			//
			uint16_t new_pal = GFX_MakePixel(r, g, b);
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

			uint32_t new_pal = GFX_MakePixel(r, g, b);
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
	GFX_EndUpdate();
	render.updating = false;
	render.active   = false;
}

void RENDER_EndUpdate([[maybe_unused]] bool abort)
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

	GFX_EndUpdate();

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

static SectionProp& get_render_section()
{
	auto section = get_section("render");
	assert(section);

	return *section;
}

static uint8_t get_best_mode(const uint8_t flags)
{
	return (flags & GFX_CAN_32) & ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
}

static void reinit_drawing()
{
	render_callback(GFX_CallbackReset);
	VGA_SetupDrawing(0);
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

	uint8_t xscale, yscale;
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

	xscale    = simpleBlock->xscale;
	yscale    = simpleBlock->yscale;
	//		LOG_MSG("Scaler:%s",simpleBlock->name);

	constexpr auto src_pixel_bytes = sizeof(uintptr_t);

	switch (render.src.pixel_format) {
	case PixelFormat::Indexed8:
	case PixelFormat::RGB555_Packed16:
	case PixelFormat::RGB565_Packed16:
		render.src_start = (render.src.width * 2) / src_pixel_bytes;
		break;
	case PixelFormat::BGR24_ByteArray:
		render.src_start = (render.src.width * 3) / src_pixel_bytes;
		break;
	case PixelFormat::BGRX32_ByteArray:
		render.src_start = (render.src.width * 4) / src_pixel_bytes;
		break;
	}

	render_width_px *= xscale;
	const auto render_height_px = make_aspect_table(render.src.height,
	                                                yscale,
	                                                yscale);

	const auto render_pixel_aspect_ratio = render.src.pixel_aspect_ratio;

	GFX_SetSize(render_width_px,
	            render_height_px,
	            render_pixel_aspect_ratio,
	            double_width,
	            double_height,
	            render.src.video_mode,
	            &render_callback);

	// Set up scaler variables
	render.scale.outMode = scalerMode32;

	const auto lineBlock = &simpleBlock->Random;

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

	// Reset the palette change detection to its initial value
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

static void render_callback(GFX_CallbackFunctions_t function)
{
	if (function == GFX_CallbackStop) {
		halt_render();
		return;
	} else if (function == GFX_CallbackRedraw) {
		render.scale.clearCache = true;
		return;
	} else if (function == GFX_CallbackReset) {
		GFX_EndUpdate();
		render_reset();
	} else {
		E_Exit("Unhandled GFX_CallbackReset %d", function);
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
static void set_scan_and_pixel_doubling()
{
	bool force_vga_single_scan   = false;
	bool force_no_pixel_doubling = false;

	switch (GFX_GetRenderBackendType()) {
	case RenderBackendType::Sdl: {
		const auto nearest_neighbour_on = (GFX_GetTextureFilterMode() ==
		                                   TextureFilterMode::NearestNeighbour);

		force_vga_single_scan   = nearest_neighbour_on;
		force_no_pixel_doubling = nearest_neighbour_on;
	} break;

	case RenderBackendType::OpenGl: {
		const auto shader_preset = GFX_GetRenderer()->GetCurrentShaderPreset();

		force_vga_single_scan = shader_preset.settings.force_single_scan;
		force_no_pixel_doubling = shader_preset.settings.force_no_pixel_doubling;
	} break;

	default: assertm(false, "Invalid RenderindBackend value");
	}

	VGA_AllowVgaScanDoubling(!force_vga_single_scan);
	VGA_AllowPixelDoubling(!force_no_pixel_doubling);
}

bool RENDER_MaybeAutoSwitchShader(const VideoMode& video_mode, const bool reinit_render)
{
	const auto curr_preset = GFX_GetRenderer()->GetCurrentShaderPreset(); //-V821

	const auto shader_changed = GFX_GetRenderer()->MaybeAutoSwitchShader(video_mode);

	if (shader_changed) {
		set_scan_and_pixel_doubling();

		const auto new_preset = GFX_GetRenderer()->GetCurrentShaderPreset(); //-V821

		if (reinit_render) {
			// No need to reinit the renderer if the double scaning
			// / pixel doubling settings have not been changed.
			const auto render_params_changed =
			        ((curr_preset.settings.force_single_scan !=
			          new_preset.settings.force_single_scan) ||
			         (curr_preset.settings.force_no_pixel_doubling !=
			          new_preset.settings.force_no_pixel_doubling));

			if (render_params_changed) {
				reinit_drawing();
			}
		}
	}
	return shader_changed;
}

void RENDER_NotifyEgaModeWithVgaPalette()
{
	// If we're getting these notifications on non-VGA cards, that's a
	// programming error.
	assert(is_machine_vga_or_better());

	auto video_mode = VGA_GetCurrentVideoMode();

	if (!video_mode.has_vga_colors) {
		video_mode.has_vga_colors = true;

		// We are potentially auto-switching to a VGA shader now.
		constexpr auto ReinitRender = true;
		RENDER_MaybeAutoSwitchShader(video_mode, ReinitRender);
	}
}

std::deque<std::string> RENDER_GenerateShaderInventoryMessage()
{
	return ShaderManager::GetInstance().GenerateShaderInventoryMessage();
}

void RENDER_AddMessages()
{
	ShaderManager::AddMessages();
}

static bool set_shader(const std::string& descriptor)
{
	using enum RenderBackend::SetShaderResult;

	switch (GFX_GetRenderer()->SetShader(descriptor)) {
	case ShaderError: return false;

	case PresetError:
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "RENDER_DEFAULT_SHADER_PRESET_FALLBACK",
		                      descriptor.c_str(),
		                      ShaderName::Sharp);

		set_scan_and_pixel_doubling();
		return true;

	case Ok: set_scan_and_pixel_doubling(); return true;

	default: assertm(false, "Invalid SetShaderResult value"); return false;
	}
}

static void set_fallback_shader_or_exit()
{
	if (set_shader(SymbolicShaderName::AutoGraphicsStandard)) {
		set_section_property_value("render",
		                           "shader",
		                           SymbolicShaderName::AutoGraphicsStandard);
		return;
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "RENDER",
	                      "RENDER_SHADER_FALLBACK",
	                      SymbolicShaderName::AutoGraphicsStandard,
	                      ShaderName::Sharp);

	if (set_shader(ShaderName::Sharp)) {
		set_section_property_value("render", "shader", ShaderName::Sharp);
		return;
	}

	E_Exit("RENDER: Error loading fallback shaders, exiting");
}

static void set_shader_with_fallback_or_exit(const std::string& shader_descriptor)
{
	if (!set_shader(shader_descriptor)) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "RENDER_SHADER_FALLBACK",
		                      shader_descriptor.c_str(),
		                      SymbolicShaderName::AutoGraphicsStandard);

		set_fallback_shader_or_exit();
	}
}

static void reload_shader([[maybe_unused]] const bool pressed)
{
	if (!pressed) {
		return;
	}

	const auto shader_descriptor =
	        ShaderManager::GetInstance().GetCurrentShaderDescriptor();

	LOG_MSG("RENDER: Reloading shader '%s'",
	        shader_descriptor.ToString().c_str());

	if (!GFX_GetRenderer()->ForceReloadCurrentShader()) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "RENDER_SHADER_FALLBACK",
		                      shader_descriptor.ToString().c_str(),
		                      SymbolicShaderName::AutoGraphicsStandard);

		set_fallback_shader_or_exit();
	}

	set_scan_and_pixel_doubling();

	// The shader settings might have been changed (e.g. force_single_scan,
	// force_no_pixel_doubling), so force re-rendering the image using the
	// new settings. Without this, the altered settings would only take
	// effect on the next video mode change.
	//
	reinit_drawing();
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

static AspectRatioCorrectionMode get_aspect_ratio_correction_mode_setting(SectionProp& section)
{
	const std::string mode = section.GetString("aspect");

	if (has_true(mode) || mode == "auto") {
		return AspectRatioCorrectionMode::Auto;

	} else if (has_false(mode) || mode == "square-pixels") {
		return AspectRatioCorrectionMode::SquarePixels;

	} else if (mode == "stretch") {
		return AspectRatioCorrectionMode::Stretch;

	} else {
		constexpr auto SettingName  = "aspect";
		constexpr auto DefaultValue = "auto";

		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      SettingName,
		                      mode.c_str(),
		                      DefaultValue);

		return AspectRatioCorrectionMode::Auto;
	}
}

static void set_aspect_ratio_correction(SectionProp& section)
{
	aspect_ratio_correction_mode = get_aspect_ratio_correction_mode_setting(section);
}

AspectRatioCorrectionMode RENDER_GetAspectRatioCorrectionMode()
{
	return aspect_ratio_correction_mode;
}

static void log_invalid_viewport_setting_warning(
        const std::string& pref,
        const std::optional<const std::string> extra_info = {})
{
	constexpr auto SettingName  = "viewport";
	constexpr auto DefaultValue = "fit";

	if (extra_info) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "PROGRAM_CONFIG_INVALID_SETTING_WITH_DETAILS",
		                      SettingName,
		                      pref.c_str(),
		                      extra_info->c_str(),
		                      DefaultValue);
	} else {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      SettingName,
		                      pref.c_str(),
		                      DefaultValue);
	}
}

std::optional<std::pair<int, int>> parse_int_dimensions(const std::string_view s)
{
	const auto parts = split(s, "x");
	if (parts.size() == 2) {
		const auto w = parse_int(parts[0]);
		const auto h = parse_int(parts[1]);
		if (w && h) {
			return {
			        {*w, *h}
                        };
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

static void set_viewport(SectionProp& section)
{
	if (const auto& settings = parse_viewport_settings(
	            section.GetString("viewport"));
	    settings) {
		viewport_settings = *settings;
	} else {
		viewport_settings = {};
		set_section_property_value("render", "viewport", "fit");
	}
}

static IntegerScalingMode get_integer_scaling_mode_setting(SectionProp& section)
{
	const std::string mode = section.GetString("integer_scaling");

	if (has_false(mode)) {
		return IntegerScalingMode::Off;

	} else if (mode == "auto") {
		return IntegerScalingMode::Auto;

	} else if (mode == "horizontal") {
		return IntegerScalingMode::Horizontal;

	} else if (mode == "vertical") {
		return IntegerScalingMode::Vertical;

	} else {
		constexpr auto SettingName  = "integer_scaling";
		constexpr auto DefaultValue = "auto";

		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      SettingName,
		                      mode.c_str(),
		                      DefaultValue);

		return IntegerScalingMode::Auto;
	}
}

static void set_integer_scaling(SectionProp& section)
{
	render.integer_scaling_mode = get_integer_scaling_mode_setting(section);
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

DosBox::Rect RENDER_CalcDrawRectInPixels(const DosBox::Rect& canvas_size_px,
                                         const DosBox::Rect& render_size_px,
                                         const Fraction& render_pixel_aspect_ratio)
{
	const auto viewport_px = RENDER_CalcRestrictedViewportSizeInPixels(
	        canvas_size_px);

	const auto draw_size_fit_px =
	        render_size_px.Copy()
	                .ScaleWidth(render_pixel_aspect_ratio.ToFloat())
	                .ScaleSizeToFit(viewport_px);

	auto calc_horiz_integer_scaling_dims_in_pixels = [&]() {
		auto integer_scale_factor = iroundf(draw_size_fit_px.w) /
		                            iroundf(render_size_px.w);
		if (integer_scale_factor < 1) {
			// Revert to fit to viewport
			return draw_size_fit_px;
		} else {
			const auto vert_scale =
			        render_pixel_aspect_ratio.Inverse().ToFloat();

			return render_size_px.Copy()
			        .ScaleSize(integer_scale_factor)
			        .ScaleHeight(vert_scale);
		}
	};

	auto calc_vert_integer_scaling_dims_in_pixels = [&](const float integer_scale_factor) {
		if (integer_scale_factor < 1) {
			// Revert to fit to viewport
			return draw_size_fit_px;
		} else {
			const auto horiz_scale = render_pixel_aspect_ratio.ToFloat();

			return render_size_px.Copy()
			        .ScaleSize(integer_scale_factor)
			        .ScaleWidth(horiz_scale);
		}
	};

	auto handle_auto_mode = [&] {
		// The 'auto' mode is special:
		//
		// - it enables vertical integer scaling for the adaptive CRT
		//   shaders if the viewport is large enough (otherwise it falls
		//   back to the 'sharp' shader with no integer scaling),
		// - it allows the 3.5x and 4.5x half steps,
		// - and it disables integer scaling above 5.0x scaling.
		//
		// The half-steps and no scaling above 5.0x result in no moire
		// artifacts in 99% of cases, so it's very much worth it for
		// better viewport utilisation.
		//
		if (GFX_GetRenderBackendType() != RenderBackendType::OpenGl) {
			return draw_size_fit_px;
		}

		if (GFX_GetRenderer()->GetCurrentShaderInfo().is_adaptive) {
			auto integer_scale_factor = [&] {
				const auto factor = draw_size_fit_px.h /
				                    render_size_px.h;
				if (factor >= 5.0) {
					// Disable integer scaling above 5.0x
					// vertical scaling
					return factor;
				}
				if (factor >= 3.0) {
					// Allow 3.5x and 4.5x half steps in
					// the 3.0x to 5.0x vertical scaling range
					return floorf(factor * 2) / 2;
				}
				// Allow only integer steps below 3.0x vertical
				// scaling
				return floorf(factor);
			}();

			return calc_vert_integer_scaling_dims_in_pixels(
			        integer_scale_factor);
		}

		// Handles the `sharp` shader fallback when the viewport is
		// is too small for CRT shaders; integer scaling is then disabled.
		return draw_size_fit_px;
	};

	auto draw_size_px = [&] {
		switch (render.integer_scaling_mode) {
		case IntegerScalingMode::Off: return draw_size_fit_px;

		case IntegerScalingMode::Auto: return handle_auto_mode();

		case IntegerScalingMode::Horizontal:
			return calc_horiz_integer_scaling_dims_in_pixels();

		case IntegerScalingMode::Vertical: {
			auto integer_scale_factor = floorf(draw_size_fit_px.h /
			                                   render_size_px.h);
			return calc_vert_integer_scaling_dims_in_pixels(
			        integer_scale_factor);
		}

		default:
			assertm(false, "Invalid IntegerScalingMode value");
			return DosBox::Rect{};
		}
	}();

	return draw_size_px.CenterTo(canvas_size_px.cx(), canvas_size_px.cy());
}

std::string RENDER_GetCgaColorsSetting()
{
	return get_render_section().GetString("cga_colors");
}

static void init_render_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto int_prop = section.AddInt("frameskip", Deprecated, 0);
	int_prop->SetHelp(
	        "The [color=light-green]'frameskip'[reset] setting has been removed; "
	        "consider capping frame rates using the\n"
	        "[color=light-green]'host_rate'[reset] setting instead.");

	auto string_prop = section.AddString("glshader", DeprecatedButAllowed, "");
	string_prop->SetHelp(
	        "The [color=light-green]'glshader'[reset] setting is deprecated but still accepted;\n"
	        "please use [color=light-green]'shader'[reset] instead.");

	string_prop = section.AddString("shader", Always, "crt-auto");
	string_prop->SetOptionHelp(
	        "Set an adaptive CRT monitor emulation shader or a regular shader ('crt-auto' by\n"
	        "default). Shaders are only supported in the OpenGL output mode (see 'output').\n"
	        "Adaptive CRT shader options:\n"
	        "\n"
	        "  crt-auto:               Adaptive CRT shader that prioritises developer intent\n"
	        "                          and how people experienced the games at the time of\n"
	        "                          release (default). An appropriate shader variant is\n"
	        "                          auto-selected based the graphics standard of the\n"
	        "                          current video mode and the viewport size, irrespective\n"
	        "                          of the 'machine' setting. This means you'll get the\n"
	        "                          authentic single-scanned CGA and EGA monitor look with\n"
	        "                          visible scanlines in CGA and EGA games even on an\n"
	        "                          emulated VGA adapter. The sharp shader is used below\n"
	        "                          3.0x vertical scaling.\n"
	        "\n"
	        "  crt-auto-machine:       A variation of 'crt-auto'; this emulates a fixed CRT\n"
	        "                          monitor for the video adapter configured via the\n"
	        "                          'machine' setting. E.g., CGA and EGA games will appear\n"
	        "                          double-scanned on an emulated VGA adapter.\n"
	        "\n"
	        "  crt-auto-arcade:        Emulation of an arcade or home computer monitor with\n"
	        "                          a less sharp image and thick scanlines in low-\n"
	        "                          resolution video modes. This is a fantasy option that\n"
	        "                          never existed in real life, but it can be a lot of\n"
	        "                          fun, especially with DOS ports of Amiga games.\n"
	        "\n"
	        "  crt-auto-arcade-sharp:  A sharper arcade shader variant for those who like the\n"
	        "                          thick scanlines but want to retain the sharpness of a\n"
	        "                          typical PC monitor.\n"
	        "\n"
	        "Other shader options include (non-exhaustive list):\n"
	        "\n"
	        "  sharp:     Upscale the image treating the pixels as small rectangles,\n"
	        "             resulting in a sharp image with minimum blur while maintaining\n"
	        "             the correct pixel aspect ratio. This is the recommended option for\n"
	        "             those who don't want to use the adaptive CRT shaders.\n"
	        "\n"
	        "  bilinear:  Upscale the image using bilinear interpolation (results in a blurry\n"
	        "             image).\n"
	        "\n"
	        "  nearest:   Upscale the image using nearest-neighbour interpolation (also known\n"
	        "             as \"no bilinear\"). This results in the sharpest possible image at\n"
	        "             the expense of uneven pixels, especially with non-square pixel\n"
	        "             aspect ratios (this is less of an issue on high resolution\n"
	        "             monitors).\n"
	        "\n"
	        "Start DOSBox Staging with the '--list-shaders' command line option to see the\n"
	        "full list of available shaders. You can also use an absolute or relative path to\n"
	        "a file. In all cases, you may omit the shader's '.glsl' file extension.");
	string_prop->SetEnabledOptions({
#if C_OPENGL
	        "shader",
#endif
	});

	string_prop = section.AddString("aspect", Always, "auto");
	string_prop->SetHelp(
	        "Set the aspect ratio correction mode ('auto' by default). Possible values:\n"
	        "\n"
	        "  auto, on:            Apply aspect ratio correction for modern square-pixel\n"
	        "                       flat-screen displays, so DOS video modes with non-square\n"
	        "                       pixels appear as they would on a 4:3 display aspect\n"
	        "                       ratio CRT monitor the majority of DOS games were designed\n"
	        "                       for. This setting only affects video modes that use non-\n"
	        "                       square pixels, such as 320x200 or 640x400; square pixel\n"
	        "                       modes (e.g., 320x240, 640x480, and 800x600), are\n"
	        "                       displayed as-is.\n"
	        "\n"
	        "  square-pixels, off:  Don't apply aspect ratio correction; all DOS video modes\n"
	        "                       will be displayed with square pixels. Most 320x200 games\n"
	        "                       will appear squashed, but a minority of titles (e.g.,\n"
	        "                       DOS ports of PAL Amiga games) need square pixels to\n"
	        "                       appear as the artists intended.\n"
	        "\n"
	        "  stretch:             Calculate the aspect ratio from the viewport's\n"
	        "                       dimensions. Combined with the 'viewport' setting, this\n"
	        "                       mode is useful to force arbitrary aspect ratios (e.g.,\n"
	        "                       stretching DOS games to fullscreen on 16:9 displays) and\n"
	        "                       to emulate the horizontal and vertical stretch controls\n"
	        "                       of CRT monitors.");

	string_prop->SetValues({"auto", "on", "square-pixels", "off", "stretch"});

	string_prop = section.AddString("integer_scaling", Always, "auto");
	string_prop->SetHelp(
	        "Constrain the horizontal or vertical scaling factor to the largest integer\n"
	        "value so the image still fits into the viewport ('auto' by default). The\n"
	        "configured aspect ratio is always maintained according to the 'aspect' and\n"
	        "'viewport' settings, which may result in a non-integer scaling factor in the\n"
	        "other dimension. If the image is larger than the viewport, the integer scaling\n"
	        "constraint is auto-disabled (same as 'off'). Possible values:\n"
	        "\n"
	        "  auto:        A special vertical mode auto-enabled only for the adaptive CRT\n"
	        "               shaders (see `shader`). This mode has refinements over standard\n"
	        "               vertical integer scaling: 3.5x and 4.5x scaling factors are also\n"
	        "               allowed, and integer scaling is disabled above 5.0x scaling.\n"
	        "\n"
	        "  vertical:    Constrain the vertical scaling factor to integer values.\n"
	        "               This is the recommended setting for third-party shaders to avoid\n"
	        "               uneven scanlines and interference artifacts.\n"
	        "\n"
	        "  horizontal:  Constrain the horizontal scaling factor to integer values.\n"
	        "\n"
	        "  off:         No integer scaling constraint is applied; the image fills the\n"
	        "               viewport while maintaining the configured aspect ratio.");

	string_prop->SetValues({"auto", "vertical", "horizontal", "off"});

	string_prop = section.AddString("viewport", Always, "fit");
	string_prop->SetHelp(
	        "Set the viewport size ('fit' by default). This is the maximum drawable area;\n"
	        "the video output is always contained within the viewport while taking the\n"
	        "configured aspect ratio into account (see 'aspect'). Possible values:\n"
	        "\n"
	        "  fit:               Fit the viewport into the available window/screen\n"
	        "                     (default). There might be padding (black areas) around the\n"
	        "                     image with 'integer_scaling' enabled.\n"
	        "\n"
	        "  WxH:               Set a fixed viewport size in WxH format in logical units\n"
	        "                     (e.g., 960x720). The specified size must not be larger than\n"
	        "                     the desktop. If it's larger than the window size, it will\n"
	        "                     be scaled to fit within the window.\n"
	        "\n"
	        "  N%%:                Similar to 'WxH', but the size is specified as a percentage\n"
	        "                     of the desktop size.\n"
	        "\n"
	        "  relative H%% V%%:    The viewport is set to a 4:3 aspect ratio rectangle fit\n"
	        "                     into the available window or screen, then is scaled by\n"
	        "                     the H and V horizontal and vertical scaling factors (valid\n"
	        "                     range is from 20%% to 300%%). The resulting viewport is\n"
	        "                     allowed to extend beyond the bounds of the window or\n"
	        "                     screen. Useful to force arbitrary display aspect ratios\n"
	        "                     with 'aspect = stretch' and to \"zoom\" into the image.\n"
	        "                     This effectively emulates the horizontal and vertical\n"
	        "                     stretch controls of CRT monitors.\n"
	        "\n"
	        "Notes:\n"
	        "  - Using 'relative' mode with 'integer_scaling' enabled could lead to\n"
	        "    surprising (but correct) results.\n"
	        "  - Use the 'Stretch Axis', 'Inc Stretch', and 'Dec Stretch' hotkey actions to\n"
	        "    adjust the image size in 'relative' mode in real-time, then copy the new\n"
	        "    settings from the logs into your config.");

	string_prop = section.AddString("monochrome_palette",
	                                Always,
	                                MonochromePaletteAmber);
	string_prop->SetHelp(
	        "Set the palette for monochrome display emulation ('amber' by default).\n"
	        "Works only with the 'hercules' and 'cga_mono' machine types. Possible values:\n"
	        "\n"
	        "  amber:       Amber palette (default).\n"
	        "  green:       Green palette.\n"
	        "  white:       White palette.\n"
	        "  paperwhite:  Paperwhite palette.\n"
	        "\n"
	        "Note: You can also cycle through the available palettes via hotkeys.");

	string_prop->SetValues({MonochromePaletteAmber,
	                        MonochromePaletteGreen,
	                        MonochromePaletteWhite,
	                        MonochromePalettePaperwhite});

	string_prop = section.AddString("cga_colors", OnlyAtStart, "default");
	string_prop->SetHelp(
	        "Set the interpretation of CGA RGBI colours ('default' by default). Affects all\n"
	        "machine types capable of displaying CGA or better graphics. Built-in presets:\n"
	        "\n"
	        "  default:       The canonical CGA palette, as emulated by VGA adapters\n"
	        "                 (default).\n"
	        "\n"
	        "  tandy <bl>:    Emulation of an idealised Tandy monitor with adjustable brown\n"
	        "                 level. The brown level can be provided as an optional second\n"
	        "                 parameter (0 - red, 50 - brown, 100 - dark yellow;\n"
	        "                 defaults to 50). E.g. tandy 100\n"
	        "\n"
	        "  tandy-warm:    Emulation of the actual colour output of an unknown Tandy\n"
	        "                 monitor.\n"
	        "\n"
	        "  ibm5153 <c>:   Emulation of the actual colour output of an IBM 5153 monitor\n"
	        "                 with a unique contrast control that dims non-bright colours\n"
	        "                 only. The contrast can be optionally provided as a second\n"
	        "                 parameter (0 to 100; defaults to 100), e.g. ibm5153 60\n"
	        "\n"
	        "  agi-amiga-v1, agi-amiga-v2, agi-amiga-v3:\n"
	        "                 Palettes used by the Amiga ports of Sierra AGI games.\n"
	        "\n"
	        "  agi-amigaish:  A mix of EGA and Amiga colours used by the Sarien\n"
	        "                 AGI-interpreter.\n"
	        "\n"
	        "  scumm-amiga:   Palette used by the Amiga ports of LucasArts EGA games.\n"
	        "  colodore:      Commodore 64 inspired colours based on the Colodore palette.\n"
	        "  colodore-sat:  Colodore palette with 20%% more saturation.\n"
	        "\n"
	        "  dga16:         A modern take on the canonical CGA palette with dialed back\n"
	        "                 contrast.\n"
	        "\n"
	        "You can also set custom colours by specifying 16 space or comma separated\n"
	        "colour values, either as 3 or 6-digit hex codes (e.g. #f00 or #ff0000 for full\n"
	        "red), or decimal RGB triplets (e.g. (255, 0, 255) for magenta). The 16 colours\n"
	        "are ordered as follows:\n"
	        "\n"
	        "  black, blue, green, cyan, red, magenta, brown, light-grey, dark-grey,\n"
	        "  light-blue, light-green, light-cyan, light-red, light-magenta, yellow, white.\n"
	        "\n"
	        "Their default values, shown here in 6-digit hex code format, are:\n"
	        "\n"
	        "  #000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa\n"
	        "  #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff");

	string_prop = section.AddString("scaler", Deprecated, "none");
	string_prop->SetHelp(
	        "Software scalers are deprecated in favour of hardware-accelerated options:\n"
	        "  - If you used the normal2x/3x scalers, consider using [color=light-green]'integer_scaling'[reset]\n"
	        "    with [color=light-green]'shader = sharp'[reset] and optionally setting the desired [color=light-green]'window_size'[reset]\n"
	        "    or [color=light-green]'viewport'[reset] size.\n"
	        "  - If you used an advanced scaler, consider one of the [color=light-green]'shader'[reset] options.");
}

enum { Horiz, Vert };

static auto current_stretch_axis       = Horiz;
static constexpr auto StretchIncrement = 0.01f;

static void log_stretch_hotkeys_viewport_mode_warning()
{
	LOG_WARNING(
	        "RENDER: Viewport stretch hotkeys are only supported in 'relative' "
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

	reinit_drawing();
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

static std::string get_shader_setting_value()
{
	const auto legacy_pref = get_render_section().GetString("glshader");

	if (!legacy_pref.empty()) {
		set_section_property_value("render", "glshader", "");
		set_section_property_value("render", "shader", legacy_pref);
	}
	return get_render_section().GetString("shader");
}

void RENDER_SetShaderWithFallback()
{
	set_shader_with_fallback_or_exit(get_shader_setting_value());
}

static void set_monochrome_palette(SectionProp& section)
{
	const auto mono_palette = to_monochrome_palette_enum(
	        section.GetString("monochrome_palette").c_str());

	VGA_SetMonochromePalette(mono_palette);
}

void RENDER_SyncMonochromePaletteSetting(const enum MonochromePalette palette)
{
	set_section_property_value("render", "monochrome_palette", to_string(palette));
}

void RENDER_Init()
{
	auto section = get_section("render");
	assert(section);

	set_aspect_ratio_correction(*section);
	set_viewport(*section);
	set_integer_scaling(*section);

	set_monochrome_palette(*section);
}

static void notify_render_setting_updated(SectionProp& section,
                                          const std::string& prop_name)
{
	if (prop_name == "aspect") {
		set_aspect_ratio_correction(section);
		reinit_drawing();

	} else if (prop_name == "cga_colors") {
		// TODO Support switching custom CGA colors at runtime. This is
		// somewhat complicated and needs experimentation.

	} else if (prop_name == "glshader" || prop_name == "shader") {
		set_shader_with_fallback_or_exit(get_shader_setting_value());
		reinit_drawing();

		set_section_property_value(
		        "render",
		        "shader",
		        GFX_GetRenderer()->GetCurrentShaderDescriptorString());

	} else if (prop_name == "integer_scaling") {
		set_integer_scaling(section);
		reinit_drawing();

	} else if (prop_name == "monochrome_palette") {
		set_monochrome_palette(section);

	} else if (prop_name == "viewport") {
		set_viewport(section);
		reinit_drawing();
	}
}

static void register_render_text_messages()
{
	MSG_Add("RENDER_SHADER_RENAMED",
	        "Built-in shader [color=white]'%s'[reset] has been renamed to [color=white]'%s'[reset];\n"
	        "using [color=white]'%s'[reset]");

	MSG_Add("RENDER_SHADER_FALLBACK",
	        "Error setting shader [color=white]'%s'[reset],\n"
	        "falling back to [color=white]'%s'[reset]");

	MSG_Add("RENDER_DEFAULT_SHADER_PRESET_FALLBACK",
	        "Error setting shader preset [color=white]'%s'[reset],\n"
	        "falling back to default preset");
}

void RENDER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("render");

	section->AddUpdateHandler(notify_render_setting_updated);

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

	MAPPER_AddHandler(reload_shader,
	                  SDL_SCANCODE_F2,
	                  PRIMARY_MOD,
	                  "reloadshader",
	                  "Reload Shader");

	init_render_settings(*section);
	register_render_text_messages();
}
