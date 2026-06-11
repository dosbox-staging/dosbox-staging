// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer_png.h"

#include <array>
#include <cstdio>
#include <vector>

#include <png.h>

#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace VirtualPrinter {

bool write_png_page(PageBitmap& page, const std_fs::path& out_path)
{
	// Open the output file. RAII closes on any return path.
	FILE_unique_ptr fp{fopen(out_path.string().c_str(), "wb")};
	if (!fp) {
		LOG_ERR("PRINTER: Can't open file %s for printer output",
		        out_path.string().c_str());
		return false;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                              nullptr,
	                                              nullptr,
	                                              nullptr);
	if (!png_ptr) {
		return false;
	}
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, static_cast<png_infopp>(nullptr));
		return false;
	}

	png_init_io(png_ptr, fp.get());
	png_set_compression_level(png_ptr, ZBestCompression);
	png_set_compression_mem_level(png_ptr, 8);
	png_set_compression_strategy(png_ptr, ZDefaultStrategy);
	png_set_compression_window_bits(png_ptr, 15);
	png_set_compression_method(png_ptr, 8);
	png_set_compression_buffer_size(png_ptr, 8192);

	png_set_IHDR(png_ptr,
	             info_ptr,
	             page.width,
	             page.height,
	             8,
	             PNG_COLOR_TYPE_PALETTE,
	             PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);

	std::array<png_color, 256> palette = {};
	for (int i = 0; i < 256; i++) {
		const auto& c = page.palette[i];
		palette[i].red   = c.r;
		palette[i].green = c.g;
		palette[i].blue  = c.b;
	}
	png_set_PLTE(png_ptr, info_ptr, palette.data(), 256);

	std::vector<png_bytep> row_pointers(page.height);
	for (int i = 0; i < page.height; i++) {
		row_pointers[i] = page.pixels.data() + (i * page.pitch);
	}

	png_set_rows(png_ptr, info_ptr, row_pointers.data());
	png_write_png(png_ptr, info_ptr, 0, nullptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);
	// fp closes here via FILE_unique_ptr destructor.
	return true;
}

} // namespace VirtualPrinter
