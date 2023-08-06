/*
 *  Copyright (C) 2019-2023  The DOSBox Staging Team
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
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>

#include <sys/types.h>

#include "../capture/capture.h"
#include "control.h"
#include "cross.h"
#include "fraction.h"
#include "mapper.h"
#include "render.h"
#include "setup.h"
#include "shell.h"
#include "string_utils.h"
#include "support.h"
#include "vga.h"
#include "video.h"

#include "render_scalers.h"

Render_t render;
ScalerLineHandler_t RENDER_DrawLine;

const char* to_string(const PixelFormat pf)
{
	switch (pf) {
	case PixelFormat::Indexed8: return "Indexed8";
	case PixelFormat::BGR555: return "BGR555";
	case PixelFormat::BGR565: return "BGR565";
	case PixelFormat::BGR888: return "BGR888";
	case PixelFormat::BGRX8888: return "BGRX8888";
	default: assertm(false, "Invalid pixel format"); return {};
	}
}

uint8_t get_bits_per_pixel(const PixelFormat pf)
{
	return enum_val(pf);
}

static void render_callback(GFX_CallBackFunctions_t function);

static void check_palette(void)
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
		auto src   = static_cast<const uintptr_t*>(s);
		auto cache = reinterpret_cast<uintptr_t*>(render.scale.cacheRead);
		for (Bits x = render.src.start; x > 0;) {
			const auto src_ptr = reinterpret_cast<const uint8_t*>(src);
			const auto src_val = read_unaligned_size_t(src_ptr);
			if (GCC_UNLIKELY(src_val != cache[0])) {
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
		auto src   = static_cast<const uintptr_t*>(s);
		auto cache = reinterpret_cast<uintptr_t*>(render.scale.cacheRead);
		for (Bits x = render.src.start; x > 0;) {
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
	Bitu x, width;
	uint32_t *srcLine, *cacheLine;
	srcLine   = (uint32_t*)src;
	cacheLine = (uint32_t*)render.scale.cacheRead;
	width     = render.scale.cachePitch / 4;
	for (x = 0; x < width; x++) {
		cacheLine[x] = ~srcLine[x];
	}
	render.scale.lineHandler(src);
}

bool RENDER_StartUpdate(void)
{
	if (GCC_UNLIKELY(render.updating)) {
		return false;
	}
	if (GCC_UNLIKELY(!render.active)) {
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
	if (GCC_UNLIKELY(render.scale.clearCache)) {
		// LOG_MSG("Clearing cache");

		// Will always have to update the screen with this one anyway,
		// so let's update already
		if (GCC_UNLIKELY(!GFX_StartUpdate(render.scale.outWrite,
		                                  render.scale.outPitch))) {
			return false;
		}
		render.fullFrame        = true;
		render.scale.clearCache = false;
		RENDER_DrawLine         = clear_cache_handler;
	} else {
		if (render.pal.changed) {
			// Assume pal changes always do a full screen update
			// anyway
			if (GCC_UNLIKELY(!GFX_StartUpdate(render.scale.outWrite,
			                                  render.scale.outPitch))) {
				return false;
			}
			RENDER_DrawLine  = render.scale.linePalHandler;
			render.fullFrame = true;
		} else {
			RENDER_DrawLine = start_line_handler;
			if (GCC_UNLIKELY(CAPTURE_IsCapturingImage() ||
			                 CAPTURE_IsCapturingVideo())) {
				render.fullFrame = true;
			} else {
				render.fullFrame = false;
			}
		}
	}
	render.updating = true;
	return true;
}

static void halt_render(void)
{
	RENDER_DrawLine = empty_line_handler;
	GFX_EndUpdate(nullptr);
	render.updating = false;
	render.active   = false;
}

extern uint32_t PIC_Ticks;
void RENDER_EndUpdate(bool abort)
{
	if (GCC_UNLIKELY(!render.updating)) {
		return;
	}

	RENDER_DrawLine = empty_line_handler;

	if (GCC_UNLIKELY((CAPTURE_IsCapturingImage() || CAPTURE_IsCapturingVideo()))) {
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

		image.width              = render.src.width;
		image.height             = render.src.height;
		image.double_width       = double_width;
		image.double_height      = double_height;
		image.pixel_aspect_ratio = render.src.pixel_aspect_ratio;
		image.pixel_format       = render.src.pixel_format;
		image.pitch              = render.scale.cachePitch;
		image.image_data         = (uint8_t*)&scalerSourceCache;
		image.palette_data       = (uint8_t*)&render.pal.rgb;

		auto video_mode = render.video_mode;

		const auto frames_per_second = static_cast<float>(render.src.fps);

		CAPTURE_AddFrame(image, video_mode, frames_per_second);
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
std::mutex render_reset_mutex;

static void render_reset(void)
{
	if (render.src.width == 0 || render.src.height == 0) {
		return;
	}

	// Despite rendering being a single-threaded sequence, the Reset() can
	// be called from the rendering callback, which might come from a video
	// driver operating in a different thread or process.
	std::lock_guard<std::mutex> guard(render_reset_mutex);

	Bitu width         = render.src.width;
	bool double_width  = render.src.double_width;
	bool double_height = render.src.double_height;

	Bitu gfx_flags, xscale, yscale;
	ScalerSimpleBlock_t* simpleBlock = &ScaleNormal1x;

	// Don't do software scaler sizes larger than 4k
	Bitu maxsize_current_input = SCALER_MAXWIDTH / width;
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

	if ((width * simpleBlock->xscale > SCALER_MAXWIDTH) ||
	    (render.src.height * simpleBlock->yscale > SCALER_MAXHEIGHT)) {
		simpleBlock = &ScaleNormal1x;
	}

	gfx_flags = simpleBlock->gfxFlags;
	xscale    = simpleBlock->xscale;
	yscale    = simpleBlock->yscale;
	//		LOG_MSG("Scaler:%s",simpleBlock->name);

	switch (render.src.pixel_format) {
	case PixelFormat::Indexed8:
		render.src.start = (render.src.width * 1) / sizeof(Bitu);
		break;
	case PixelFormat::BGR555:
		render.src.start = (render.src.width * 2) / sizeof(Bitu);
		gfx_flags        = (gfx_flags & ~GFX_CAN_8);
		break;
	case PixelFormat::BGR565:
		render.src.start = (render.src.width * 2) / sizeof(Bitu);
		gfx_flags        = (gfx_flags & ~GFX_CAN_8);
		break;
	case PixelFormat::BGR888:
		render.src.start = (render.src.width * 3) / sizeof(Bitu);
		gfx_flags        = (gfx_flags & ~GFX_CAN_8);
		break;
	case PixelFormat::BGRX8888:
		render.src.start = (render.src.width * 4) / sizeof(Bitu);
		gfx_flags        = (gfx_flags & ~GFX_CAN_8);
		break;
	}

	gfx_flags = GFX_GetBestMode(gfx_flags);

	if (!gfx_flags) {
		if (simpleBlock == &ScaleNormal1x) {
			E_Exit("Failed to create a rendering output");
		}
	}
	width *= xscale;
	const auto height = make_aspect_table(render.src.height, yscale, yscale);

	// Set up scaler variables
	if (double_height) {
		gfx_flags |= GFX_DBL_H;
	}
	if (double_width) {
		gfx_flags |= GFX_DBL_W;
	}

#if C_OPENGL
	GFX_SetShader(render.shader.source);
#endif

	const auto render_pixel_aspect_ratio = render.src.pixel_aspect_ratio;

	gfx_flags = GFX_SetSize(width,
	                        height,
	                        render_pixel_aspect_ratio,
	                        gfx_flags,
	                        render.video_mode,
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
	case PixelFormat::BGR555:
		render.scale.lineHandler = (*lineBlock)[1][render.scale.outMode];
		render.scale.linePalHandler = nullptr;
		render.scale.inMode         = scalerMode15;
		render.scale.cachePitch     = render.src.width * 2;
		break;
	case PixelFormat::BGR565:
		render.scale.lineHandler = (*lineBlock)[2][render.scale.outMode];
		render.scale.linePalHandler = nullptr;
		render.scale.inMode         = scalerMode16;
		render.scale.cachePitch     = render.src.width * 2;
		break;
	case PixelFormat::BGR888:
		render.scale.lineHandler = (*lineBlock)[3][render.scale.outMode];
		render.scale.linePalHandler = nullptr;
		render.scale.inMode         = scalerMode32;
		render.scale.cachePitch     = render.src.width * 3;
		break;
	case PixelFormat::BGRX8888:
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

void RENDER_SetSize(const uint16_t width, const uint16_t height,
                    const bool double_width, const bool double_height,
                    const Fraction& render_pixel_aspect_ratio,
                    const PixelFormat pixel_format,
                    const double frames_per_second, const VideoMode& video_mode)
{
	halt_render();

	if (!width || !height || width > SCALER_MAXWIDTH || height > SCALER_MAXHEIGHT) {
		return;
	}

	render.src.width              = width;
	render.src.height             = height;
	render.src.double_width       = double_width;
	render.src.double_height      = double_height;
	render.src.pixel_aspect_ratio = render_pixel_aspect_ratio;
	render.src.pixel_format       = pixel_format;
	render.src.fps                = frames_per_second;

	render.video_mode = video_mode;

	render_reset();
}

#if C_OPENGL

// Reads the given shader path into the string
static bool read_shader(const std_fs::path& shader_path, std::string& shader_str)
{
	std::ifstream fshader(shader_path, std::ios_base::binary);
	if (!fshader.is_open()) {
		return false;
	}

	std::stringstream buf;
	buf << fshader.rdbuf();
	fshader.close();
	if (buf.str().empty()) {
		return false;
	}

	shader_str = buf.str();
	shader_str += '\n';
	return true;
}

std::deque<std::string> RENDER_InventoryShaders()
{
	std::deque<std::string> inventory;
	inventory.emplace_back("");
	inventory.emplace_back("List of available GLSL shaders");
	inventory.emplace_back("------------------------------");

	const std::string dir_prefix  = "Path '";
	const std::string file_prefix = "        ";

	std::error_code ec = {};
	for (auto& [dir, shaders] : GetFilesInResource("glshaders", ".glsl")) {
		const auto dir_exists      = std_fs::is_directory(dir, ec);
		auto shader                = shaders.begin();
		const auto dir_has_shaders = shader != shaders.end();
		const auto dir_postfix     = dir_exists
		                                   ? (dir_has_shaders ? "' has:"
		                                                      : "' has no shaders")
		                                   : "' does not exist";

		inventory.emplace_back(dir_prefix + dir.string() + dir_postfix);

		while (shader != shaders.end()) {
			shader->replace_extension("");
			const auto is_last = (shader + 1 == shaders.end());
			inventory.emplace_back(file_prefix +
			                       (is_last ? "`- " : "|- ") +
			                       shader->string());
			shader++;
		}
		inventory.emplace_back("");
	}
	inventory.emplace_back(
	        "The above shaders can be used exactly as listed in the \"glshader\"");
	inventory.emplace_back(
	        "conf setting, without the need for the resource path or .glsl extension.");
	inventory.emplace_back("");
	return inventory;
}

static bool read_shader_source(const std::string& shader_path, std::string& source)
{
	// Start with the path as-is and then try from resources
	const auto candidate_paths = {std_fs::path(shader_path),
	                              std_fs::path(shader_path + ".glsl"),
	                              GetResourcePath("glshaders", shader_path),
	                              GetResourcePath("glshaders",
	                                              shader_path + ".glsl")};

	std::string s; // to be populated with the shader source
	for (const auto& p : candidate_paths) {
		if (read_shader(p, s)) {
			break;
		}
	}

	if (s.empty()) {
		source.clear();
		return false;
	}

	if (first_shell) {
		std::string pre_defs;
		const size_t count = first_shell->GetEnvCount();
		for (size_t i = 0; i < count; ++i) {
			std::string env;
			if (!first_shell->GetEnvNum(i, env)) {
				continue;
			}
			if (env.compare(0, 9, "GLSHADER_") == 0) {
				const auto brk = env.find('=');
				if (brk == std::string::npos) {
					continue;
				}
				env[brk] = ' ';
				pre_defs += "#define " + env.substr(9) + '\n';
			}
		}
		if (pre_defs.length()) {
			// If "#version" occurs, it must be before anything,
			// except comments and whitespace
			auto pos = s.find("#version ");

			if (pos != std::string::npos) {
				pos = s.find('\n', pos + 9);
			}

			s.insert(pos, pre_defs);
		}
	}
	if (s.empty()) {
		source.clear();
		LOG_ERR("RENDER: Failed to read shader source");
		return false;
	}
	source = std::move(s);
	assert(source.length());
	return true;
}

static void set_shader_settings(const std::string& source)
{
	try {
		const std::regex re("\\s*#pragma\\s+(\\w+)");
		std::sregex_iterator next(source.begin(), source.end(), re);
		const std::sregex_iterator end;

		while (next != end) {
			std::smatch match = *next;

			auto pragma = match[1].str();
			if (pragma == "use_srgb_texture") {
				render.shader.settings.use_srgb_texture = true;
			} else if (pragma == "use_srgb_framebuffer") {
				render.shader.settings.use_srgb_framebuffer = true;
			} else if (pragma == "force_single_scan") {
				render.shader.settings.force_single_scan = true;
			} else if (pragma == "force_no_pixel_doubling") {
				render.shader.settings.force_no_pixel_doubling = true;
			}
			++next;
		}
	} catch (std::regex_error& e) {
		LOG_ERR("RENDER: Regex error while parsing OpenGL shader for pragmas: %d",
		        e.code());
	}
}

bool RENDER_UseSrgbTexture()
{
	return render.shader.settings.use_srgb_texture;
}

bool RENDER_UseSrgbFramebuffer()
{
	return render.shader.settings.use_srgb_framebuffer;
}

#endif

#if C_OPENGL
void log_warning_if_legacy_shader_name(const std::string& name)
{
	static const std::map<std::string, std::string> legacy_name_mappings = {
	        {"advinterp2x", "scaler/advinterp2x"},
	        {"advinterp3x", "scaler/advinterp3x"},
	        {"advmame2x", "scaler/advmame2x"},
	        {"advmame3x", "scaler/advmame3x"},
	        {"crt-easymode-flat", "crt/easymode.tweaked"},
	        {"crt-fakelottes-flat", "crt/fakelottes"},
	        {"rgb2x", "scaler/rgb2x"},
	        {"rgb3x", "scaler/rgb3x"},
	        {"scan2x", "scaler/scan2x"},
	        {"scan3x", "scaler/scan3x"},
	        {"sharp", "interpolation/sharp"},
	        {"tv2x", "scaler/tv2x"},
	        {"tv3x", "scaler/tv3x"}};

	std_fs::path shader_path = name;
	std_fs::path ext         = shader_path.extension();

	if (!(ext == "" || ext == ".glsl")) {
		return;
	}

	shader_path.replace_extension("");

	const auto it = legacy_name_mappings.find(shader_path.string());
	if (it != legacy_name_mappings.end()) {
		const auto new_name = it->second;
		LOG_WARNING("RENDER: Built-in shader '%s' has been renamed; please use '%s' instead.",
		            name.c_str(),
		            new_name.c_str());
	}
}
#endif

// Returns true if the shader has been changed, or if a shader has been loaded
// for the first time since launch
static bool init_shader([[maybe_unused]] Section* sec)
{
#if C_OPENGL
	assert(control);
	const Section* sdl_sec = control->GetSection("sdl");
	assert(sdl_sec);

	const bool using_opengl = starts_with(sdl_sec->GetPropValue("output"),
	                                      "opengl");

	const auto render_sec = static_cast<const Section_prop*>(
	        control->GetSection("render"));

	assert(render_sec);
	auto sh       = render_sec->Get_path("glshader");
	auto filename = std::string(sh->GetValue());

	constexpr auto fallback_shader = "none";
	if (filename.empty()) {
		filename = fallback_shader;
	} else if (filename == "default") {
		filename = "interpolation/sharp";
	}

	if (render.shader.filename == filename) {
		return false;
	}

	log_warning_if_legacy_shader_name(filename);

	std::string source = {};

	if (!read_shader_source(sh->realpath.string(), source) &&
	    (sh->realpath == filename || !read_shader_source(filename, source))) {
		sh->SetValue("none");
		source.clear();

		// List all the existing shaders for the user
		LOG_ERR("RENDER: Shader file '%s' not found", filename.c_str());
		for (const auto& line : RENDER_InventoryShaders()) {
			LOG_WARNING("RENDER: %s", line.c_str());
		}

		// Fallback to the 'none' shader and otherwise fail
		if (read_shader_source(fallback_shader, source)) {
			filename = fallback_shader;
		} else {
			E_Exit("RENDER: Fallback shader file '%s' not found and is mandatory",
			       fallback_shader);
		}
	}

	// Reset shader settings to defaults
	render.shader.settings = {};

	if (using_opengl && source.length() && render.shader.filename != filename) {
		LOG_MSG("RENDER: Using GLSL shader '%s'", filename.c_str());
		set_shader_settings(source);

		// Move the temporary filename and source into the memebers
		render.shader.filename = std::move(filename);
		render.shader.source   = std::move(source);

		// Pass the shader source up to the GFX engine
		GFX_SetShader(render.shader.source);
	}

	return true;
#endif
}

void RENDER_InitShader(Section* sec)
{
	init_shader(sec);
}

void RENDER_Init(Section* sec);

static void reload_shader(const bool pressed)
{
	// Quick and dirty hack to reload the current shader. Very useful when
	// tweaking shader presets. Ultimately, this code will go away once the
	// new shading system has been introduced, so massaging the current code
	// to make this "nicer" would be largely a wasted effort...
	if (!pressed) {
		return;
	}

	assert(control);
	auto render_section = control->GetSection("render");

	assert(render_section);
	const auto sec = dynamic_cast<Section_prop*>(render_section);

	auto glshader_prop = sec ? sec->Get_path("glshader") : nullptr;
	if (!glshader_prop) {
		LOG_WARNING("RENDER: Failed parsing the 'glshader' setting; not reloading");
		return;
	}

	assert(glshader_prop);
	const auto shader_path = std::string(glshader_prop->GetValue());
	if (shader_path.empty()) {
		LOG_WARNING("RENDER: Failed getting path for 'glshader' setting; not reloading");
		return;
	}

	glshader_prop->SetValue("none");
	RENDER_Init(render_section);

	glshader_prop->SetValue(shader_path);
	RENDER_Init(render_section);

	// The shader settings might have been changed (e.g. force_single_scan,
	// force_no_pixel_doubling), so force re-rendering the image using the
	// new settings. Without this, the altered settings would only take
	// effect on the next video mode change.
	VGA_SetupDrawing(0);
}

static bool force_square_pixels     = false;
static bool force_vga_single_scan   = false;
static bool force_no_pixel_doubling = false;

// We double-scan VGA modes and pixel-double all video modes by default unless:
//
//  1) Single-scanning or no pixel-doubling is forced by the OpenGL shader.
//  2) The interpolation mode is nearest-neighbour.
//
// About the first point: the default `interpolation/sharp.glsl` shader forces
// both because it scales pixels as flat adjacent rectangles. This not only
// produces identical output versus double-scanning and pixel-doubling, but
// also provides finer integer scaling steps (especially important on sub-4K
// screens) and improves performance on low-end systems like the Raspberry Pi.
//
static void setup_scan_and_pixel_doubling([[maybe_unused]] Section_prop* section)
{
	const auto nearest_neighbour_enabled = (GFX_GetInterpolationMode() ==
	                                        InterpolationMode::NearestNeighbour);

	force_vga_single_scan   = nearest_neighbour_enabled;
	force_no_pixel_doubling = nearest_neighbour_enabled;

#if C_OPENGL
	force_vga_single_scan |= render.shader.settings.force_single_scan;
	force_no_pixel_doubling |= render.shader.settings.force_no_pixel_doubling;
#endif

	VGA_EnableVgaDoubleScanning(!force_vga_single_scan);
	VGA_EnablePixelDoubling(!force_no_pixel_doubling);
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

static void init_render_settings(Section_prop& secprop)
{
	constexpr auto always        = Property::Changeable::Always;
	constexpr auto deprecated    = Property::Changeable::Deprecated;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	auto* int_prop = secprop.Add_int("frameskip", deprecated, 0);
	int_prop->Set_help(
	        "Consider capping frame-rates using the '[sdl] host_rate' setting.");

	auto* bool_prop = secprop.Add_bool("aspect", always, true);
	bool_prop->Set_help(
	        "Apply aspect ratio correction for modern square-pixel flat-screen displays,\n"
	        "so DOS resolutions with non-square pixels appear as they would on a 4:3 display\n"
	        "aspect ratio CRT monitor the majority of DOS games were designed for (enabled\n"
	        "by default). This setting only affects video modes that use non-square pixels,\n"
	        "such as 320x200 or 640x400; square-pixel modes, such as 320x240, 640x480, and\n"
	        "800x600 are displayed as-is.");

	auto* string_prop = secprop.Add_string("integer_scaling", always, "off");
	string_prop->Set_help(
	        "Constrain the horizontal or vertical scaling factor to integer values.\n"
	        "The correct aspect ratio is always maintained, which may result in a\n"
	        "non-integer scaling factor to be applied in the other dimension. If the\n"
	        "image is larger than the viewport, the integer scaling constraint is\n"
	        "auto-disabled (same as 'off').\n"
	        "  off:         No integer scaling constraint; the aspect ratio correct image\n"
	        "               fills the viewport (default).\n"
	        "  horizontal:  Constrain the horizontal scaling factor to integer values\n"
	        "               within the viewport.\n"
	        "  vertical:    Constrain the vertical scaling factor to integer values\n"
	        "               within the viewport.");

	string_prop = secprop.Add_string("monochrome_palette",
	                                 always,
	                                 MonochromePaletteAmber);
	string_prop->Set_help(
	        "Set the palette for monochrome display emulation ('amber' by default).\n"
	        "Works only with the 'hercules' and 'cga_mono' machine types.\n"
	        "Note: You can also cycle through the available palettes via hotkeys.");

	const char* mono_pal[] = {MonochromePaletteAmber,
	                          MonochromePaletteGreen,
	                          MonochromePaletteWhite,
	                          MonochromePalettePaperwhite,
	                          nullptr};
	string_prop->Set_values(mono_pal);

	string_prop = secprop.Add_string("cga_colors", only_at_start, "default");
	string_prop->Set_help(
	        "Set the interpretation of CGA RGBI colours. Affects all machine types capable\n"
	        "of displaying CGA or better graphics. Built-in presets:\n"
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
	        "  colodore-sat:  Colodore palette with 20% more saturation.\n"
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
	        "  - If you used the normal2x/3x scalers, set a desired 'windowresolution'\n"
	        "    instead.\n"
	        "  - If you used an advanced scaler, consider one of the 'glshader'\n"
	        "    options instead.");

#if C_OPENGL
	string_prop = secprop.Add_path("glshader", always, "default");
	string_prop->Set_help(
	        "Set the GLSL shader to use in OpenGL output modes.\n"
	        "Options include 'default', 'none', a shader listed using the --list-glshaders\n"
	        "command-line argument, or an absolute or relative path to a file.\n"
	        "'default' sets the 'interpolation/sharp.glsl' shader.\n"
	        "In all cases, you may omit the shader's '.glsl' file extension.");
#endif
}

void RENDER_AddConfigSection(const config_ptr_t& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	Section_prop* sec = conf->AddSection_prop("render",
	                                          &RENDER_Init,
	                                          changeable_at_runtime);
	assert(sec);
	init_render_settings(*sec);
}

void RENDER_SyncMonochromePaletteSetting(const enum MonochromePalette palette)
{
	assert(control);

	auto render_section = control->GetSection("render");
	assert(render_section);

	const auto sec = dynamic_cast<Section_prop*>(render_section);
	assert(sec);
	if (!sec) {
		return;
	}

	const auto string_prop = sec->GetStringProp("monochrome_palette");
	string_prop->SetValue(to_string(palette));
}

void RENDER_Init(Section* sec)
{
	Section_prop* section = static_cast<Section_prop*>(sec);
	assert(section);

	// For restarting the renderer
	static auto running = false;

	const auto prev_scale_size              = render.scale.size;
	const auto prev_force_square_pixels     = force_square_pixels;
	const auto prev_force_vga_single_scan   = force_vga_single_scan;
	const auto prev_force_no_pixel_doubling = force_no_pixel_doubling;
	const auto prev_integer_scaling_mode    = GFX_GetIntegerScalingMode();

	render.pal.first = 256;
	render.pal.last  = 0;

	force_square_pixels = !section->Get_bool("aspect");
	VGA_ForceSquarePixels(force_square_pixels);

	const auto mono_palette = to_monochrome_palette_enum(
	        section->Get_string("monochrome_palette"));
	VGA_SetMonochromePalette(mono_palette);

	// Only use the default 1x rendering scaler
	render.scale.size = 1;

	GFX_SetIntegerScalingMode(section->Get_string("integer_scaling"));

#if C_OPENGL
	const auto shader_changed = init_shader(section);
#endif

	setup_scan_and_pixel_doubling(section);

	const auto needs_reinit =
	        ((force_square_pixels != prev_force_square_pixels) ||
	         (render.scale.size != prev_scale_size) ||
	         (GFX_GetIntegerScalingMode() != prev_integer_scaling_mode)
#if C_OPENGL
	         || shader_changed
#endif
	         || (prev_force_vga_single_scan != force_vga_single_scan) ||
	         (prev_force_no_pixel_doubling != force_no_pixel_doubling));

	if (running && needs_reinit) {
		render_callback(GFX_CallBackReset);
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
