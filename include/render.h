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

struct Render_t {
	struct {
		uint32_t width              = 0;
		uint32_t start              = 0;
		uint32_t height             = 0;
		bool double_width           = false;
		bool double_height          = false;
		Fraction pixel_aspect_ratio = {};
		uint8_t bpp                 = 0;
		double fps                  = 0;
	} src = {};

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
		std::string filename      = {};
		std::string source        = {};
		bool use_srgb_texture     = false;
		bool use_srgb_framebuffer = false;
	} shader = {};
#endif

	RenderPal_t pal = {};

	bool updating  = false;
	bool active    = false;
	bool aspect    = true;
	bool fullFrame = true;
};

struct RenderedImage {
	uint16_t width              = 0;
	uint16_t height             = 0;
	bool double_width           = false;
	bool double_height          = false;
	bool flip_vertical          = false;
	Fraction pixel_aspect_ratio = {};
	uint8_t bits_per_pixel      = 0;
	uint16_t pitch              = 0;
	uint8_t* image_data         = nullptr;
	uint8_t* palette_data       = nullptr;

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
		std::memcpy(copy.image_data,
		            image_data,
		            image_data_num_bytes);

		// TODO it's bad that we need to make this assumption downstream
		// on the size and alignment of the palette...
		if (palette_data) {
			constexpr uint16_t PaletteNumBytes = 256 * 4;
			copy.palette_data = new uint8_t[PaletteNumBytes];

			std::memcpy(copy.palette_data,
			            palette_data,
			            PaletteNumBytes);
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

void RENDER_SetSize(const uint32_t width, const uint32_t height,
                    const bool double_width, const bool double_height,
                    const Fraction& pixel_aspect_ratio,
                    const unsigned bits_per_pixel, const double frames_per_second);

bool RENDER_StartUpdate(void);
void RENDER_EndUpdate(bool abort);
void RENDER_InitShaderSource([[maybe_unused]] Section* sec);
void RENDER_SetPal(uint8_t entry, uint8_t red, uint8_t green, uint8_t blue);

#if C_OPENGL
bool RENDER_UseSrgbTexture();
bool RENDER_UseSrgbFramebuffer();
#endif

#endif
