// SPDX-FileCopyrightText:  2019-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <mutex>

#include "gui/private/auto_image_adjustments.h"

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
ScalerLineHandler RENDER_DrawLine;

static void render_callback(GFX_CallbackFunctions_t function);

static void check_palette()
{
	// Clean up any previous changed palette data
	if (render.palette.changed) {
		memset(render.palette.modified, 0, sizeof(render.palette.modified));
		render.palette.changed = false;
	}
	if (render.palette.first > render.palette.last) {
		return;
	}

	for (auto i = render.palette.first; i <= render.palette.last; i++) {
		const auto color = render.palette.rgb[i];
		const auto new_color = GFX_MakePixel(color.red, color.green, color.blue);

		if (new_color != render.palette.lut[i]) {
			render.palette.changed     = true;
			render.palette.modified[i] = 1;
			render.palette.lut[i]      = new_color;
		}
	}

	// Setup palette index to startup values
	render.palette.first = NumVgaColors;
	render.palette.last  = 0;
}

void RENDER_SetPalette(const uint8_t entry, const uint8_t red,
                       const uint8_t green, const uint8_t blue)
{
	auto& color = render.palette.rgb[entry];

	color.red   = red;
	color.green = green;
	color.blue  = blue;

	if (render.palette.first > entry) {
		render.palette.first = entry;
	}
	if (render.palette.last < entry) {
		render.palette.last = entry;
	}
}

static bool is_deinterlacing()
{
	// We only deinterlace (S)VGA and VESA modes
	if (machine != MachineType::Vga) {
		return false;
	}

	return (render.deinterlacing_strength != DeinterlacingStrength::Off);
}

static bool maybe_gfx_start_update()
{
	uint32_t* pixel_data = nullptr;
	int pitch            = 0;

	if (!GFX_StartUpdate(pixel_data, pitch)) {
		return false;
	}

	if (is_deinterlacing()) {
		// Write the scaled output to a temporary buffer first
		render.scale.out_write = reinterpret_cast<uint8_t*>(
		        render.scale.out_buf.data());

		render.dest = pixel_data;

	} else {
		// Write the scaled output directly to the render backend's
		// texture buffer
		render.scale.out_write = reinterpret_cast<uint8_t*>(pixel_data);
		render.dest            = nullptr;
	}

	render.scale.out_pitch = pitch;

	return true;
}

static void empty_line_handler(const void*) {}

static void start_line_handler(const void* src_line_data)
{
	if (src_line_data) {
		if (std::memcmp(src_line_data,
		                render.scale.cache_read,
		                render.scale.cache_pitch) != 0) {

			// This triggers transferring the pixel data to the
			// render backend if the contents of the current frame
			// has changed since the last frame. This will in turn
			// make the backend do a buffer swap (if it's
			// double-buffered). Otherwise, it will keep displaying
			// the same frame at present time without doing a buffer
			// swap followed by a texture upload to the GPU.
			//
			if (!maybe_gfx_start_update()) {
				RENDER_DrawLine = empty_line_handler;
				return;
			}

			render.scale.out_write += render.scale.out_pitch *
			                          scaler_changed_lines[0];

			render.updating_frame = true;

			RENDER_DrawLine = render.scale.line_handler;
			RENDER_DrawLine(src_line_data);
			return;
		}
	}

	render.scale.cache_read += render.scale.cache_pitch;

	scaler_changed_lines[0] += render.scale.y_scale;
}

static void finish_line_handler(const void* src_line_data)
{
	if (src_line_data) {
		std::memcpy(render.scale.cache_read,
		            src_line_data,
		            render.scale.cache_pitch);
	}

	render.scale.cache_read += render.scale.cache_pitch;
}

static void clear_cache_handler(const void* src_line_data)
{
	// `src_line_data` contains a scanline worth of pixel data. All screen
	// mode widths are multiples of 8, therefore we can access this data one
	// uint64_t at a time, regardless of pixel format (pixel can be stored
	// on 1 to 4 bytes).
	auto src = static_cast<const uint8_t*>(src_line_data);

	// The width of all screen modes and therefore `cache_pitch` too is a
	// multiple of 8 (`cache_pitch` equals the screen mode's width
	// multiplied by the number of bytes per pixel and no padding).
	//
	auto cache = render.scale.cache_read;

	constexpr auto Step = sizeof(uint64_t);
	const auto count    = render.scale.cache_pitch / Step;

	for (size_t x = 0; x < count; ++x) {
		const auto src_val = read_unaligned_uint64(src);

		write_unaligned_uint64(cache, ~src_val);

		src += Step;
		cache += Step;
	}

	render.scale.line_handler(src_line_data);
}

bool RENDER_StartUpdate()
{
	if (render.render_in_progress) {
		return false;
	}
	if (!render.active) {
		return false;
	}

	if (render.scale.line_palette_handler) {
		check_palette();
	}

	render.scale.cache_read = reinterpret_cast<uint8_t*>(
	        render.scale.cache.data());

	render.scale.out_write = nullptr;
	render.scale.out_pitch = 0;

	scaler_changed_lines[0]   = 0;
	scaler_changed_line_index = 0;

	// Set up output image dimensions
	render.scale.out_width = render.src.width *
	                         (render.src.double_width ? 2 : 1);

	render.scale.out_height = render.src.height *
	                          (render.src.double_height ? 2 : 1);

	// Clearing the cache will first process the line to make sure it's
	// never the same.
	if (render.scale.clear_cache) {

		// This will force a buffer swap & texture update in the render
		// backend (see comments in `start_line_handler()`).
		//
		if (!maybe_gfx_start_update()) {
			return false;
		}

		RENDER_DrawLine = clear_cache_handler;

		render.render_in_progress = true;
		render.updating_frame     = true;
		render.scale.clear_cache  = false;
		return true;
	}

	if (render.palette.changed) {
		// Assume palette changes always do a full screen update anyway.
		//
		// This will force a buffer swap & texture update in the render
		// backend (see comments in `start_line_handler()`).
		//
		if (!maybe_gfx_start_update()) {
			return false;
		}

		RENDER_DrawLine = render.scale.line_palette_handler;

		render.render_in_progress = true;
		return true;
	}

	// Regular path -- scaler cache reset was not request and the palette
	// hasn't been changed (for screen modes with indexed colour).
	//
	// `GFX_StartUpdate()` will be called conditionally in
	// `start_line_handler()` if the contents of the current frame differs
	// from the previous one (see comments in `start_line_handler()`).
	//
	RENDER_DrawLine = start_line_handler;

	render.render_in_progress = true;
	return true;
}

static void halt_render()
{
	RENDER_DrawLine = empty_line_handler;
	GFX_EndUpdate();

	render.render_in_progress = false;
	render.active             = false;
}

static void handle_capture_frame()
{
	RenderedImage image = {};

	image.params = render.src;
	image.pitch  = render.scale.cache_pitch;

	image.image_data = reinterpret_cast<uint8_t*>(render.scale.cache.data());

	image.palette = render.palette.rgb;

	const auto frames_per_second = static_cast<float>(render.fps);

	if (is_deinterlacing()) {
		// The pixel data in the returned new image points either to the
		// input image's data (for 32-bit BGRX images), or to the
		// deinterlacer's internal decode buffer (for any other pixel
		// format). We *must not* call `free()` on `new_image` in either
		// case as it doesn't own these pixel data buffers.
		//
		auto new_image = render.deinterlacer->Deinterlace(
		        image, render.deinterlacing_strength);

		// The image capturer will create its own deep copy the rendered
		// image (and thus of the pixel data), and will free it when
		// it's done with it.
		//
		// The video capturer doesn't create a copy, and consequently
		// doesn't free the rendered image either.
		CAPTURE_AddFrame(new_image, frames_per_second);

	} else {
		CAPTURE_AddFrame(image, frames_per_second);
	}
}

