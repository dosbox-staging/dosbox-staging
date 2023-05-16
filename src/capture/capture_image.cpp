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

#include "byteorder.h"
#include "capture.h"
#include "rgb.h"
#include "rgb555.h"
#include "rgb565.h"
#include "rgb888.h"

#if (C_SSHOT)
#include <png.h>
#include <zlib.h>

static void write_png_info(const png_structp png_ptr, const png_infop info_ptr,
                           const uint16_t width, const uint16_t height,
                           const uint8_t bits_per_pixel, const uint8_t* palette_data)
{
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

	// TODO document magic values
	png_set_compression_mem_level(png_ptr, 8);
	png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
	png_set_compression_window_bits(png_ptr, 15);
	png_set_compression_method(png_ptr, 8);
	png_set_compression_buffer_size(png_ptr, 8192);

	constexpr auto PngBitDepth = 8;

	if (bits_per_pixel == 8) {
		png_set_IHDR(png_ptr,
		             info_ptr,
		             width,
		             height,
		             PngBitDepth,
		             PNG_COLOR_TYPE_PALETTE,
		             PNG_INTERLACE_NONE,
		             PNG_COMPRESSION_TYPE_DEFAULT,
		             PNG_FILTER_TYPE_DEFAULT);

		constexpr auto NumPaletteEntries     = 256;
		png_color palette[NumPaletteEntries] = {};

		for (auto i = 0; i < NumPaletteEntries; ++i) {
			palette[i].red   = palette_data[i * 4 + 0];
			palette[i].green = palette_data[i * 4 + 1];
			palette[i].blue  = palette_data[i * 4 + 2];
		}
		png_set_PLTE(png_ptr, info_ptr, palette, NumPaletteEntries);

	} else {
		png_set_IHDR(png_ptr,
		             info_ptr,
		             width,
		             height,
		             PngBitDepth,
		             PNG_COLOR_TYPE_RGB,
		             PNG_INTERLACE_NONE,
		             PNG_COMPRESSION_TYPE_DEFAULT,
		             PNG_FILTER_TYPE_DEFAULT);
	}

#ifdef PNG_TEXT_SUPPORTED
	char keyword[] = "Software";
	static_assert(sizeof(keyword) < 80, "libpng limit");

	char value[]           = CANONICAL_PROJECT_NAME " " VERSION;
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

static void decode_row_to_linear_rgb(const uint16_t width,
                                     const uint8_t bits_per_pixel,
                                     const uint8_t* image_data,
                                     const uint8_t* palette_data, float* out)
{
	for (auto x = 0; x < width; ++x) {
		Rgb888 pixel = {};

		switch (bits_per_pixel) {
		// Indexed8
		case 8: {
			const auto pal_index = image_data[x];

			const auto r = palette_data[pal_index * 4 + 0];
			const auto g = palette_data[pal_index * 4 + 1];
			const auto b = palette_data[pal_index * 4 + 2];

			pixel = {r, g, b};
		} break;

		// RGB555
		case 15: {
			const auto p = host_to_le(
			        reinterpret_cast<const uint16_t*>(image_data)[x]);

			pixel = Rgb555(p).ToRgb888();
		} break;

		// RGB565
		case 16: {
			const auto p = host_to_le(
			        reinterpret_cast<const uint16_t*>(image_data)[x]);

			pixel = Rgb565(p).ToRgb888();
		} break;

		// RGB888
		case 24: {
			const auto b = image_data[x * 3 + 0];
			const auto g = image_data[x * 3 + 1];
			const auto r = image_data[x * 3 + 2];

			pixel = {r, g, b};
		} break;

		// XRGB8888
		case 32: {
			const auto b = image_data[x * 4 + 0];
			const auto g = image_data[x * 4 + 1];
			const auto r = image_data[x * 4 + 2];

			pixel = {r, g, b};
		} break;
		}

		*out++ = srgb8_to_linear_lut(pixel.red);
		*out++ = srgb8_to_linear_lut(pixel.green);
		*out++ = srgb8_to_linear_lut(pixel.blue);
	}
}

static void write_png_image_data(const png_structp png_ptr, const uint16_t width,
                                 const uint16_t height, const uint8_t bits_per_pixel,
                                 const uint16_t pitch, const uint8_t* image_data)
{
	static std::vector<uint8_t> row_buffer = {};

	constexpr auto MaxBytesPerPixel = 4;
	row_buffer.resize(width * MaxBytesPerPixel);

	auto src_row = image_data;

	for (auto y = 0; y < height; ++y) {
		for (auto x = 0; x < width; ++x) {
			switch (bits_per_pixel) {
			// Indexed8
			case 8: {
				const auto pixel = src_row[x];

				row_buffer[x] = pixel;
			} break;

			// RGB555
			case 15: {
				const auto p = host_to_le(
				        reinterpret_cast<const uint16_t*>(src_row)[x]);

				const auto pixel = Rgb555(p).ToRgb888();

				row_buffer[x * 3 + 0] = pixel.red;
				row_buffer[x * 3 + 1] = pixel.green;
				row_buffer[x * 3 + 2] = pixel.blue;
			} break;

			// RGB565
			case 16: {
				const auto p = host_to_le(
				        reinterpret_cast<const uint16_t*>(src_row)[x]);

				const auto pixel = Rgb565(p).ToRgb888();

				row_buffer[x * 3 + 0] = pixel.red;
				row_buffer[x * 3 + 1] = pixel.green;
				row_buffer[x * 3 + 2] = pixel.blue;
			} break;

			// RGB888
			case 24: {
				const auto b = src_row[x * 3 + 0];
				const auto g = src_row[x * 3 + 1];
				const auto r = src_row[x * 3 + 2];

				row_buffer[x * 3 + 0] = r;
				row_buffer[x * 3 + 1] = g;
				row_buffer[x * 3 + 2] = b;
			} break;

			// XRGB8888
			case 32: {
				const auto b = src_row[x * 4 + 0];
				const auto g = src_row[x * 4 + 1];
				const auto r = src_row[x * 4 + 2];

				row_buffer[x * 3 + 0] = r;
				row_buffer[x * 3 + 1] = g;
				row_buffer[x * 3 + 2] = b;
			} break;
			}
		}

		png_write_row(png_ptr, row_buffer.data());
		src_row += pitch;
	}
}

uint16_t calc_vertical_scale_factor(const uint16_t height)
{
	constexpr auto target_height = 1200.0f;
	// Slight fudge factor to push 350-line modes into 4x vertical scaling
	constexpr auto fudge_offset = 0.1;

	return round(target_height / height + fudge_offset);
}

void capture_image(const uint16_t width, const uint16_t height,
                   const uint8_t bits_per_pixel, const uint16_t pitch,
                   const uint8_t capture_flags, const float one_per_pixel_aspect_ratio,
                   const uint8_t* image_data, const uint8_t* palette_data)
{
	const bool is_double_width  = (capture_flags & CaptureFlagDoubleWidth);
	const bool is_double_height = (capture_flags & CaptureFlagDoubleHeight);

	const auto vert_scale  = calc_vertical_scale_factor(height);
	const auto horiz_scale = vert_scale / one_per_pixel_aspect_ratio;

	const auto scaled_height = vert_scale *
	                           (is_double_height ? height * 2 : height);

	const auto scaled_width = static_cast<uint16_t>(
	        (is_double_width ? width * 2 : width) * horiz_scale);

	LOG_MSG("CAPTURE: Capturing image\n"
	        "    width:         %8d\n"
	        "    height:        %8d\n"
	        "    bits_per_pixel:%8d\n"
	        "    pitch:         %8d\n"
	        "    double_width:  %8s\n"
	        "    double_height: %8s\n"
	        "    1/PAR:         %8f\n"
	        "    -----------------------\n"
	        "    scaled_width:  %8d\n"
	        "    scaled_height: %8d\n"
	        "    horiz_scale:   %8f\n"
	        "    vert_scale:    %8d",
	        width,
	        height,
	        bits_per_pixel,
	        pitch,
	        is_double_width ? "yes" : "no",
	        is_double_height ? "yes" : "no",
	        one_per_pixel_aspect_ratio,
	        scaled_width,
	        scaled_height,
	        horiz_scale,
	        vert_scale);

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

	write_png_info(png_ptr, info_ptr, width, height, bits_per_pixel, palette_data);

	write_png_image_data(png_ptr, width, height, bits_per_pixel, pitch, image_data);

	png_write_end(png_ptr, nullptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
}

#endif

