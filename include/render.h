// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_H
#define DOSBOX_RENDER_H

#include <cstring>
#include <deque>
#include <optional>
#include <string>

#include "../src/gui/render/scaler/scalers.h"
#include "fraction.h"
#include "rect.h"
#include "vga.h"

enum class ViewportMode { Fit, Relative };

struct ViewportSettings {
	ViewportMode mode = {};

	// Either parameter can be set in Fit mode (but not both at the
	// same time), or none
	struct {
		std::optional<DosBox::Rect> limit_size = {};
		std::optional<float> desktop_scale     = {};
	} fit = {};

	struct {
		float height_scale = 1.0f;
		float width_scale  = 1.0f;
	} relative = {};

	constexpr bool operator==(const ViewportSettings& that) const
	{
		return (mode == that.mode && fit.limit_size == that.fit.limit_size &&
		        fit.desktop_scale == that.fit.desktop_scale &&
		        relative.height_scale == that.relative.height_scale &&
		        relative.width_scale == that.relative.width_scale);
	}

	constexpr bool operator!=(const ViewportSettings& that) const
	{
		return !operator==(that);
	}
};

enum class IntegerScalingMode {
	Off,
	Auto,
	Horizontal,
	Vertical,
};

enum class AspectRatioCorrectionMode {
	// Calculate the pixel aspect ratio from the display timings on VGA, and
	// from heuristics & hardcoded values on all other adapters.
	Auto,

	// Always force square pixels (1:1 pixel aspect ratio).
	SquarePixels,

	// Use a 4:3 display aspect ratio viewport as the starting point, then
	// apply user-defined horizontal and vertical scale factors to it. Stretch
	// all video modes into the resulting viewport and derive the pixel aspect
	// ratios from that.
	Stretch
};

struct RenderPal_t {
	struct {
		uint8_t red    = 0;
		uint8_t green  = 0;
		uint8_t blue   = 0;
		uint8_t unused = 0;
	} rgb[256] = {};

	union {
		uint16_t b16[256];
		uint32_t b32[256] = {};
	} lut = {};

	bool changed          = false;
	uint8_t modified[256] = {};
	uint32_t first        = 0;
	uint32_t last         = 0;
};

struct Render {
	ImageInfo src = {};
	uint32_t src_start   = 0;

	// Frames per second
	double fps = 0;

	struct {
		uint32_t size = 0;

		ScalerMode inMode  = {};
		ScalerMode outMode = {};

		bool clearCache = false;

		ScalerLineHandler_t lineHandler    = nullptr;
		ScalerLineHandler_t linePalHandler = nullptr;

		uint32_t blocks     = 0;
		uint32_t lastBlock  = 0;
		int outPitch        = 0;
		uint8_t* outWrite   = nullptr;
		uint32_t cachePitch = 0;
		uint8_t* cacheRead  = nullptr;
		uint32_t inHeight   = 0;
		uint32_t inLine     = 0;
		uint32_t outLine    = 0;
	} scale = {};

	RenderPal_t pal = {};

	bool updating  = false;
	bool active    = false;
	bool fullFrame = true;

	std::string current_shader_name = {};
	bool force_reload_shader        = false;

	IntegerScalingMode integer_scaling_mode = {};
};

// A frame of the emulated video output that's passed to the rendering backend
// or to the image and video capturers.
//
// Also used for passing the post-shader output read back from the frame buffer
// to the image capturer.
//
struct RenderedImage {
	ImageInfo params = {};

	// If true, the image is stored flipped vertically, starting from the
	// bottom row
	bool is_flipped_vertically = false;

	// Bytes per row
	uint16_t pitch = 0;

	// (width * height) number of pixels stored in the pixel format defined
	// by pixel_format
	uint8_t* image_data = nullptr;

	// Pointer to a (256 * 4) byte long palette data, stored as 8-bit RGB
	// values with 1 extra padding byte per entry (R0, G0, B0, X0, R1, G1,
	// B1, X1, etc.)
	uint8_t* palette_data = nullptr;

	inline bool is_paletted() const
	{
		return (params.pixel_format == PixelFormat::Indexed8);
	}

	RenderedImage deep_copy() const
	{
		RenderedImage copy = *this;

		// Deep-copy image and palette data
		const auto image_data_num_bytes = static_cast<uint32_t>(
		        params.height * pitch);

		copy.image_data = new uint8_t[image_data_num_bytes];

		assert(image_data);
		std::memcpy(copy.image_data, image_data, image_data_num_bytes);

		// TODO it's bad that we need to make this assumption downstream
		// on the size and alignment of the palette...
		if (palette_data) {
			constexpr uint16_t PaletteNumBytes = 256 * 4;
			copy.palette_data = new uint8_t[PaletteNumBytes];

			std::memcpy(copy.palette_data, palette_data, PaletteNumBytes);
		}
		return copy;
	}

	void free()
	{
		if (image_data) {
			delete[] image_data;
			image_data = nullptr;
		}
		if (palette_data) {
			delete[] palette_data;
			palette_data = nullptr;
		}
	}
};

extern Render render;
extern ScalerLineHandler_t RENDER_DrawLine;

void RENDER_Reinit();

void RENDER_AddConfigSection(const ConfigPtr& conf);

AspectRatioCorrectionMode RENDER_GetAspectRatioCorrectionMode();

DosBox::Rect RENDER_CalcRestrictedViewportSizeInPixels(const DosBox::Rect& canvas_px);

DosBox::Rect RENDER_CalcDrawRectInPixels(const DosBox::Rect& canvas_size_px,
                                         const DosBox::Rect& render_size_px,
                                         const Fraction& render_pixel_aspect_ratio);
std::string RENDER_GetCgaColorsSetting();

void RENDER_SyncMonochromePaletteSetting(const enum MonochromePalette palette);

std::deque<std::string> RENDER_GenerateShaderInventoryMessage();
void RENDER_AddMessages();

void RENDER_SetSize(const ImageInfo& image_info, const double frames_per_second);

bool RENDER_StartUpdate();
void RENDER_EndUpdate(bool abort);

void RENDER_SetPalette(const uint8_t entry, const uint8_t red,
                       const uint8_t green, const uint8_t blue);

bool RENDER_MaybeAutoSwitchShader([[maybe_unused]] const DosBox::Rect canvas_size_px,
                                  [[maybe_unused]] const VideoMode& video_mode,
                                  [[maybe_unused]] const bool reinit_render);

void RENDER_NotifyEgaModeWithVgaPalette();

#endif // DOSBOX_RENDER_H