static void deinterlace_rendered_output()
{
	// Copy scaled & deinterlaced output into the render backend's
	// texture buffer (always in 32-bit BGRX pixel format)
	std::memcpy(render.dest,
	            render.scale.out_buf.data(),
	            render.scale.out_height * render.scale.out_pitch);

	// Deinterlace the render's backend buffer and leave the scaler
	// output buffer intact (as deinterlacing the scaler output
	// buffer itself would screw up the scaler diffing).
	//
	RenderedImage image = {};

	image.params              = render.src;
	image.params.width        = render.scale.out_width;
	image.params.height       = render.scale.out_height;
	image.params.pixel_format = PixelFormat::BGRX32_ByteArray;
	image.pitch               = render.scale.out_pitch;

	image.image_data = reinterpret_cast<uint8_t*>(render.dest);

	// 32-bit BGRX images will always be processed in-place, so we don't
	// care about the returned `RenderedImage` object (it's the same as the
	// input image).
	render.deinterlacer->Deinterlace(image, render.deinterlacing_strength);
}

void RENDER_EndUpdate([[maybe_unused]] bool abort)
{
	if (!render.render_in_progress) {
		return;
	}

	RENDER_DrawLine = empty_line_handler;

	if (CAPTURE_IsCapturingImage() || CAPTURE_IsCapturingVideo()) {
		handle_capture_frame();
	}

	// Only deinterlace the output if the frame has changed
	if (is_deinterlacing() && render.updating_frame) {
		deinterlace_rendered_output();
	}

	GFX_EndUpdate();

	render.render_in_progress = false;
	render.updating_frame     = false;
}

static SectionProp& get_render_section()
{
	auto section = get_section("render");
	assert(section);

	return *section;
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

	int render_width_px = render.src.width;
	bool double_width   = render.src.double_width;
	bool double_height  = render.src.double_height;

	auto scaler = &Scale1x;

	if (double_height && double_width) {
		scaler = &Scale2x;
	} else if (double_width) {
		scaler = &ScaleHoriz2x;
	} else if (double_height) {
		scaler = &ScaleVert2x;
	} else {
		scaler = &Scale1x;
	}

	render.scale.y_scale = scaler->y_scale;

	if ((render_width_px * scaler->x_scale > ScalerMaxWidth) ||
	    (render.src.height * scaler->y_scale > ScalerMaxHeight)) {
		scaler = &Scale1x;
	}

	render_width_px *= scaler->x_scale;
	const auto render_height_px = render.src.height * scaler->y_scale;

	const auto render_pixel_aspect_ratio = render.src.pixel_aspect_ratio;

	GFX_SetSize(render_width_px,
	            render_height_px,
	            render_pixel_aspect_ratio,
	            double_width,
	            double_height,
	            render.src.video_mode,
	            &render_callback);

	// Set up scaler variables
	switch (render.src.pixel_format) {
	case PixelFormat::Indexed8:
		render.scale.line_handler         = scaler->line_handlers[0];
		render.scale.line_palette_handler = scaler->line_handlers[5];
		render.scale.cache_pitch          = render.src.width * 1;
		break;

	case PixelFormat::RGB555_Packed16:
		render.scale.line_handler         = scaler->line_handlers[1];
		render.scale.line_palette_handler = nullptr;
		render.scale.cache_pitch          = render.src.width * 2;
		break;

	case PixelFormat::RGB565_Packed16:
		render.scale.line_handler         = scaler->line_handlers[2];
		render.scale.line_palette_handler = nullptr;
		render.scale.cache_pitch          = render.src.width * 2;
		break;

	case PixelFormat::BGR24_ByteArray:
		render.scale.line_handler         = scaler->line_handlers[3];
		render.scale.line_palette_handler = nullptr;
		render.scale.cache_pitch          = render.src.width * 3;
		break;

	case PixelFormat::BGRX32_ByteArray:
		render.scale.line_handler         = scaler->line_handlers[4];
		render.scale.line_palette_handler = nullptr;
		render.scale.cache_pitch          = render.src.width * 4;
		break;

	default:
		E_Exit("RENDER: Invalid pixel_format %u",
		       static_cast<uint8_t>(render.src.pixel_format));
	}

	// Reset the palette change detection to its initial value
	render.palette.first   = 0;
	render.palette.last    = 255;
	render.palette.changed = false;
	memset(render.palette.modified, 0, sizeof(render.palette.modified));

	// Finish this frame using a copy only handler
	RENDER_DrawLine        = finish_line_handler;
	render.scale.out_write = nullptr;

	// Signal the next frame to first reinit the cache
	render.scale.clear_cache = true;
	render.active            = true;
}

static void render_callback(GFX_CallbackFunctions_t function)
{
	if (function == GFX_CallbackStop) {
		halt_render();
		return;

	} else if (function == GFX_CallbackRedraw) {
		render.scale.clear_cache = true;
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
	    image_info.width > ScalerMaxWidth ||
	    image_info.height > ScalerMaxHeight) {
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

static ImageAdjustmentSettings curr_image_adjustment_settings = {};

static void set_image_adjustment_settings()
{
	GFX_GetRenderer()->SetImageAdjustmentSettings(curr_image_adjustment_settings);
}

static CrtColorProfile to_crt_color_profile_enum(const std::string& setting)
{
	using enum CrtColorProfile;

	if (setting == "auto") {
		return Auto;

	} else if (has_false(setting)) {
		return None;

	} else if (setting == "ebu") {
		return Ebu;

	} else if (setting == "p22") {
		return P22;

	} else if (setting == "smpte-c") {
		return SmpteC;

	} else if (setting == "philips") {
		return Philips;

	} else if (setting == "trinitron") {
		return Trinitron;

	} else {
		assertm(false, "Invalid monochrome_palette setting");
		return None;
	}
}

static const char* to_setting_name(const CrtColorProfile profile)
{
	using enum CrtColorProfile;

	switch (profile) {
	case Auto: return "auto";
	case None: return "none";
	case Ebu: return "ebu";
	case P22: return "p22";
	case SmpteC: return "smpte-c";
	case Philips: return "philips";
	case Trinitron: return "trinitron";
	default:
		assertm(false, "Invalid CrtColorProfile enum value");
		return "auto";
	}
}

static const char* to_displayable_name(const CrtColorProfile profile)
{
	using enum CrtColorProfile;

	switch (profile) {
	case Auto: return "auto";
	case None: return "none";
	case Ebu: return "EBU";
	case P22: return "P22";
	case SmpteC: return "SMPTE-C";
	case Philips: return "Philips";
	case Trinitron: return "Trinitron";
	default:
		assertm(false, "Invalid CrtColorProfile enum value");
		return "auto";
	}
}

static void handle_auto_image_adjustment_settings(const VideoMode& video_mode)
{
	const auto maybe_auto_settings =
	        AutoImageAdjustmentsManager::GetInstance().GetSettings(video_mode);

	if (maybe_auto_settings) {
		const auto settings = *maybe_auto_settings;

		if (get_render_section().GetString("crt_color_profile") == "auto") {
			if (curr_image_adjustment_settings.crt_color_profile !=
			    settings.crt_color_profile) {

				curr_image_adjustment_settings.crt_color_profile =
				        settings.crt_color_profile;
				set_image_adjustment_settings();

				if (settings.crt_color_profile ==
				    CrtColorProfile::None) {

					LOG_INFO("RENDER: Disabled CRT color profile emulation");
				} else {
					LOG_INFO("RENDER: Auto-switched to %s CRT color profile",
					         to_displayable_name(
					                 settings.crt_color_profile));
				}
			}
		}

		if (get_render_section().GetStringLowCase("black_level") == "auto") {
			if (curr_image_adjustment_settings.black_level !=
			    settings.black_level) {

				curr_image_adjustment_settings.black_level =
				        settings.black_level;
				set_image_adjustment_settings();

				LOG_INFO("RENDER: Auto-switched to %g black level",
				         settings.black_level);
			}
		}

		if (get_render_section().GetStringLowCase("color_temperature") ==
		    "auto") {
			if (curr_image_adjustment_settings.color_temperature_kelvin !=
			    settings.color_temperature_kelvin) {

				curr_image_adjustment_settings.color_temperature_kelvin =
				        settings.color_temperature_kelvin;
				set_image_adjustment_settings();

				LOG_INFO("RENDER: Auto-switched to %gK colour temperature",
				         settings.color_temperature_kelvin);
			}
		}
	}
}

static bool handle_shader_auto_switching(const VideoMode& video_mode,
                                         const bool reinit_renderer)
{
	const auto renderer = GFX_GetRenderer();

	const auto curr_shader = renderer->GetCurrentShaderInfo();
	const auto curr_preset = renderer->GetCurrentShaderPreset(); //-V821

	renderer->NotifyVideoModeChanged(video_mode);

	const auto new_shader = renderer->GetCurrentShaderInfo();
	const auto new_preset = renderer->GetCurrentShaderPreset(); //-V821

	const auto shader_changed = (curr_shader.name != new_shader.name) ||
	                            (curr_preset.name != new_preset.name);

	if (!shader_changed) {
		return false;
	}

	set_scan_and_pixel_doubling();

	if (reinit_renderer) {
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
	return true;
}

static bool notify_video_mode_changed(const VideoMode& video_mode,
                                      const bool reinit_renderer)
{
	handle_auto_image_adjustment_settings(video_mode);
	return handle_shader_auto_switching(video_mode, reinit_renderer);
}

bool RENDER_NotifyVideoModeChanged(const VideoMode& video_mode)
{
	constexpr auto ReinitRenderer = false;
	return notify_video_mode_changed(video_mode, ReinitRenderer);
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
		constexpr auto ReinitRenderer = true;
		notify_video_mode_changed(video_mode, ReinitRenderer);
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

	case Ok:
		set_scan_and_pixel_doubling();
		handle_auto_image_adjustment_settings(VGA_GetCurrentVideoMode());
		set_image_adjustment_settings();
		return true;

	default: assertm(false, "Invalid SetShaderResult value"); return false;
	}
}

static void set_fallback_shader_or_exit(const std::string& failed_shader_descriptor)
{
	if (failed_shader_descriptor != SymbolicShaderName::AutoGraphicsStandard) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "RENDER_SHADER_FALLBACK",
		                      failed_shader_descriptor.c_str(),
		                      SymbolicShaderName::AutoGraphicsStandard);

		if (set_shader(SymbolicShaderName::AutoGraphicsStandard)) {
			set_section_property_value("render",
			                           "shader",
			                           SymbolicShaderName::AutoGraphicsStandard);
			return;
		}
	}

	if (failed_shader_descriptor != ShaderName::Sharp) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "RENDER_SHADER_FALLBACK",
		                      SymbolicShaderName::AutoGraphicsStandard,
		                      ShaderName::Sharp);

		if (set_shader(ShaderName::Sharp)) {
			set_section_property_value("render", "shader", ShaderName::Sharp);
			return;
		}
	}

	E_Exit("RENDER: Error setting fallback shaders, exiting");
}

