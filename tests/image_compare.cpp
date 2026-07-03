// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "image_compare.h"

#include <cstdio>
#include <system_error>

#include <gtest/gtest.h>
#include <png.h>

#include "utils/string_utils.h"

constexpr auto NumRgbChannels = 3;

bool DecodePng(const std_fs::path& path, DecodedImage& out)
{
	FILE* fp = fopen(path.string().c_str(), "rb");
	if (!fp) {
		return false;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	                                             nullptr,
	                                             nullptr,
	                                             nullptr);
	if (!png_ptr) {
		fclose(fp);
		return false;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		fclose(fp);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		fclose(fp);
		return false;
	}

	png_init_io(png_ptr, fp);
	png_read_info(png_ptr, info_ptr);

	const auto width  = png_get_image_width(png_ptr, info_ptr);
	const auto height = png_get_image_height(png_ptr, info_ptr);

	png_set_palette_to_rgb(png_ptr);
	png_set_expand(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	const auto channels = png_get_channels(png_ptr, info_ptr);
	if (channels < NumRgbChannels) {
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		fclose(fp);
		return false;
	}

	const auto row_bytes = png_get_rowbytes(png_ptr, info_ptr);

	std::vector<uint8_t> raw(row_bytes * height);
	std::vector<png_bytep> rows(height);

	for (size_t i = 0; i < height; ++i) {
		rows[i] = raw.data() + i * row_bytes;
	}
	png_read_image(png_ptr, rows.data());
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	fclose(fp);

	// Convert to flat RGB (drop alpha if present).
	out.width  = static_cast<int>(width);
	out.height = static_cast<int>(height);

	out.rgb.resize(static_cast<size_t>(width) * height * NumRgbChannels);

	for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
		out.rgb[i * NumRgbChannels + 0] = raw[i * channels + 0];
		out.rgb[i * NumRgbChannels + 1] = raw[i * channels + 1];
		out.rgb[i * NumRgbChannels + 2] = raw[i * channels + 2];
	}
	return true;
}

bool EncodePng(const std_fs::path& path, const DecodedImage& image)
{
	if (image.width <= 0 || image.height <= 0 ||
	    image.rgb.size() != static_cast<size_t>(image.width) *
	                                image.height * NumRgbChannels) {
		return false;
	}

	FILE* fp = fopen(path.string().c_str(), "wb");
	if (!fp) {
		return false;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                              nullptr,
	                                              nullptr,
	                                              nullptr);
	if (!png_ptr) {
		fclose(fp);
		return false;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, nullptr);
		fclose(fp);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		return false;
	}

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr,
	             info_ptr,
	             static_cast<png_uint_32>(image.width),
	             static_cast<png_uint_32>(image.height),
	             8,
	             PNG_COLOR_TYPE_RGB,
	             PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	const auto row_bytes = static_cast<size_t>(image.width) * NumRgbChannels;

	for (int y = 0; y < image.height; ++y) {
		// png_write_row takes a non-const pointer but does not
		// modify the row data
		png_write_row(png_ptr,
		              const_cast<uint8_t*>(image.rgb.data() +
		                                   static_cast<size_t>(y) * row_bytes));
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	return true;
}

float ComparePixels(const DecodedImage& a, const DecodedImage& b)
{
	if (a.width != b.width || a.height != b.height) {
		return 1.0f;
	}

	size_t diff_pixels = 0;

	const auto pixel_count = static_cast<size_t>(a.width) * a.height;

	for (size_t i = 0; i < pixel_count; ++i) {
		if (a.rgb[i * NumRgbChannels + 0] != b.rgb[i * NumRgbChannels + 0] ||
		    a.rgb[i * NumRgbChannels + 1] != b.rgb[i * NumRgbChannels + 1] ||
		    a.rgb[i * NumRgbChannels + 2] != b.rgb[i * NumRgbChannels + 2]) {

			++diff_pixels;
		}
	}

	return static_cast<float>(diff_pixels) / pixel_count;
}

void ExpectMatchesReference(const DecodedImage& actual,
                            const std_fs::path& expected_dir,
                            const std::string& reference_name, const float tolerance)
{
	const auto ref_path = expected_dir / (reference_name + ".png");

	const char* FgRed = "\033[31m";
	const char* Reset = "\033[0m";

	std::error_code ec;
	if (!std_fs::exists(ref_path, ec)) {
		std_fs::create_directories(ref_path.parent_path(), ec);

		ASSERT_TRUE(EncodePng(ref_path, actual));

		SCOPED_TRACE(format_str("%sreference PNG missing — wrote new candidate to %s%s",
		                        FgRed,
		                        ref_path.string().c_str(),
		                        Reset));

		FAIL();
		return;
	}

	DecodedImage reference = {};
	ASSERT_TRUE(DecodePng(ref_path, reference))
	        << "failed to decode reference PNG " << ref_path.string();

	const auto diff = ComparePixels(actual, reference);
	SCOPED_TRACE(format_str("reference=%s diff=%.4f%% tolerance=%.4f%%",
	                        reference_name.c_str(),
	                        diff * 100.0f,
	                        tolerance * 100.0f));

	if (diff > tolerance) {
		const auto actual_path = ref_path.parent_path() /
		                         (ref_path.filename().string() + "-ACTUAL.png");

		ASSERT_TRUE(EncodePng(actual_path, actual));

		SCOPED_TRACE(format_str("%sexpected and actual images differ — wrote actual to %s%s",
		                        FgRed,
		                        actual_path.string().c_str(),
		                        Reset));

		FAIL();
		return;
	}

	EXPECT_LE(diff, tolerance);
}

void RemoveStaleActualImages(const std_fs::path& expected_dir)
{
	std::error_code ec;
	if (!std_fs::is_directory(expected_dir, ec)) {
		return;
	}

	for (const auto& entry : std_fs::directory_iterator(expected_dir, ec)) {
		if (!entry.is_regular_file()) {
			continue;
		}
		if (entry.path().filename().string().ends_with("-ACTUAL.png")) {
			std_fs::remove(entry.path(), ec);
		}
	}
}
