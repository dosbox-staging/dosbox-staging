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

static inline bool is_paletted(const uint8_t bits_per_pixel)
{
	return (bits_per_pixel == 8);
}

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

	constexpr auto png_bit_depth = 8;

	if (is_paletted(bits_per_pixel)) {
		png_set_IHDR(png_ptr,
		             info_ptr,
		             width,
		             height,
		             png_bit_depth,
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
		             png_bit_depth,
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

static inline Rgb888 decode_rgb_pixel(const uint8_t bits_per_pixel, const uint8_t* data)
{
	Rgb888 pixel = {};

	switch (bits_per_pixel) {
	case 15: { // RGB555
		const auto p = host_to_le(*reinterpret_cast<const uint16_t*>(data));
		pixel = Rgb555(p).ToRgb888();
	} break;

	case 16: { // RGB565
		const auto p = host_to_le(*reinterpret_cast<const uint16_t*>(data));
		pixel = Rgb565(p).ToRgb888();
	} break;

	case 24:   // RGB888
	case 32: { // XRGB8888
		const auto b = *data;
		const auto g = *(data + 1);
		const auto r = *(data + 2);

		pixel = {r, g, b};
	} break;
	}
	return pixel;
}

inline static const uint8_t* next_pixel(const uint8_t bits_per_pixel,
                                        const uint8_t* curr)
{
	auto next = curr;
	switch (bits_per_pixel) {
	case 8: // Indexed8
		++next;
	case 15: // RGB555
	case 16: // RGB565
		next += 2;
		break;
	case 24: // RGB888
		next += 3;
		break;
	case 32: // XRGB8888
		next += 4;
		break;
	}
	return next;
}

static void decode_row_to_linear_rgb(const uint16_t width,
                                     const uint8_t bits_per_pixel,
                                     const uint8_t* image_data,
                                     const uint8_t* palette_data,
                                     std::vector<float>::iterator out)
{
	const uint8_t* in = image_data;
	Rgb888 pixel      = {};

	for (auto x = 0; x < width; ++x) {
		if (is_paletted(bits_per_pixel)) {
			const auto pal_index = *in;

			const auto r = palette_data[pal_index * 4 + 0];
			const auto g = palette_data[pal_index * 4 + 1];
			const auto b = palette_data[pal_index * 4 + 2];

			pixel = {r, g, b};
		} else {
			pixel = decode_rgb_pixel(bits_per_pixel, in);
		}

		*out++ = srgb8_to_linear_lut(pixel.red);
		*out++ = srgb8_to_linear_lut(pixel.green);
		*out++ = srgb8_to_linear_lut(pixel.blue);

		in = next_pixel(bits_per_pixel, in);
	}
}

static void write_png_image_data_paletted(const png_structp png_ptr,
                                          const uint16_t width,
                                          const uint16_t height, const uint16_t pitch,
                                          const uint8_t* image_data)
{
	static std::vector<uint8_t> row_buffer = {};
	row_buffer.resize(width);

	auto src_row = image_data;

	for (auto y = 0; y < height; ++y) {
		auto out = row_buffer.begin();

		for (auto x = 0; x < width; ++x) {
			const auto pixel = src_row[x];

			*out++ = pixel;
		}

		png_write_row(png_ptr, row_buffer.data());
		src_row += pitch;
	}
}

static void write_png_image_data_rgb888(const png_structp png_ptr,
                                        const uint16_t width, const uint16_t height,
                                        const uint8_t bits_per_pixel,
                                        const uint16_t pitch,
                                        const uint8_t* image_data)
{
	assert(bits_per_pixel != 8);

	static std::vector<uint8_t> row_buffer = {};
	constexpr auto ComponentsPerPixel      = 3;
	row_buffer.resize(width * ComponentsPerPixel);

	auto src_row = image_data;

	for (auto y = 0; y < height; ++y) {
		auto out = row_buffer.begin();
		auto in  = src_row;

		for (auto x = 0; x < width; ++x) {
			const auto pixel = decode_rgb_pixel(bits_per_pixel, in);

			*out++ = pixel.red;
			*out++ = pixel.green;
			*out++ = pixel.blue;

			in = next_pixel(bits_per_pixel, in);
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

	if (is_paletted(bits_per_pixel)) {
		write_png_image_data_paletted(png_ptr, width, height, pitch, image_data);
	} else {
		write_png_image_data_rgb888(
		        png_ptr, width, height, bits_per_pixel, pitch, image_data);
	}

	png_write_end(png_ptr, nullptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
}

#endif