static void reload_shader([[maybe_unused]] const bool pressed)
{
	if (!pressed) {
		return;
	}

	GFX_GetRenderer()->ForceReloadCurrentShader();

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

static void set_aspect_ratio_correction(const SectionProp& section)
{
	render.aspect_ratio_correction_mode = [&]() {
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
	}();
}

AspectRatioCorrectionMode RENDER_GetAspectRatioCorrectionMode()
{
	return render.aspect_ratio_correction_mode;
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

		viewport.mode = ViewportMode::Fit;
		return viewport;

	} else if (const auto width_and_height = parse_int_dimensions(pref)) {
		const auto [w, h] = *width_and_height;

		const auto desktop = GFX_GetDesktopSize();

		const bool is_out_of_bounds = (w <= 0 ||
		                               static_cast<float>(w) > desktop.w ||
		                               h <= 0 ||
		                               static_cast<float>(h) > desktop.h);
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

static void set_viewport(SectionProp& section)
{
	if (const auto& settings = parse_viewport_settings(
	            section.GetStringLowCase("viewport"));
	    settings) {
		render.viewport_settings = *settings;
	} else {
		render.viewport_settings = {};
		set_section_property_value("render", "viewport", "fit");
	}
}

static void set_integer_scaling(const SectionProp& section)
{
	using enum IntegerScalingMode;

	render.integer_scaling_mode = [&]() {
		const std::string mode = section.GetString("integer_scaling");

		if (has_false(mode)) {
			return Off;

		} else if (mode == "auto") {
			return Auto;

		} else if (mode == "horizontal") {
			return Horizontal;

		} else if (mode == "vertical") {
			return Vertical;

		} else {
			constexpr auto SettingName  = "integer_scaling";
			constexpr auto DefaultValue = "auto";

			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "RENDER",
			                      "PROGRAM_CONFIG_INVALID_SETTING",
			                      SettingName,
			                      mode.c_str(),
			                      DefaultValue);

			return Auto;
		}
	}();
}

static void set_deinterlacing(const SectionProp& section)
{
	using enum DeinterlacingStrength;

	render.deinterlacing_strength = [&]() {
		const std::string pref = section.GetStringLowCase("deinterlacing");

		if (has_false(pref)) {
			return Off;

		} else if (has_true(pref)) {
			return Medium;

		} else if (pref == "light") {
			return Light;

		} else if (pref == "medium") {
			return Medium;

		} else if (pref == "strong") {
			return Strong;

		} else if (pref == "full") {
			return Full;

		} else {
			constexpr auto SettingName  = "deinterlacing";
			constexpr auto DefaultValue = "off";

			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "RENDER",
			                      "PROGRAM_CONFIG_INVALID_SETTING",
			                      SettingName,
			                      pref.c_str(),
			                      DefaultValue);

			return Off;
		}
	}();
}

