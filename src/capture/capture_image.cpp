/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include <cassert>
#include <vector>

#include "capture.h"
#include "image_scaler.h"

#if (C_SSHOT)
#include <png.h>
#include <zlib.h>

ImageScaler image_scaler = {};

static void write_png_info(const png_structp png_ptr, const png_infop info_ptr,
                           const uint16_t width, const uint16_t height,
                           const bool is_paletted, const uint8_t* palette_data)
{
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

	// TODO document magic values
	png_set_compression_mem_level(png_ptr, 8);
	png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
	png_set_compression_window_bits(png_ptr, 15);
	png_set_compression_method(png_ptr, 8);
	png_set_compression_buffer_size(png_ptr, 8192);

	constexpr auto png_bit_depth = 8;

	const auto png_color_type = is_paletted ? PNG_COLOR_TYPE_PALETTE
	                                        : PNG_COLOR_TYPE_RGB;

	png_set_IHDR(png_ptr,
	             info_ptr,
	             width,
	             height,
	             png_bit_depth,
	             png_color_type,
	             PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);

	if (is_paletted) {
		constexpr auto NumPaletteEntries     = 256;
		png_color palette[NumPaletteEntries] = {};

		for (auto i = 0; i < NumPaletteEntries; ++i) {
			palette[i].red   = palette_data[i * 4 + 0];
			palette[i].green = palette_data[i * 4 + 1];
			palette[i].blue  = palette_data[i * 4 + 2];
		}
		png_set_PLTE(png_ptr, info_ptr, palette, NumPaletteEntries);
	}

#ifdef PNG_TEXT_SUPPORTED
	char keyword[] = "Software";
	static_assert(sizeof(keyword) < 80, "libpng limit");

	char value[] = CANONICAL_PROJECT_NAME " " VERSION;

	constexpr int num_text = 1;

	png_text texts[num_text] = {};

	texts[0].compression = PNG_TEXT_COMPRESSION_NONE;
	texts[0].key         = static_cast<png_charp>(keyword);
	texts[0].text        = static_cast<png_charp>(value);
	texts[0].text_length = sizeof(value);

	png_set_text(png_ptr, info_ptr, texts, num_text);
#endif

	png_write_info(png_ptr, info_ptr);
}

void capture_image(const uint16_t width, const uint16_t height,
                   const uint8_t bits_per_pixel, const uint16_t pitch,
                   const uint8_t capture_flags, const float one_per_pixel_aspect_ratio,
                   const uint8_t* image_data, const uint8_t* palette_data)
{
	const bool is_double_width  = (capture_flags & CaptureFlagDoubleWidth);
	const bool is_double_height = (capture_flags & CaptureFlagDoubleHeight);

	image_scaler_params_t params      = {};
	params.width                      = width;
	params.height                     = height;
	params.bits_per_pixel             = bits_per_pixel;
	params.pitch                      = pitch;
	params.double_width               = is_double_width;
	params.double_height              = is_double_height;
	params.one_per_pixel_aspect_ratio = one_per_pixel_aspect_ratio;
	params.image_data                 = image_data;
	params.palette_data               = palette_data;

	image_scaler.Init(params);

	FILE* fp = CAPTURE_CreateFile("raw image", ".png");
	if (!fp) {
		return;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                              nullptr,
	                                              nullptr,
	                                              nullptr);
	if (!png_ptr) {
		fclose(fp);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp) nullptr);
		fclose(fp);
		return;
	}

	png_init_io(png_ptr, fp);

	bool out_is_paletted = (image_scaler.GetOutputPixelFormat() ==
	                        PixelFormat::Indexed8);

	write_png_info(png_ptr,
	               info_ptr,
	               image_scaler.GetOutputWidth(),
	               image_scaler.GetOutputHeight(),
	               out_is_paletted,
	               palette_data);

	auto rows_to_write = image_scaler.GetOutputHeight();
	while (rows_to_write--) {
		auto row = image_scaler.GetNextOutputRow();
		png_write_row(png_ptr, &*row);
	}

	png_write_end(png_ptr, nullptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);
}

#endif

