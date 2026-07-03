// SPDX-FileCopyrightText:  2019-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_H
#define DOSBOX_RENDER_H

#include <array>
#include <cstring>
#include <deque>
#include <optional>
#include <string>

#include "private/deinterlacer.h"

#include "gui/render/frame_dirty_tracker.h"
#include "hardware/video/vga.h"
#include "utils/fraction.h"
#include "utils/rect.h"
#include "utils/rgb888.h"

// The additional padding pixels are partly for some tweaked text modes
// (e.g., Q200x25x8 used by Necromancer's DOS Navigator), plus as a safety
// margin. The pixel-format expanders are plain per-pixel loops and must not
// rely on reading past the end of a row; the padding survives purely as
// buffer-sizing capacity.

// Make sure ScalerMaxWidth remains a multiple of 8
constexpr int ScalerWidthExtraPadding = 8 * 5;

constexpr int ScalerMaxWidth  = 1600 + ScalerWidthExtraPadding;
constexpr int ScalerMaxHeight = 1200;

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

struct Render {
	ImageInfo src = {};

	// Frames per second
	double fps = 0;

	struct {
		int cache_pitch = 0;

		// The row the next `RENDER_DrawLine()` call writes into `cache`
		int curr_row = 0;

		// The live source frame VGA scanout writes into, at
		// `render.src` dimensions (baked-in VGA scaling included)
		alignas(uint64_t)
		        std::array<uint32_t, ScalerMaxWidth * ScalerMaxHeight> cache = {};

		// Present-time scratch buffer the latched source frame is
		// expanded into (32-bit BGRX pixels at source dimensions)
		alignas(uint64_t)
		        std::array<uint32_t, ScalerMaxWidth * ScalerMaxHeight> out_buf = {};
	} scale = {};

	// Source-pixel snapshot of the last completed frame. Deep-copied from
	// `scale.cache` at the end of every successful
	// `RENDER_EndUpdate(false)`.
	//
	// Used as a clean source for screenshots and video capture, so
	// consumers don't read the live, possibly mid-frame `scale.cache`, and
	// as the input for `RENDER_RescaleLastFrame()` after a pause-time
	// recreate.
	struct {
		alignas(uint64_t)
		        std::array<uint32_t, ScalerMaxWidth * ScalerMaxHeight> cache = {};

		ImageInfo src = {};

		std::array<Rgb888, NumVgaColors> palette = {};

		int pitch = 0;

		// `true` when the latched data matches `render.src` exactly
		// (width, height, pixel format). Reset by `RENDER_SetSize()`,
		// so e.g. a pause-time scan-doubling toggle (`crt-hyllian` ->
		// `sharp` and back) drops it to false.
		// `RENDER_GetCurrentImage()` gates on this so screenshots /
		// video capture never pick up a frame at the previous geometry.
		bool valid = false;

		// `true` once any frame has ever been latched. *Not* reset by
		// `RENDER_SetSize()` -- the cache lives in a fixed-size array
		// embedded in this struct, so the bytes survive that reset.
		// `RENDER_RescaleLastFrame()` reads via this flag during
		// pause-time recreates: with no scanout coming to repopulate
		// the latch, the held bytes are the only source we can
		// replay through the freshly-configured scaler.
		bool populated = false;
	} last_complete_source = {};

	// Tracks live-frame-vs-latch dirtiness and the latch/upload
	// generations: whether the live source cache differs from the latch,
	// and whether the latch is newer than the frame the render backend
	// holds (see frame_dirty_tracker.h).
	FrameDirtyTracker frame_dirty = {};

	std::array<Rgb888, NumVgaColors> palette = {};

	bool active             = false;
	bool render_in_progress = false;

	AspectRatioCorrectionMode aspect_ratio_correction_mode = {};
	IntegerScalingMode integer_scaling_mode                = {};

	ViewportSettings viewport_settings = {};

	std::unique_ptr<Deinterlacer> deinterlacer   = {};
	DeinterlacingStrength deinterlacing_strength = {};
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

float get_gamma(const ColorSpace cs);

extern Render render;

// Called once per emitted scanline during VGA scanout (always through the
// `ReelMagic_RENDER_DrawLine` wrapper). A null `src_line_data` means "line
// unchanged". Only active between a successful `RENDER_StartUpdate()` and
// the matching `RENDER_EndUpdate()`; stray calls outside a scanout are
// ignored.
void RENDER_DrawLine(const void* src_line_data);

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

// Expand the latched source frame to 32-bit BGRX and hand it to the render
// backend. Called from the present path; the expansion and upload only run
// when the latch holds a newer frame than the one the backend has (tracked
// via the latch and upload generations).
void RENDER_MaybeUploadFrame();

// Returns the last completed source-pixel frame as a non-owning
// `RenderedImage`. Never returns a torn mid-scanout view -- the live
// `render.scale.cache` is private to the scaler. `image_data` is null
// until the first complete frame has been latched (e.g. right after a
// video mode change); callers must check.
RenderedImage RENDER_GetCurrentImage();

// Rescale the latched source frame at the current output dimensions and
// run it through `RENDER_EndUpdate(false)` so the renderer's
// `last_framebuf` is refreshed. No effect on VGA timing or PIC -- the
// emulator core stays paused. No-op if no frame has been latched yet.
void RENDER_RescaleLastFrame();

void RENDER_SetPalette(const uint8_t entry, const uint8_t red,
                       const uint8_t green, const uint8_t blue);

bool RENDER_NotifyVideoModeChanged(const VideoMode& video_mode);
void RENDER_NotifyEgaModeWithVgaPalette();

void RENDER_SetScanAndPixelDoubling();

#endif // DOSBOX_RENDER_H