DosBox::Rect RENDER_CalcRestrictedViewportSizeInPixels(const DosBox::Rect& canvas_size_px)
{
	const auto dpi_scale = GFX_GetDpiScaleFactor();

	switch (render.viewport_settings.mode) {
	case ViewportMode::Fit: {
		auto viewport_size_px = [&] {
			if (render.viewport_settings.fit.limit_size) {
				return render.viewport_settings.fit.limit_size
				        ->Copy()
				        .ScaleSize(dpi_scale);

			} else if (render.viewport_settings.fit.desktop_scale) {
				auto desktop_size_px = GFX_GetDesktopSize().ScaleSize(
				        dpi_scale);

				return desktop_size_px.ScaleSize(
				        *render.viewport_settings.fit.desktop_scale);
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
		        .ScaleWidth(render.viewport_settings.relative.width_scale)
		        .ScaleHeight(render.viewport_settings.relative.height_scale);
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
			        .ScaleSize(static_cast<float>(integer_scale_factor))
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
		// The half-steps and no scaling above 5.0x result in no
		// moire artifacts in 99% of cases, so it's very much
		// worth it for better viewport utilisation.
		//
		if (GFX_GetRenderBackendType() != RenderBackendType::OpenGl) {
			return draw_size_fit_px;
		}

		if (GFX_GetRenderer()->GetCurrentShaderInfo().is_adaptive) {
			auto integer_scale_factor = [&] {
				const auto factor = draw_size_fit_px.h /
				                    render_size_px.h;
				if (factor >= 5.0) {
					// Disable integer scaling
					// above 5.0x vertical scaling
					return factor;
				}
				if (factor >= 3.0) {
					// Allow 3.5x and 4.5x half
					// steps in the 3.0x to 5.0x
					// vertical scaling range
					return floorf(factor * 2) / 2;
				}
				// Allow only integer steps below 3.0x
				// vertical scaling
				return floorf(factor);
			}();

			return calc_vert_integer_scaling_dims_in_pixels(
			        integer_scale_factor);
		}

		// Handles the `sharp` shader fallback when the viewport
		// is is too small for CRT shaders; integer scaling is
		// then disabled.
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

static void init_color_space_setting(SectionProp& section)
{
	using enum Property::Changeable::Value;

#if defined(MACOSX)
	constexpr auto DefaultColorSpace = "display-p3";
#else
	constexpr auto DefaultColorSpace = "srgb";
#endif

	auto string_prop = section.AddString("color_space", Always, DefaultColorSpace);
	string_prop->SetValues(
#if defined(MACOSX)
	        {"display-p3"}
#else
	        {"srgb", "display-p3", "dci-p3", "dci-p3-d65", "modern-p3", "adobe-rgb", "rec-2020"}
#endif
	);

	string_prop->SetOptionHelp("color_space_description",
	                           format_str("Set the colour space of the video output ('%s' by default). This setting\n"
	                                      "allows to take advantage of wide color gamut monitors and to more accurately\n"
	                                      "emulate CRT colors. Possible values:",
	                                      DefaultColorSpace));

	string_prop->SetOptionHelp("color_space_description_macos",
	                           "Set the colour space of the video output. On macOS, this is always 'display-p3';\n"
	                           "the OS performs the conversion to the colour profile set in your system\n"
	                           "settings.");

	string_prop->SetOptionHelp(
	        "color_space_srgb",
	        "\n"
	        "  srgb:        The lowest common denominator non-wide gamut sRGB colour space\n"
	        "               with 6500K white point and sRGB gamma (default).");

	string_prop->SetOptionHelp("color_space_display_p3",
	                           "\n"
	                           "  display-p3:  Display P3 wide gamut colour space with 6500K white point and\n"
	                           "               sRGB gamma.");

	string_prop->SetOptionHelp(
	        "color_space_rest",
	        "\n"
	        "  dci-p3:      Standard DCI-P3 wide gamut colour space with DCI white point\n"
	        "               (~6300K) and a 2.6 gamma. Use 'dci-p3-d65' instead if the whites\n"
	        "               and grays have a greenish tint with your monitor in DCI-P3 mode.\n"
	        "\n"
	        "  dci-p3-d65:  DCI-P3 variant with modified D65 white point (6500K) and 2.6\n"
	        "               gamma. Use 'dci-p3' instead if the whites and grays have a\n"
	        "               yellowish tint with your monitor in DCI-P3 mode.\n"
	        "\n"
	        "  modern-p3:   Setting for average consumer/gaming monitors that only reach\n"
	        "               around 90%% DCI-P3 colour space gamut coverage (6500K white\n"
	        "               point, sRGB gamma). Use the other DCI-P3 colour spaces if your\n"
	        "               monitor's DCI-P3 coverage is close to 100%%.\n"
	        "\n"
	        "  adobe-rgb:   AdobeRGB 2020 wide gamut colour space with 6500K white point\n"
	        "               and 2.2 gamma.\n"
	        "\n"
	        "  rec-2020:    Rec.2020 wide gamut colour space with 6500K white point and 2.2\n"
	        "               gamma.");

	string_prop->SetOptionHelp(
	        "color_space_notes",
	        "\n"
	        "Notes:\n"
	        "  - Colour space transforms are applied to rendered screenshots, but not to raw\n"
	        "    and upscaled screenshots and video captures (those are always in sRGB).");

	string_prop->SetOptionHelp(
	        "color_space_notes_windows_linux",
	        "\n"
	        "  - The feature only works in OpenGL output mode.\n"
	        "\n"
	        "  - The setting must match the colour space set on your monitor.\n"
	        "\n"
	        "  - You must disable all OS and graphics driver level colour management, and you\n"
	        "    must not use any 3rd party colour management programs for DOSBox Staging,\n"
	        "    otherwise you'll get incorrect colours.");

	string_prop->SetEnabledOptions({
#if defined(MACOSX)
	        "color_space_description_macos", "color_space_display_p3", "color_space_notes"
#else
	        "color_space_description",
	        "color_space_srgb",
	        "color_space_display_p3",
	        "color_space_rest",
	        "color_space_notes",
	        "color_space_notes_windows_linux"
#endif
	});
}

std::string RENDER_GetCgaColorsSetting()
{
	return get_render_section().GetStringLowCase("cga_colors");
}

constexpr int BrightnessMin = 0;
constexpr int BrightnessMax = 100;

constexpr int ContrastMin = 0;
constexpr int ContrastMax = 100;

constexpr int GammaMin = -50;
constexpr int GammaMax = 50;

constexpr int DigitalContrastMin = -50;
constexpr int DigitalContrastMax = 50;

constexpr int BlackLevelMin = 0;
constexpr int BlackLevelMax = 100;

constexpr int SaturationMin = -50;
constexpr int SaturationMax = 50;

constexpr int ColorTemperatureNeutral = 6500;
constexpr int ColorTemperatureMin     = 3000;
constexpr int ColorTemperatureMax     = 10000;

constexpr int ColorTemperatureLumaPreserveMin = 0;
constexpr int ColorTemperatureLumaPreserveMax = 100;

constexpr int RgbGainMin = 0;
constexpr int RgbGainMax = 200;

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
	        "  jinc2:     Upscale the image using jinc 2-lobe interpolation with anti-ringing.\n"
	        "             This blends together dithered color patterns at the cost of image\n"
	        "             sharpness.\n"
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
	string_prop->SetValues({"auto", "on", "square-pixels", "off", "stretch"});
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

	string_prop = section.AddString("integer_scaling", Always, "auto");
	string_prop->SetValues({"auto", "vertical", "horizontal", "off"});
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
	        "\n"
	        "  - Use the 'Stretch Axis', 'Inc Stretch', and 'Dec Stretch' hotkey actions to\n"
	        "    adjust the image size in 'relative' mode in real-time, then copy the new\n"
	        "    settings from the logs into your config.");

	string_prop = section.AddString("monochrome_palette",
	                                Always,
	                                MonochromePaletteAmber);
	string_prop->SetValues({MonochromePaletteAmber,
	                        MonochromePaletteGreen,
	                        MonochromePaletteWhite,
	                        MonochromePalettePaperwhite});
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

	string_prop = section.AddString("cga_colors", OnlyAtStart, "default");
	string_prop->SetHelp(
	        "Set the interpretation of CGA RGBI colours ('default' by default). Affects all\n"
	        "machine types capable of displaying CGA or better graphics, including the PCjr,\n"
	        "the Tandy, and CGA/EGA modes on VGA adapters. Note these colours will be further\n"
	        "adjusted by the video output settings (see 'crt_color_profile', 'brightness',\n"
	        "'saturation', etc.). Built-in presets:\n"
	        "\n"
	        "  default:       The canonical CGA palette, as emulated by VGA adapters\n"
	        "                 (default).\n"
	        "\n"
	        "  tandy <bl>:    Emulation of an idealised Tandy monitor with adjustable brown\n"
	        "                 level. The brown level can be provided as an optional second\n"
	        "                 parameter (0 - red, 50 - brown, 100 - dark yellow;\n"
	        "                 defaults to 50). E.g., tandy 100\n"
	        "\n"
	        "  tandy-warm:    Emulation of the actual colour output of an unknown Tandy\n"
	        "                 monitor. Intended to be used with 'crt_color_profile = none'\n"
	        "                 and 'color_temperature = 6500'.\n"
	        "\n"
	        "  ibm5153 <c>:   Emulation of the actual colour output of an IBM 5153 monitor\n"
	        "                 with a unique contrast control that dims non-bright colours\n"
	        "                 only. The contrast can be optionally provided as a second\n"
	        "                 parameter (0 to 100; defaults to 100), e.g., ibm5153 60.\n"
	        "                 Intended to be used with 'crt_color_profile = none' and\n"
	        "                 'color_temperature = 6500'.\n"
	        "\n"
	        "  agi-amiga-v1, agi-amiga-v2, agi-amiga-v3:\n"
	        "                 Palettes used by the Amiga ports of Sierra AGI games.\n"
	        "\n"
	        "  agi-amigaish:  A mix of EGA and Amiga colours used by the Sarien\n"
	        "                 AGI-interpreter.\n"
	        "\n"
	        "  scumm-amiga:   Palette used by the Amiga ports of LucasArts EGA games.\n"
	        "\n"
	        "  colodore:      Commodore 64 inspired colours based on the Colodore palette.\n"
	        "\n"
	        "  colodore-sat:  Colodore palette with 20%% more saturation.\n"
	        "\n"
	        "  dga16:         A modern take on the canonical CGA palette with dialed back\n"
	        "                 contrast.\n"
	        "\n"
	        "You can also set custom colours by specifying 16 space or comma separated\n"
	        "sRGB colour values, either as 3 or 6-digit hex codes (e.g., #f00 or #ff0000 for\n"
	        "full red), or decimal RGB triplets (e.g., (255, 0, 255) for magenta). The 16\n"
	        "colours are ordered as follows:\n"
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
	        "\n"
	        "  - If you used the normal2x/3x scalers, consider using [color=light-green]'integer_scaling'[reset]\n"
	        "    with [color=light-green]'shader = sharp'[reset] and optionally setting the desired [color=light-green]'window_size'[reset]\n"
	        "    or [color=light-green]'viewport'[reset] size.\n"
	        "\n"
	        "  - If you used an advanced scaler, consider one of the [color=light-green]'shader'[reset] options.");

	init_color_space_setting(section);

	auto bool_prop = section.AddBool("image_adjustments", WhenIdle, true);
	bool_prop->SetHelp(
	        "Enable image adjustments ('on' by default). When disabled, the image adjustment\n"
	        "settings in the render section (e.g., 'crt_color_profile', 'brightness',\n"
	        "'contrast', etc.) have no effect and the raw RGB values are used for the video\n"
	        "output. The colour space conversion is always active, that cannot be disabled\n"
	        "(see 'color_space').\n"
	        "\n"
	        "Notes:\n"
	        "  - Image adjustments only work in OpenGL output mode.\n"
	        "\n"
	        "  - Adjustments are applied to rendered screenshots, but not to raw and upscaled\n"
	        "    screenshots and video captures.\n"
	        "\n"
	        "  - Use the 'PrevImageAdj' and 'NextImageAdj' hotkeys to select an image\n"
	        "    adjustment setting and the 'DecImageAdj' and 'IncImageAdj' hotkeys to adjust\n"
	        "    the settings in real-time. Copy the new settings from the logs into your\n"
	        "    config, or write a new config with the 'CONFIG -wc' command.");

	string_prop = section.AddString("crt_color_profile", Always, "auto");
	string_prop->SetValues(
	        {"auto", "none", "ebu", "p22", "smpte-c", "philips", "trinitron"});
	string_prop->SetHelp(
	        "Set a CRT colour profile for more authentic video output emulation ('auto' by\n"
	        "default). All profiles have a built-in colour temperature (white point) that you\n"
	        "can tweak further with the 'color_temperature' setting. Possible values:\n"
	        "\n"
	        "  auto:       Select an authentic colour profile for adaptive CRT shaders;\n"
	        "              for any other shader, use 'none' (default).\n"
	        "\n"
	        "  none:       Display raw colours without any colour profile transforms.\n"
	        "\n"
	        "  ebu:        EBU standard phosphor emulation, used in high-end professional CRT\n"
	        "              monitors, such as the Sony BVM/PVM series (6500K white point).\n"
	        "\n"
	        "  p22:        P22 phosphor emulation, the most commonly used in lower-end CRT\n"
	        "              monitors (6500K white point).\n"
	        "\n"
	        "  smpte-c:    SMPT \"C\" phosphor emulation, the standard for American broadcast\n"
	        "              video monitors (6500K white point).\n"
	        "\n"
	        "  philips:    Philips CRT monitor colours typical to 15 kHz home computer\n"
	        "              monitors, such as the Commodore 1084S (~6100K white point).\n"
	        "              Needs a wide gamut DCI-P3 display for the best results.\n"
	        "\n"
	        "  trinitron:  Typical Sony Trinitron CRT TV and monitor colours (~9300K\n"
	        "              white point). Needs a wide gamut DCI-P3 display for the best\n"
	        "              results.");

	constexpr int DefaultBrightness = 45;
	int_prop = section.AddInt("brightness", Always, DefaultBrightness);
	int_prop->SetMinMax(BrightnessMin, BrightnessMax);
	int_prop->SetHelp(
	        format_str("Set the brightness of the video output (%d by default). Valid range is %d to %d.\n"
	                   "This emulates the brightness control of CRT monitors that sets the black point;\n"
	                   "higher values will result in raised blacks.",
	                   DefaultBrightness,
	                   BrightnessMin,
	                   BrightnessMax));

	constexpr int DefaultContrast = 65;
	int_prop = section.AddInt("contrast", Always, DefaultContrast);
	int_prop->SetMinMax(ContrastMin, ContrastMax);
	int_prop->SetHelp(
	        format_str("Set the contrast of the video output (%d by default). Valid range is %d to %d.\n"
	                   "This emulates the contrast control of CRT monitors that sets the white point;\n"
	                   "higher values will result in raised blacks (lower the 'brightness' control to\n"
	                   "compensate).",
	                   DefaultContrast,
	                   ContrastMin,
	                   ContrastMax));

	constexpr int DefaultGamma = 0;
	int_prop = section.AddInt("gamma", Always, DefaultGamma);
	int_prop->SetMinMax(GammaMin, GammaMax);
	int_prop->SetHelp(
	        format_str("Set the gamma of the video output (%d by default). Valid range is %d to %d.\n"
	                   "This is additional gamma adjustment relative to the emulated virtual monitor's\n"
	                   "gamma.",
	                   DefaultGamma,
	                   GammaMin,
	                   GammaMax));

	constexpr int DefaultDigitalContrast = 0;
	int_prop = section.AddInt("digital_contrast", Always, DefaultDigitalContrast);
	int_prop->SetMinMax(DigitalContrastMin, DigitalContrastMax);
	int_prop->SetHelp(format_str(
	        "Set the digital contrast of the video output (%d by default). Valid range is %d\n"
	        "to %d. This works very differently from the 'contrast' virtual monitor setting;\n"
	        "digital contrast is applied to the raw RGB values of the framebuffer image.",
	        DefaultDigitalContrast,
	        DigitalContrastMin,
	        DigitalContrastMax));

	constexpr auto DefaultBlackLevel = "auto";
	string_prop = section.AddString("black_level", Always, DefaultBlackLevel);
	string_prop->SetHelp(format_str(
	        "Raise the black level of the video output ('%s' by default). It is applied\n"
	        "before the 'brightness' and 'contrast' settings which can also raise the black\n"
	        "level, so it effectively acts as a black level boost. Possible values:\n"
	        "\n"
	        "  auto:      Raise the black level for PCjr, Tandy, CGA and EGA video modes only\n"
	        "             for adaptive CRT shaders; for any other shader, use 0 (default).\n"
	        "\n"
	        "  <number>:  Set the black level raise amount. Valid range is %d to %d.\n"
	        "             0 does not raise the black level.\n"
	        "\n"
	        "Note: Raising the black level if useful for \"black scanline\" emulation; this\n"
	        "      adds visual interest to PCjr, Tandy, CGA, and EGA games with simple\n"
	        "      graphics.",
	        DefaultBlackLevel,
	        BlackLevelMin,
	        BlackLevelMax));

	constexpr int DefaultSaturation = 0;
	int_prop = section.AddInt("saturation", Always, DefaultSaturation);
	int_prop->SetMinMax(SaturationMin, SaturationMax);
	int_prop->SetHelp(
	        format_str("Set the saturation of the video output (%d by default). Valid range is %d to %d.\n"
	                   "This is digital saturation applied to the raw RGB values of framebuffer image,\n"
	                   "similarly to 'digital_contrast'.",
	                   DefaultSaturation,
	                   SaturationMin,
	                   SaturationMax));

	constexpr auto DefaultColorTemperature = "auto";
	string_prop = section.AddString("color_temperature",
	                                Always,
	                                DefaultColorTemperature);
	string_prop->SetHelp(format_str(
	        "Set the colour temperature (white point) of the video output ('%s' by\n"
	        "default). Possible values:\n"
	        "\n"
	        "  auto:      Select an authentic colour temperature for adaptive CRT shaders;\n"
	        "             for any other shader, use 6500 (default).\n"
	        "\n"
	        "  <number>:  Specify colour temperature in Kelvin (K). Valid range is %d to\n"
	        "             %d. The Kelvin value only makes sense if 'crt_color_profile' is\n"
	        "             set to 'none' or to one of the profiles with 6500K white point,\n"
	        "             otherwise it acts as a relative colour temperature adjustment (less\n"
	        "             then 6500 results in warmer colours, more than 6500 in cooler\n"
	        "             colours).",
	        DefaultColorTemperature,
	        ColorTemperatureMin,
	        ColorTemperatureMax));

	constexpr auto DefaultColorTemperatureLumaPreserve = 0;
	int_prop = section.AddInt("color_temperature_luma_preserve",
	                          Always,
	                          DefaultColorTemperatureLumaPreserve);
	int_prop->SetMinMax(ColorTemperatureLumaPreserveMin,
	                    ColorTemperatureLumaPreserveMax);
	int_prop->SetHelp(format_str(
	        "Preserve image luminosity prior to colour temperature adjustment (%d by\n"
	        "default). Valid range is %d to %d. 0 doesn't perform any luminosity\n"
	        "preservation, 100 fully preserves the luminosity. Values greater than 0 result\n"
	        "in inaccurate colour temperatures in the brighter shades, so it's best to set\n"
	        "this to 0 or close to 0 if your monitor is bright enough.",
	        DefaultColorTemperatureLumaPreserve,
	        ColorTemperatureLumaPreserveMin,
	        ColorTemperatureLumaPreserveMax));

	constexpr int DefaultRgbGain = 100;
	int_prop = section.AddInt("red_gain", Always, DefaultRgbGain);
	int_prop->SetMinMax(RgbGainMin, RgbGainMax);
	int_prop->SetHelp(
	        format_str("Set gain factor of the video output's red channel (%d by default). Valid range\n"
	                   "is %d to %d. 100 results in no gain change.",
	                   DefaultRgbGain,
	                   RgbGainMin,
	                   RgbGainMax));

	int_prop = section.AddInt("green_gain", Always, DefaultRgbGain);
	int_prop->SetMinMax(RgbGainMin, RgbGainMax);
	int_prop->SetHelp(
	        format_str("Set gain factor of the video output's green channel (%d by default). Valid\n"
	                   "range is %d to %d. 100 results in no gain change.",
	                   DefaultRgbGain,
	                   RgbGainMin,
	                   RgbGainMax));

	int_prop = section.AddInt("blue_gain", Always, DefaultRgbGain);
	int_prop->SetMinMax(RgbGainMin, RgbGainMax);
	int_prop->SetHelp(
	        format_str("Set gain factor of the video output's blue channel (%d by default). Valid range\n"
	                   "is %d to %d. 100 results in no gain change.",
	                   DefaultRgbGain,
	                   RgbGainMin,
	                   RgbGainMax));

	string_prop = section.AddString("deinterlacing", Always, "off");
	string_prop->SetValues({"on", "off", "light", "medium", "strong", "full"});
	string_prop->SetHelp(
	        "Remove black lines from interlaced videos ('off' by default). Use with games\n"
	        "that display video content with alternating black lines. This trick worked well\n"
	        "on CRT monitors to increase perceptual resolution while saving storage space,\n"
	        "but it resulted in brightness-loss. Possible values:\n"
	        "\n"
	        "  off:     Disable deinterlacing (default).\n"
	        "\n"
	        "  on:      Enable deinterlacing at 'medium' strength.\n"
	        "\n"
	        "  light:   Light deinterlacing. Black scanlines are softened to mimic the\n"
	        "           CRT look.\n"
	        "\n"
	        "  medium:  Medium deinterlacing. Best balance between removing black lines,\n"
	        "           increasing brightness, and keeping the higher resolution look.\n"
	        "\n"
	        "  strong:  Strong deinterlacing. Image brightness is almost completely\n"
	        "           restored at the expense of diminishing the higher resolution look.\n"
	        "\n"
	        "  full:    Full deinterlacing. Completely removes black lines and maximises\n"
	        "           brightness, but the image will appear blockier.\n"
	        "\n"
	        "Note: Enabling vertical 'integer_scaling' is recommended on lower resolution\n"
	        "      displays to avoid interference artifacts when using lower deinterlacing\n"
	        "      strengths. Alternatively, use 'full' strength to completely eliminate all\n"
	        "      potential interference patterns.");
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
	if (render.viewport_settings.mode != ViewportMode::Relative) {
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
	if (render.viewport_settings.mode != ViewportMode::Relative) {
		log_stretch_hotkeys_viewport_mode_warning();
		return;
	}

	auto& r = render.viewport_settings.relative;

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

static ColorSpace to_color_space_enum(const std::string& setting)
{
	using enum ColorSpace;

	if (setting == "srgb") {
		return Srgb;

	} else if (setting == "dci-p3") {
		return DciP3;

	} else if (setting == "dci-p3-d65") {
		return DciP3_D65;

	} else if (setting == "display-p3") {
		return DisplayP3;

	} else if (setting == "modern-p3") {
		return ModernP3;

	} else if (setting == "adobe-rgb") {
		return AdobeRgb;

	} else if (setting == "rec-2020") {
		return Rec2020;

	} else {
		assertm(false, "Invalid color_space setting");
		return Srgb;
	}
}

[[maybe_unused]] static const char* to_setting_name(const ColorSpace color_space)
{
	using enum ColorSpace;

	switch (color_space) {
	case Srgb: return "srgb";
	case DciP3: return "dci-p3";
	case DciP3_D65: return "dci-p3-d65";
	case DisplayP3: return "display-p3";
	case ModernP3: return "modern-p3";
	case AdobeRgb: return "adobe-rgb";
	case Rec2020: return "rec-2020";

	default: assertm(false, "Invalid ColorSpace enum value"); return "srgb";
	}
}

static void update_color_space_setting()
{
	const auto color_space = to_color_space_enum(
	        get_render_section().GetString("color_space"));

	GFX_GetRenderer()->SetColorSpace(color_space);
}

static void update_enable_image_adjustments_setting()
{
	GFX_GetRenderer()->EnableImageAdjustments(
	        get_render_section().GetBool("image_adjustments"));
}

static void update_crt_color_profile_setting()
{
	curr_image_adjustment_settings.crt_color_profile = to_crt_color_profile_enum(
	        get_render_section().GetString("crt_color_profile"));
}

static void update_brightness_setting()
{
	curr_image_adjustment_settings.brightness = remap(
	        static_cast<float>(BrightnessMin),
	        static_cast<float>(BrightnessMax),
	        0.0f,
	        100.0f,
	        static_cast<float>(get_render_section().GetInt("brightness")));
}

static void update_contrast_setting()
{
	curr_image_adjustment_settings.contrast =
	        remap(static_cast<float>(ContrastMin),
	              static_cast<float>(ContrastMax),
	              0.0f,
	              100.0f,
	              static_cast<float>(get_render_section().GetInt("contrast")));
}

static void update_gamma_setting()
{
	curr_image_adjustment_settings.gamma =
	        remap(static_cast<float>(GammaMin),
	              static_cast<float>(GammaMax),
	              -1.0f,
	              1.0f,
	              static_cast<float>(get_render_section().GetInt("gamma")));
}

static void update_digital_contrast_setting()
{
	curr_image_adjustment_settings.digital_contrast = remap(
	        static_cast<float>(DigitalContrastMin),
	        static_cast<float>(DigitalContrastMax),
	        -2.0f,
	        2.0f,
	        static_cast<float>(get_render_section().GetInt("digital_contrast")));
}

static void update_black_level_color_setting()
{
	curr_image_adjustment_settings.black_level_color = VGA_GetBlackLevelColor();
}

static std::optional<int> get_black_level_setting_value()
{
	constexpr auto SettingName  = "black_level";
	constexpr auto DefaultValue = "auto";

	const std::string pref = get_render_section().GetString(SettingName);
	if (pref == "auto") {
		return {};
	}

	if (const auto maybe_int = parse_int(pref); maybe_int) {
		const auto black_level = *maybe_int;

		if (black_level >= BlackLevelMin && black_level <= BlackLevelMax) {
			return black_level;

		} else {
			NOTIFY_DisplayWarning(
			        Notification::Source::Console,
			        "RENDER",
			        "PROGRAM_CONFIG_SETTING_OUTSIDE_VALID_RANGE",
			        SettingName,
			        format_str("%d", black_level).c_str(),
			        format_str("%d", BlackLevelMin).c_str(),
			        format_str("%d", BlackLevelMax).c_str(),
			        DefaultValue);

			set_section_property_value("render", SettingName, DefaultValue);
			return {};
		}
	} else {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      SettingName,
		                      pref.c_str(),
		                      DefaultValue);

		set_section_property_value("render", SettingName, DefaultValue);
		return {};
	}
}

static void update_black_level_setting()
{
	if (const auto maybe_black_level = get_black_level_setting_value();
	    maybe_black_level) {

		curr_image_adjustment_settings.black_level =
		        remap(static_cast<float>(BlackLevelMin),
		              static_cast<float>(BlackLevelMax),
		              0.0f,
		              1.0f,
		              static_cast<float>(*maybe_black_level));
	} else {
		curr_image_adjustment_settings.black_level = BlackLevelMin;
	}
}

static void update_saturation_setting()
{
	curr_image_adjustment_settings.saturation = remap(
	        static_cast<float>(SaturationMin),
	        static_cast<float>(SaturationMax),
	        -1.0f,
	        1.0f,
	        static_cast<float>(get_render_section().GetInt("saturation")));
}

static std::optional<int> get_color_temperature_setting_value()
{
	constexpr auto SettingName  = "color_temperature";
	constexpr auto DefaultValue = "auto";

	const std::string pref = get_render_section().GetString(SettingName);
	if (pref == "auto") {
		return {};
	}

	if (const auto maybe_int = parse_int(pref); maybe_int) {
		const auto color_temperature = *maybe_int;

		if (color_temperature >= ColorTemperatureMin &&
		    color_temperature <= ColorTemperatureMax) {

			return color_temperature;

		} else {
			NOTIFY_DisplayWarning(
			        Notification::Source::Console,
			        "RENDER",
			        "PROGRAM_CONFIG_SETTING_OUTSIDE_VALID_RANGE",
			        SettingName,
			        format_str("%d", color_temperature).c_str(),
			        format_str("%d", ColorTemperatureMin).c_str(),
			        format_str("%d", ColorTemperatureMax).c_str(),
			        DefaultValue);

			set_section_property_value("render", SettingName, DefaultValue);
			return {};
		}
	} else {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      SettingName,
		                      pref.c_str(),
		                      DefaultValue);

		set_section_property_value("render", SettingName, DefaultValue);
		return {};
	}
}

static void update_color_temperature_setting()
{
	curr_image_adjustment_settings.color_temperature_kelvin = static_cast<float>(
	        get_color_temperature_setting_value().value_or(ColorTemperatureNeutral));
}

static void update_color_temperature_luma_preserve_setting()
{
	curr_image_adjustment_settings.color_temperature_luma_preserve =
	        remap(static_cast<float>(ColorTemperatureLumaPreserveMin),
	              static_cast<float>(ColorTemperatureLumaPreserveMax),
	              0.0f,
	              1.0f,
	              static_cast<float>(get_render_section().GetInt(
	                      "color_temperature_luma_preserve")));
}

static void update_red_gain_setting()
{
	curr_image_adjustment_settings.red_gain =
	        remap(static_cast<float>(RgbGainMin),
	              static_cast<float>(RgbGainMax),
	              0.0f,
	              2.0f,
	              static_cast<float>(get_render_section().GetInt("red_gain")));
}

static void update_green_gain_setting()
{
	curr_image_adjustment_settings.green_gain = remap(
	        static_cast<float>(RgbGainMin),
	        static_cast<float>(RgbGainMax),
	        0.0f,
	        2.0f,
	        static_cast<float>(get_render_section().GetInt("green_gain")));
}

static void update_blue_gain_setting()
{
	curr_image_adjustment_settings.blue_gain = remap(
	        static_cast<float>(RgbGainMin),
	        static_cast<float>(RgbGainMax),
	        0.0f,
	        2.0f,
	        static_cast<float>(get_render_section().GetInt("blue_gain")));
}

enum class ImageAdjustmentControl {
	ColorSpace = 0,

	CrtColorProfile,
	Brightness,
	Contrast,
	Gamma,
	DigitalContrast,
	BlackLevel,
	Saturation,

	ColorTemperature,
	ColorTemperatureLumaPreserve,

	RedGain,
	GreenGain,
	BlueGain,
};

enum class Direction { Dec, Inc };

static ImageAdjustmentControl curr_image_adjustment_control = ImageAdjustmentControl::ColorSpace;

template <typename T>
T adjust_enum(const T curr_val, const Direction dir, const T min_val, const T max_val)
{
	const auto curr_int = enum_val(curr_val);
	const auto min_int  = enum_val(min_val);
	const auto max_int  = enum_val(max_val);

	int new_int = wrap(curr_int + ((dir == Direction::Dec) ? -1 : 1),
	                   min_int,
	                   max_int);

	return static_cast<T>(new_int);
}

static void select_image_adjustment_setting_control(const Direction dir)
{
	using enum ImageAdjustmentControl;

	const auto min_val = ColorSpace;
	const auto max_val = BlueGain;

	curr_image_adjustment_control =
	        adjust_enum(curr_image_adjustment_control, dir, min_val, max_val);

	const auto name = [] {
		switch (curr_image_adjustment_control) {
		case ColorSpace: return "color space"; break;
		case CrtColorProfile: return "CRT color profile"; break;

		case Brightness: return "brightness"; break;
		case Contrast: return "contrast"; break;
		case Gamma: return "gamma"; break;
		case DigitalContrast: return "digital contrast"; break;
		case BlackLevel: return "black level"; break;
		case Saturation: return "saturation"; break;

		case ColorTemperature: return "colour temperature"; break;
		case ColorTemperatureLumaPreserve:
			return "colour temperature luma preserve";
			break;

		case RedGain: return "red gain"; break;
		case GreenGain: return "green gain"; break;
		case BlueGain: return "blue gain"; break;

		default:
			assertm(false, "Invalid ImageAdjustmentControl value");
			return "";
		}
	}();

	LOG_INFO("RENDER: Selected %s video setting", name);
}

static void select_prev_image_adjustment_control(const bool pressed)
{
	if (pressed) {
		select_image_adjustment_setting_control(Direction::Dec);
	}
}

static void select_next_image_adjustment_control(const bool pressed)
{
	if (pressed) {
		select_image_adjustment_setting_control(Direction::Inc);
	}
}

static void adjust_image_setting(const Direction dir)
{
	auto set_setting = [](const char* setting_name, const std::string& new_value) {
		set_section_property_value("render", setting_name, new_value);

		LOG_INFO("RENDER: %s = %s", setting_name, new_value.c_str());
	};

	auto adjust_setting = [&](const char* setting_name,
	                          const int minval,
	                          const int maxval,
	                          const int delta) {
		const auto curr_value = get_render_section().GetInt(setting_name);
		const auto new_value = clamp(curr_value + delta, minval, maxval);

		set_setting(setting_name, format_str("%d", new_value));
	};

	using enum ImageAdjustmentControl;

	switch (curr_image_adjustment_control) {
	case ColorSpace: {
		// Only `srgb` is supported on macOS, so we only allow
		// cycling through the other color space settings on
		// Windows and Linux.
		//
#if !defined(MACOSX)
		const auto setting_name = "color_space";

		const auto curr_color_space = to_color_space_enum(
		        get_render_section().GetString(setting_name));

		const auto min_val = ColorSpace::Srgb;
		const auto max_val = ColorSpace::Rec2020;

		const auto new_color_space =
		        adjust_enum(curr_color_space, dir, min_val, max_val);

		set_setting(setting_name, to_setting_name(new_color_space));
		update_color_space_setting();
#endif
	} break;

	case CrtColorProfile: {
		const auto setting_name = "crt_color_profile";

		const auto curr_profile = to_crt_color_profile_enum(
		        get_render_section().GetString(setting_name));

		const auto min_val = CrtColorProfile::Auto;
		const auto max_val = CrtColorProfile::Trinitron;

		const auto new_profile = adjust_enum(curr_profile, dir, min_val, max_val);

		set_setting(setting_name, to_setting_name(new_profile));

		if (new_profile == CrtColorProfile::Auto) {
			handle_auto_image_adjustment_settings(
			        VGA_GetCurrentVideoMode());
		} else {
			update_crt_color_profile_setting();
		}
		set_image_adjustment_settings();
	} break;

	case Brightness:
		adjust_setting("brightness",
		               BrightnessMin,
		               BrightnessMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_brightness_setting();
		set_image_adjustment_settings();
		break;

	case Contrast:
		adjust_setting("contrast",
		               ContrastMin,
		               ContrastMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_contrast_setting();
		set_image_adjustment_settings();
		break;

	case Gamma:
		adjust_setting("gamma",
		               GammaMin,
		               GammaMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_gamma_setting();
		set_image_adjustment_settings();
		break;

	case DigitalContrast:
		adjust_setting("digital_contrast",
		               DigitalContrastMin,
		               DigitalContrastMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_digital_contrast_setting();
		set_image_adjustment_settings();
		break;

	case BlackLevel: {
		const auto curr_value = get_black_level_setting_value().value_or(
		        BlackLevelMin);

		const auto delta     = (dir == Direction::Dec) ? -1 : 1;
		const auto new_value = clamp(curr_value + delta,
		                             BlackLevelMin,
		                             BlackLevelMax);

		set_setting("black_level", format_str("%d", new_value));

		update_black_level_setting();
		handle_auto_image_adjustment_settings(VGA_GetCurrentVideoMode());
		set_image_adjustment_settings();
	} break;

	case Saturation:
		adjust_setting("saturation",
		               SaturationMin,
		               SaturationMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_saturation_setting();
		set_image_adjustment_settings();
		break;

	case ColorTemperature: {
		const auto curr_value = get_color_temperature_setting_value().value_or(
		        ColorTemperatureNeutral);

		const auto delta     = (dir == Direction::Dec) ? -100 : 100;
		const auto new_value = clamp(curr_value + delta,
		                             ColorTemperatureMin,
		                             ColorTemperatureMax);

		set_setting("color_temperature", format_str("%d", new_value));

		update_color_temperature_setting();
		handle_auto_image_adjustment_settings(VGA_GetCurrentVideoMode());
		set_image_adjustment_settings();
	} break;

	case ColorTemperatureLumaPreserve:
		adjust_setting("color_temperature_luma_preserve",
		               ColorTemperatureLumaPreserveMin,
		               ColorTemperatureLumaPreserveMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_color_temperature_luma_preserve_setting();
		set_image_adjustment_settings();
		break;

	case RedGain:
		adjust_setting("red_gain",
		               RgbGainMin,
		               RgbGainMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_red_gain_setting();
		set_image_adjustment_settings();
		break;

	case GreenGain:
		adjust_setting("green_gain",
		               RgbGainMin,
		               RgbGainMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_green_gain_setting();
		set_image_adjustment_settings();
		break;

	case BlueGain:
		adjust_setting("blue_gain",
		               RgbGainMin,
		               RgbGainMax,
		               (dir == Direction::Dec) ? -1 : 1);

		update_blue_gain_setting();
		set_image_adjustment_settings();
		break;

	default: assertm(false, "Invalid ImageAdjustmentControl value");
	}
}

static void decrease_image_adjustment_control(const bool pressed)
{
	if (pressed) {
		adjust_image_setting(Direction::Dec);
	}
}

static void increase_image_adjustment_control(const bool pressed)
{
	if (pressed) {
		adjust_image_setting(Direction::Inc);
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
	const auto shader_descriptor = get_shader_setting_value();

	if (!set_shader(shader_descriptor)) {
		set_fallback_shader_or_exit(shader_descriptor);
	}
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

	update_black_level_color_setting();
	set_image_adjustment_settings();
}

void RENDER_Init()
{
	auto section = get_section("render");
	assert(section);

	render.deinterlacer = std::make_unique<Deinterlacer>();

	set_aspect_ratio_correction(*section);
	set_viewport(*section);
	set_integer_scaling(*section);

	set_monochrome_palette(*section);

	update_color_space_setting();

	update_enable_image_adjustments_setting();

	update_crt_color_profile_setting();
	update_brightness_setting();
	update_contrast_setting();
	update_gamma_setting();
	update_digital_contrast_setting();

	update_black_level_color_setting();
	update_black_level_setting();

	update_saturation_setting();

	update_color_temperature_setting();
	update_color_temperature_luma_preserve_setting();

	update_red_gain_setting();
	update_green_gain_setting();
	update_blue_gain_setting();

	set_image_adjustment_settings();

	set_deinterlacing(*section);
}

static void notify_render_setting_updated(SectionProp& section,
                                          const std::string& prop_name)
{
	if (prop_name == "aspect") {
		set_aspect_ratio_correction(section);
		reinit_drawing();

	} else if (prop_name == "cga_colors") {
		// TODO Support switching custom CGA colors at runtime.
		// This is somewhat complicated and needs experimentation.

	} else if (prop_name == "deinterlacing") {
		set_deinterlacing(section);
		render_reset();

	} else if (prop_name == "glshader" || prop_name == "shader") {

		const auto shader_descriptor = get_shader_setting_value();
		if (!set_shader(shader_descriptor)) {
			set_fallback_shader_or_exit(shader_descriptor);
		}
		reinit_drawing();

		set_section_property_value(
		        "render",
		        "shader",
		        GFX_GetRenderer()->GetCurrentSymbolicShaderDescriptor());

	} else if (prop_name == "integer_scaling") {
		set_integer_scaling(section);
		reinit_drawing();

	} else if (prop_name == "monochrome_palette") {
		set_monochrome_palette(section);
		update_black_level_color_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "viewport") {
		set_viewport(section);
		reinit_drawing();

	} else if (prop_name == "color_space") {
		update_color_space_setting();

	} else if (prop_name == "image_adjustments") {
		update_enable_image_adjustments_setting();

	} else if (prop_name == "crt_color_profile") {
		update_crt_color_profile_setting();
		handle_auto_image_adjustment_settings(VGA_GetCurrentVideoMode());
		set_image_adjustment_settings();

	} else if (prop_name == "brightness") {
		update_brightness_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "contrast") {
		update_contrast_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "gamma") {
		update_gamma_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "digital_contrast") {
		update_digital_contrast_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "black_level") {
		update_black_level_setting();
		handle_auto_image_adjustment_settings(VGA_GetCurrentVideoMode());
		set_image_adjustment_settings();

	} else if (prop_name == "saturation") {
		update_saturation_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "color_temperature") {
		update_color_temperature_setting();
		handle_auto_image_adjustment_settings(VGA_GetCurrentVideoMode());
		set_image_adjustment_settings();

	} else if (prop_name == "color_temperature_luma_preserve") {
		update_color_temperature_luma_preserve_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "red_gain") {
		update_red_gain_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "green_gain") {
		update_green_gain_setting();
		set_image_adjustment_settings();

	} else if (prop_name == "blue_gain") {
		update_blue_gain_setting();
		set_image_adjustment_settings();
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

#ifdef MACOSX
	constexpr auto ImageAdjustmentModKeys = MMOD2 | MMOD3;
#else
	// Windows & Linux
	constexpr auto ImageAdjustmentModKeys = MMOD3;
#endif

	MAPPER_AddHandler(select_prev_image_adjustment_control,
	                  SDL_SCANCODE_F9,
	                  ImageAdjustmentModKeys,
	                  "previmageadj",
	                  "PrevImageAdj");

	MAPPER_AddHandler(select_next_image_adjustment_control,
	                  SDL_SCANCODE_F10,
	                  ImageAdjustmentModKeys,
	                  "nextimageadj",
	                  "NextImageAdj");

	MAPPER_AddHandler(decrease_image_adjustment_control,
	                  SDL_SCANCODE_F11,
	                  ImageAdjustmentModKeys,
	                  "decimageadj",
	                  "DecImageAdj");

	MAPPER_AddHandler(increase_image_adjustment_control,
	                  SDL_SCANCODE_F12,
	                  ImageAdjustmentModKeys,
	                  "incimageadj",
	                  "IncImageAdj");

	init_render_settings(*section);
	register_render_text_messages();
}
