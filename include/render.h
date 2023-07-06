/*
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

#ifndef DOSBOX_RENDER_H
#define DOSBOX_RENDER_H

#include <cstring>
#include <deque>
#include <string>

#include "../src/gui/render_scalers.h"
#include "fraction.h"

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

struct VideoMode {
	uint16_t width              = 0;
	uint16_t height             = 0;
	Fraction pixel_aspect_ratio = {};
};

struct Render_t {
	// Details about the rendered image.
	// E.g. for the 320x200 256-colour 13h VGA mode with double-scanning
	// and pixel-doubling enabled:
	//   - width = 320 (will be pixel-doubled post-render via double_width)
	//   - height = 400 (2*200 lines because we're rendering scan-doubled)
	//   - pixel_aspect_ratio = 5/6 (1:1.2) (because the PAR is meant for the
	//     final image, post the optional width & height doubling)
	//   - double_width = true (pixel-doubling)
	//   - double_height = false (we're rendering scan-doubled)
	struct {
		// Width of the rendered image (prior to optional width-doubling)
		uint16_t width = 0;

		// Height of the rendered image (prior to optional height-doubling)
		uint16_t height = 0;

		// If true, the rendered image is doubled horizontally after via
		// a scaler (e.g. to achieve pixel-doubling)
		bool double_width = false;

		// If true, the rendered image is doubled vertically via a
		// scaler (e.g. to achieve fake double-scanning)
		bool double_height = false;

		// Pixel aspect ratio to be applied to the final image (post
		// width & height doubling) so it appears as intended on screen.
		// (video_mode.pixel_aspect_ratio holds the "nominal" pixel
		// aspect ratio of the source video mode)
		Fraction pixel_aspect_ratio = {};

		uint32_t start = 0;

		// Pixel format of the image data (see `bpp` in vga.h for details)
		uint8_t bpp = 0;

		// Frames per second
		double fps = 0;
	} src = {};

	// Details about the source video mode.
	// This is usually different than the rendered image's details.
	// E.g. for the 320x200 256-colour 13h VGA mode we always have the
	// following, regardless of whether double-scanning and pixel-doubling
	// is enabled at the rendering level:
	//   - width = 320
	//   - height = 200
	//   - pixel_aspect_ratio = 5/6 (1:1.2)
	VideoMode video_mode = {};

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

#if C_OPENGL
	struct {
		std::string filename         = {};
		std::string source           = {};
		bool use_srgb_texture        = false;
		bool use_srgb_framebuffer    = false;
		bool force_single_scan       = false;
		bool force_no_pixel_doubling = false;
	} shader = {};
#endif

	RenderPal_t pal = {};

	bool updating            = false;
	bool active              = false;
	bool fullFrame           = true;
};

// A frame of the emulated video output that's passed to the rendering backend
// or to the image and video capturers.
//
// Also used for passing the post-shader output read back from the frame buffer
// to the image capturer.
//
struct RenderedImage {
	// Width of the rendered image (prior to optional width-doubling)
	uint16_t width = 0;

	// Height of the rendered image (prior to optional height-doubling)
	uint16_t height = 0;

	// If true, the rendered image is doubled horizontally after via
	// a scaler (e.g. to achieve pixel-doubling)
	bool double_width = false;

	// If true, the rendered image is doubled vertically via a
	// scaler (e.g. to achieve fake double-scanning)
	bool double_height = false;

	// If true, the image is stored flipped vertically, starting from the
	// bottom row
	bool is_flipped_vertically = false;

	// Pixel aspect ratio to be applied to the final image (post
	// width & height doubling) so it appears as intended on screen.
	// (video_mode.pixel_aspect_ratio holds the "nominal" pixel
	// aspect ratio of the source video mode)
	Fraction pixel_aspect_ratio = {};

	// Pixel format of the image data (see `bpp` in vga.h for details)
	uint8_t bits_per_pixel = 0;

	// Bytes per row
	uint16_t pitch = 0;

	// (width * height) number of pixels stored in the pixel format defined
	// by bits_per_pixel
	uint8_t* image_data = nullptr;

	// Pointer to a (256 * 4) byte long palette data, stored as 8-bit RGB
	// values with 1 extra padding byte per entry (R0, G0, B0, X0, R1, G1,
	// B1, X1, etc.)
	uint8_t* palette_data = nullptr;

	inline bool is_paletted() const
	{
		return (bits_per_pixel == 8);
	}

	RenderedImage deep_copy() const
	{
		RenderedImage copy = *this;

		// Deep-copy image and palette data
		const auto image_data_num_bytes = static_cast<uint32_t>(height * pitch);

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
		delete[] image_data;
		image_data = nullptr;

		delete[] palette_data;
		palette_data = nullptr;
	}
};

extern Render_t render;
extern ScalerLineHandler_t RENDER_DrawLine;

std::deque<std::string> RENDER_InventoryShaders();

void RENDER_SetSize(const uint16_t width, const uint16_t height,
                    const bool double_width, const bool double_height,
                    const Fraction& render_pixel_aspect_ratio,
                    const uint8_t bits_per_pixel,
                    const double frames_per_second, const VideoMode& video_mode);

bool RENDER_StartUpdate(void);
void RENDER_EndUpdate(bool abort);
void RENDER_InitShaderSource([[maybe_unused]] Section* sec);
void RENDER_SetPal(uint8_t entry, uint8_t red, uint8_t green, uint8_t blue);

#if C_OPENGL
bool RENDER_UseSrgbTexture();
bool RENDER_UseSrgbFramebuffer();
#endif

bool RENDER_IsVgaSingleScanningForced();
bool RENDER_IsNoPixelDoublingForced();

#endif
