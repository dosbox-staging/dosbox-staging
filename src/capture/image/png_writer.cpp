/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
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

#include "png_writer.h"

#include "checks.h"
#include "string_utils.h"
#include "support.h"
#include "version.h"

#include <zlib.h>

CHECK_NARROWING();

PngWriter::~PngWriter()
{
	FinalisePng();

	if (png_ptr && png_info_ptr) {
		png_destroy_write_struct(&png_ptr, &png_info_ptr);
	}
	png_ptr      = nullptr;
	png_info_ptr = nullptr;
}

bool PngWriter::InitRgb888(FILE* fp, const uint16_t width, const uint16_t height,
                           const Fraction& pixel_aspect_ratio,
                           const VideoMode& video_mode)
{
	if (!Init(fp)) {
		return false;
	}

	constexpr auto is_paletted  = false;
	constexpr auto palette_data = nullptr;
	WritePngInfo(width, height, pixel_aspect_ratio, video_mode, is_paletted, palette_data);
	return true;
}

bool PngWriter::InitIndexed8(FILE* fp, const uint16_t width, const uint16_t height,
                             const Fraction& pixel_aspect_ratio,
                             const VideoMode& video_mode, const uint8_t* palette_data)
{
	if (!Init(fp)) {
		return false;
	}

	constexpr auto is_paletted = true;
	WritePngInfo(width, height, pixel_aspect_ratio, video_mode, is_paletted, palette_data);
	return true;
}

bool PngWriter::Init(FILE* fp)
{
	// Initialise PNG writer
	const png_voidp error_ptr    = nullptr;
	const png_error_ptr error_fn = nullptr;
	const png_error_ptr warn_fn  = nullptr;
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                  error_ptr,
	                                  error_fn,
	                                  warn_fn);
	if (!png_ptr) {
		LOG_ERR("PNG: Error initialising PNG library");
		return false;
	}

	SetPngCompressionsParams();

	png_init_io(png_ptr, fp);

	// Write headers and extra metadata
	png_info_ptr = png_create_info_struct(png_ptr);
	if (!png_info_ptr) {
		LOG_ERR("PNG: Error initialising PNG library");
		png_destroy_write_struct(&png_ptr, (png_infopp) nullptr);
		return false;
	}

	return true;
}

void PngWriter::SetPngCompressionsParams()
{
	assert(png_ptr);

	// Default compression (equal to level 6) is the sweet spot between
	// speed and compression. Z_BEST_COMPRESSION (level 9) rarely results in
	// smaller file sizes, but makes the compression significantly slower
	// (by several folds).
	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);

	// Larger buffer sizes (e.g. 64K or 128K) could significantly speed up
	// decompression, but not compression.
	constexpr auto default_buffer_size = 8192;
	png_set_compression_buffer_size(png_ptr, default_buffer_size);

	// The "fast" filters are not only the fastest, but also result in the
	// best compression ratios on average.
	constexpr auto default_filter_method = 0;
	png_set_filter(png_ptr, default_filter_method, PNG_ALL_FILTERS);

	// Do not change the below settings; they are parameters for the zlib
	// compression library and changing them might result in invalid PNG
	// files.
	constexpr auto default_mem_level = 8;
	png_set_compression_mem_level(png_ptr, default_mem_level);

	constexpr auto default_window_bits = 15;
	png_set_compression_window_bits(png_ptr, default_window_bits);

	png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
	png_set_compression_method(png_ptr, Z_DEFLATED);
}

void PngWriter::WritePngInfo(const uint16_t width, const uint16_t height,
                             const Fraction& pixel_aspect_ratio,
                             const VideoMode& video_mode,
                             const bool is_paletted, const uint8_t* palette_data)
{
	assert(png_ptr);
	assert(png_info_ptr);

	constexpr auto png_bit_depth = 8;
	const auto png_color_type    = is_paletted ? PNG_COLOR_TYPE_PALETTE
	                                           : PNG_COLOR_TYPE_RGB;
	png_set_IHDR(png_ptr,
	             png_info_ptr,
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
		png_set_PLTE(png_ptr, png_info_ptr, palette, NumPaletteEntries);
	}

#ifdef PNG_gAMA_SUPPORTED
	// It's not strictly necessary to write this chunk, but it's recommended
	// by the spec.
	png_set_gAMA(png_ptr, png_info_ptr, 1 / 2.2);
#endif

#ifdef PNG_pHYs_SUPPORTED
	// "The pHYs chunk specifies the intended pixel size or aspect ratio for
	// display of the image."
	//
	// "If the pHYs chunk is not present, pixels are assumed to be square,
	// and the physical size of each pixel is unspecified."
	//
	// Although as of now pretty much all programs ignore the pHYs chunk and
	// simply assume square pixels, we're writing the correct pixel aspect
	// ratio in the hope that in the future applications will handle the
	// pHYs chunk appropriately.
	//
	// Source:
	//   Portable Network Graphics (PNG) Specification (Second Edition)
	//   https://www.w3.org/TR/2003/REC-PNG-20031110/#11pHYs)
	//
	const uint32_t pixels_per_unit_x = static_cast<uint32_t>(
	        pixel_aspect_ratio.Num());

	const uint32_t pixels_per_unit_y = static_cast<uint32_t>(
	        pixel_aspect_ratio.Denom());

	// "When the unit specifier is 0, the pHYs chunk defines pixel aspect
	// ratio only; the actual size of the pixels remains unspecified."
	auto unit_type = 0;

	png_set_pHYs(png_ptr, png_info_ptr, pixels_per_unit_x, pixels_per_unit_y, unit_type);
#endif

#ifdef PNG_TEXT_SUPPORTED
	constexpr int MaxNumText   = 2;
	png_text texts[MaxNumText] = {};

	char software_keyword[] = "Software";
	static_assert(sizeof(software_keyword) < 80, "libpng limit");
	char software_value[] = DOSBOX_PROJECT_NAME " " DOSBOX_VERSION;

	texts[0].compression = PNG_TEXT_COMPRESSION_NONE;
	texts[0].key         = static_cast<png_charp>(software_keyword);
	texts[0].text        = static_cast<png_charp>(software_value);
	texts[0].text_length = sizeof(software_value);

	auto num_text = 1;

	char source_keyword[] = "Source";
	static_assert(sizeof(source_keyword) < 80, "libpng limit");

	const auto source_value = format_str(
	        "source resolution: %dx%d; source pixel aspect ratio: %d:%d (1:%1.6f)",
	        video_mode.width,
	        video_mode.height,
	        video_mode.pixel_aspect_ratio.Num(),
	        video_mode.pixel_aspect_ratio.Denom(),
	        video_mode.pixel_aspect_ratio.Inverse().ToDouble());

	texts[1].compression = PNG_TEXT_COMPRESSION_NONE;
	texts[1].key         = static_cast<png_charp>(source_keyword);
	texts[1].text        = const_cast<png_charp>(source_value.c_str());
	texts[1].text_length = source_value.size();

	++num_text;

	png_set_text(png_ptr, png_info_ptr, texts, num_text);
#endif

	png_write_info(png_ptr, png_info_ptr);
}

void PngWriter::WriteRow(std::vector<uint8_t>::const_iterator row)
{
	assert(png_ptr);
	png_write_row(png_ptr, const_cast<png_bytep>(&*row));
}

void PngWriter::FinalisePng()
{
	assert(png_ptr);

	const png_infop end_info_ptr = nullptr;
	png_write_end(png_ptr, end_info_ptr);
}

