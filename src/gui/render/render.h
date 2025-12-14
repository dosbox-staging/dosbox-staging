// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_H
#define DOSBOX_RENDER_H

#include <cstring>
#include <deque>
#include <optional>
#include <string>

#include "gui/render/scaler/scalers.h"
#include "hardware/video/vga.h"
#include "utils/fraction.h"
#include "utils/rect.h"
#include "utils/rgb888.h"

enum class ViewportMode { Fit, Relative };

struct ViewportSettings {
	ViewportMode mode = ViewportMode::Fit;

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
	ImageInfo src      = {};
	uint32_t src_start = 0;

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
	int pitch = 0;

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

// CRT color profile emulation settings.
enum class CrtColorProfile {
	// Auto-select in adaptive CRT shader mode, otherwise None
	Auto = -1,

	// Raw RGB colours
	None = 0,

	// EBU standard phosphor emulation, used in high-end professional CRT
	// monitors, such as the Sony BVM/PVM series (6500K white point)
	Ebu = 1,

	// P22 phosphor emulation, the most commonly used in lower-end CRT
	// monitors (6500K white point)
	P22 = 2,

	// SMPT "C" phosphor emulation, the standard for American broadcast video
	// monitors (6500K white point)
	SmpteC = 3,

	// 1980s Philips home computer monitor colours (e.g., Commodore 1084,
	// Philips CM8833-II)
	Philips = 4,

	// Sony Trinitron CRT TV and monitor colours (~9300K whitepoint)
	Trinitron = 5
};

// Settings to mimic the image adjustment options of real CRT monitors
//
struct ImageAdjustmentSettings {

	// CRT colour profile emulation (see the `CrtColorProfile` enum).
	CrtColorProfile crt_color_profile = CrtColorProfile::None;

	// Analog brightness control. Valid range between 0.0 and 100.0; 50.0
	// means no change.
	float brightness = 50.0f;

	// Analog contrast control. Valid range between 0.0 and 100.0; 50.0
	// means no change.
	float contrast = 50.0f;

	// Gamma control. Valid range between -1.0 and 1.0; 0.0 means no change.
	float gamma = 0.0f;

	// Digital saturation control. Valid range between -1.0 and 1.0; 0.0
	// means no change
	float saturation = 0.0f;

	// Digital sigmoid ("S-curve") contrast. Valid range between -2.0
	// and 2.0; 0.0 means no change.
	float digital_contrast = 50.0f;

	// Used in CGA mono and Hercules modes to tint the raised black level as
	// true monochrome monitors can't display pure grey.
	Rgb888 black_level_color = {};

	// Minimum black level to achieve visible "black scanlines". Valid range
	// between 0.0 and 1.0; 0.0 means no change.
	float black_level = 0.0f;

	// Colour temperature (white point) in Kelvin (K); valid range is from
	// 3000 K to 10,000 K.
	float color_temperature_kelvin = 6500.0f;

	// Post color temperature adjustment luminosity preservation factor. 0.0
	// disables luminosity preservation, 1.0 restores the full luminosity.
	// The closer the value is to 1.0, the less precise the temperature of
	// the white point and lighter colours become.
	float color_temperature_luma_preserve = 0.0f;

	// Gain of the red channel. Valid range between 0.0 and 2.0; 1.0 means
	// no change (unity gain).
	float red_gain = 1.0f;

	// Gain of the green channel. Valid range between 0.0 and 2.0; 1.0 means
	// no change (unity gain).
	float green_gain = 1.0f;

	// Gain of the blue channel. Valid range between 0.0 and 2.0; 1.0 means
	// no change (unity gain).
	float blue_gain = 1.0f;
};

enum class ColorSpace {
	// Standard sRGB with D65 (6500K) whitepoint and sRGB gamma
	Srgb = 0,

	// DCI-P3 colour space with DCI whitepoint (~6300K) and 2.6 gamma
	DciP3 = 1,

	// DCI-P3 colour space variant with D65 whitepoint (6500K) and 2.6 gamma
	DciP3_D65 = 2,

	// Display P3 with D65 whitepoint (6500K) and sRGB gamma
	DisplayP3 = 3,

	// "Modern" DCI-P3 variant for average consumer/gamer displays with ~90%
	// P3 colour space coverage (D65 whitepoint and sRGB gamma)
	ModernP3 = 4,

	// AdobeRgb 2020 with D65 whitepoint (6500K) and 2.2 gamma
	AdobeRgb = 5,

	// Rec.2020 with D65 whitepoint (6500K) and 2.2 gamma
	Rec2020 = 6
};

extern Render render;
extern ScalerLineHandler_t RENDER_DrawLine;

void RENDER_Init();
void RENDER_Reinit();

void RENDER_AddConfigSection(const ConfigPtr& conf);

void RENDER_SetShaderWithFallback();

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

bool RENDER_NotifyVideoModeChanged(const VideoMode& video_mode);
void RENDER_NotifyEgaModeWithVgaPalette();

#endif // DOSBOX_RENDER_H
