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
#include <cstdlib>
#include <cstring>

#include "capture_image.h"

#include "capture.h"
#include "support.h"

#if (C_SSHOT)
#include <png.h>
#include <zlib.h>

ImageCapturer::~ImageCapturer()
{
	Close();
}

void ImageCapturer::Open()
{
	if (is_open) {
		Close();
	}

	const auto worker_function = std::bind(&ImageCapturer::SaveQueuedImages, this);
	renderer = std::thread(worker_function);
	set_thread_name(renderer, "dosbox:imgcap");

	LOG_MSG("CAPTURE: Image capturer started");

	is_open = true;
}

void ImageCapturer::Close()
{
	if (!is_open) {
		return;
	}

	LOG_MSG("CAPTURE: Image capturer shutting down");

	image_fifo.Stop();

	// Wait for the worker thread to finish
	if (renderer.joinable()) {
		renderer.join();
	}

	is_open = false;
}

void ImageCapturer::CaptureImage(const RenderedImage_t image)
{
	if (!image_fifo.IsRunning()) {
		LOG_WARNING("CAPTURE: Cannot create screenshots while image capturer is shutting down");
		return;
	}

	RenderedImage_t copied_image = image;

	// Deep-copy image and palette data
	// TODO it's bad that we need to calculate the image data size
	// downstream...
	const auto image_data_num_bytes = image.height * image.pitch;

	copied_image.image_data = static_cast<uint8_t*>(
	        std::malloc(image_data_num_bytes));
	assert(copied_image.image_data);

	assert(image.image_data);
	std::memcpy(const_cast<uint8_t*>(copied_image.image_data),
	            image.image_data,
	            image_data_num_bytes);

	// TODO it's bad that we need to make this assumption downstream on
	// the size and alignment of the palette...
	if (image.palette_data) {
		constexpr auto PaletteNumBytes = 256 * 4;

		copied_image.palette_data = static_cast<uint8_t*>(
		        std::malloc(PaletteNumBytes));
		assert(copied_image.palette_data);

		std::memcpy(const_cast<uint8_t*>(copied_image.palette_data),
		            image.palette_data,
		            PaletteNumBytes);
	}

	image_fifo.Enqueue(std::move(copied_image));
}

void ImageCapturer::SaveQueuedImages()
{
	while (auto image_opt = image_fifo.Dequeue()) {
		auto image = *image_opt;
		SavePng(image);

		if (image.image_data) {
			std::free(const_cast<uint8_t*>(image.image_data));
		}
		if (image.palette_data) {
			std::free(const_cast<uint8_t*>(image.palette_data));
		}
	}
}

void ImageCapturer::SavePng(const RenderedImage_t image)
{
	image_scaler.Init(image);

	FILE* fp = CAPTURE_CreateFile("raw image", ".png");
	if (!fp) {
		return;
	}

	// Initialise PNG writer
	const png_voidp error_ptr    = nullptr;
	const png_error_ptr error_fn = nullptr;
	const png_error_ptr warn_fn  = nullptr;
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                  error_ptr,
	                                  error_fn,
	                                  warn_fn);
	if (!png_ptr) {
		LOG_ERR("CAPTURE: Error initialising PNG library for image capture");
		fclose(fp);
		return;
	}

	SetPngCompressionsParams();

	png_init_io(png_ptr, fp);

	// Write headers and extra metadata
	png_info_ptr = png_create_info_struct(png_ptr);
	if (!png_info_ptr) {
		LOG_ERR("CAPTURE: Error initialising PNG library for image capture");
		png_destroy_write_struct(&png_ptr, (png_infopp) nullptr);
		fclose(fp);
		return;
	}

	bool out_is_paletted = (image_scaler.GetOutputPixelFormat() ==
	                        PixelFormat::Indexed8);

	WritePngInfo(image_scaler.GetOutputWidth(),
	             image_scaler.GetOutputHeight(),
	             out_is_paletted,
	             image.palette_data);

	// Write image data
	auto rows_to_write = image_scaler.GetOutputHeight();
	while (rows_to_write--) {
		auto row = image_scaler.GetNextOutputRow();
		png_write_row(png_ptr, &*row);
	}

	// We've already written the metadata information to the start of the file
	const png_infop end_info_ptr = nullptr;
	png_write_end(png_ptr, end_info_ptr);

	png_destroy_write_struct(&png_ptr, &png_info_ptr);
	png_ptr      = nullptr;
	png_info_ptr = nullptr;

	fclose(fp);
}

void ImageCapturer::SetPngCompressionsParams()
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

	// TODO compare with PNG_NO_FILTERS, PNG_FILTER_PAETH, and
	// PNG_ALL_FILTERS
	// TODO turn off filtering for paletted data?
	constexpr auto default_filter_method = 0;
	png_set_filter(png_ptr, default_filter_method, PNG_FAST_FILTERS);

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

void ImageCapturer::WritePngInfo(const uint16_t width, const uint16_t height,
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

	png_set_text(png_ptr, png_info_ptr, texts, num_text);
#endif

	png_write_info(png_ptr, png_info_ptr);
}

#endif

