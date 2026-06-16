// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// PngWriter golden-file tests.
//
// Drive PngWriter with deterministic inputs (a fixed VideoMode literal, a
// 16x16 pixel buffer, and a known 256-entry palette), write to a temp PNG,
// and compare decoded pixels + key metadata chunks against a checked-in
// reference under tests/png_writer_data/expected/.
//
// Pixel comparison is byte-exact (zero tolerance). Metadata comparison
// covers pHYs (pixel aspect ratio), gAMA (gamma), and tEXt chunks
// (Software, Source). These are the chunks the PngWriter refactor in
// Part 3 of the printer-output consolidation reshuffles, so they're the
// surface that must not regress for the screenshot feature.
//
// Raw byte comparison is deliberately avoided: zlib version drift between
// CI and developer machines would flap the test without indicating any
// real regression.
//
// If a reference is missing, the rendered output is copied to the expected
// location AND the test fails — so a new reference is committed
// deliberately, not silently. Same flow as printer_render_tests.cpp.

#include "capture/image/png_writer.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>
#include <png.h>

#include "hardware/video/vga.h"
#include "misc/std_filesystem.h"
#include "misc/video.h"
#include "utils/fraction.h"
#include "utils/rgb888.h"
#include "utils/string_utils.h"

namespace {

constexpr int TestWidth  = 16;
constexpr int TestHeight = 16;

VideoMode make_test_video_mode()
{
	VideoMode mode = {};

	mode.bios_mode_number   = 0x13;
	mode.is_graphics_mode   = true;
	mode.is_custom_mode     = false;
	mode.width              = 320;
	mode.height             = 200;
	mode.pixel_aspect_ratio = Fraction(6, 5);

	mode.graphics_standard      = GraphicsStandard::Vga;
	mode.color_depth            = ColorDepth::IndexedColor256;
	mode.is_double_scanned_mode = false;
	mode.has_vga_colors         = true;

	return mode;
}

std::array<Rgb888, NumVgaColors> make_test_palette()
{
	std::array<Rgb888, NumVgaColors> palette = {};

	for (int i = 0; i < NumVgaColors; ++i) {
		palette[i] = Rgb888{static_cast<uint8_t>(i),
		                    static_cast<uint8_t>(255 - i),
		                    static_cast<uint8_t>((i * 3) & 0xff)};
	}
	return palette;
}

std::vector<uint8_t> make_test_pixels_indexed8()
{
	std::vector<uint8_t> pixels(static_cast<size_t>(TestWidth) * TestHeight);

	for (int y = 0; y < TestHeight; ++y) {
		for (int x = 0; x < TestWidth; ++x) {
			pixels[y * TestWidth + x] = static_cast<uint8_t>(
			        (x + y * 16) & 0xff);
		}
	}
	return pixels;
}

std::vector<uint8_t> make_test_pixels_rgb888()
{
	std::vector<uint8_t> pixels(static_cast<size_t>(TestWidth) * TestHeight * 3);

	for (int y = 0; y < TestHeight; ++y) {
		for (int x = 0; x < TestWidth; ++x) {
			const auto idx = (y * TestWidth + x) * 3;

			pixels[idx + 0] = static_cast<uint8_t>(x * 16);
			pixels[idx + 1] = static_cast<uint8_t>(y * 16);
			pixels[idx + 2] = static_cast<uint8_t>((x + y) * 8);
		}
	}
	return pixels;
}

// Decoded PNG with the metadata fields the refactor risks dropping.
struct DecodedPng {
	int width    = 0;
	int height   = 0;
	int channels = 0; // 3 for RGB, 3 for palette-expanded RGB

	std::vector<uint8_t> pixels = {}; // flat, post-palette-expansion

	bool has_phys                   = false;
	uint32_t phys_x_pixels_per_unit = 0;
	uint32_t phys_y_pixels_per_unit = 0;
	int phys_unit_type              = 0;

	bool has_gama = false;
	double gama   = 0.0;

	struct TextChunk {
		std::string key  = {};
		std::string text = {};
	};
	std::vector<TextChunk> text_chunks = {};
};

bool decode_png(const std_fs::path& path, DecodedPng& out)
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

	out.width  = static_cast<int>(png_get_image_width(png_ptr, info_ptr));
	out.height = static_cast<int>(png_get_image_height(png_ptr, info_ptr));

	// Expand palette and any low-bit-depth grayscale to RGB so we can
	// compare pixels uniformly.
	png_set_palette_to_rgb(png_ptr);
	png_set_expand(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	out.channels = static_cast<int>(png_get_channels(png_ptr, info_ptr));

	const auto row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	std::vector<uint8_t> raw(row_bytes * out.height);
	std::vector<png_bytep> rows(out.height);
	for (int i = 0; i < out.height; ++i) {
		rows[i] = raw.data() + i * row_bytes;
	}
	png_read_image(png_ptr, rows.data());

	// Read metadata chunks BEFORE destroying the read struct.
	png_uint_32 res_x = 0;
	png_uint_32 res_y = 0;

	int unit_type = 0;

	if (png_get_pHYs(png_ptr, info_ptr, &res_x, &res_y, &unit_type)) {
		out.has_phys               = true;
		out.phys_x_pixels_per_unit = res_x;
		out.phys_y_pixels_per_unit = res_y;
		out.phys_unit_type         = unit_type;
	}

	double gama = 0.0;

	if (png_get_gAMA(png_ptr, info_ptr, &gama)) {
		out.has_gama = true;
		out.gama     = gama;
	}

	png_textp text_ptr = nullptr;
	int num_text       = 0;

	if (png_get_text(png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
		for (int i = 0; i < num_text; ++i) {
			DecodedPng::TextChunk t;

			t.key  = text_ptr[i].key ? text_ptr[i].key : "";
			t.text = text_ptr[i].text ? text_ptr[i].text : "";

			out.text_chunks.push_back(std::move(t));
		}
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	fclose(fp);

	// Drop alpha if libpng expanded into RGBA.
	if (out.channels == 4) {
		std::vector<uint8_t> rgb;
		rgb.reserve(static_cast<size_t>(out.width) * out.height * 3);

		for (size_t i = 0; i < static_cast<size_t>(out.width) * out.height;
		     ++i) {
			rgb.push_back(raw[i * 4 + 0]);
			rgb.push_back(raw[i * 4 + 1]);
			rgb.push_back(raw[i * 4 + 2]);
		}

		out.pixels   = std::move(rgb);
		out.channels = 3;

	} else {
		out.pixels = std::move(raw);
	}

	return true;
}

std_fs::path reference_path(const std::string& name)
{
	return std_fs::path{"tests/png_writer_data/expected"} / (name + ".png");
}

std_fs::path tmp_output_path(const std::string& name)
{
	const auto path = std_fs::temp_directory_path() /
	                  ("dosbox_png_writer_test_" + name + ".png");
	std::error_code ec;
	std_fs::remove(path, ec);
	return path;
}

// Compare two decoded PNGs across pixels and the metadata chunks the
// PngWriter refactor risks shuffling. Returns true on full match.
::testing::AssertionResult pngs_match(const DecodedPng& a, const DecodedPng& b)
{
	if (a.width != b.width || a.height != b.height) {
		return ::testing::AssertionFailure()
		    << "dimensions differ: " << a.width << "x" << a.height
		    << " vs " << b.width << "x" << b.height;
	}

	if (a.channels != b.channels) {
		return ::testing::AssertionFailure()
		    << "channel count differs: " << a.channels << " vs "
		    << b.channels;
	}

	if (a.pixels != b.pixels) {
		return ::testing::AssertionFailure() << "pixel data differs";
	}

	if (a.has_phys != b.has_phys) {
		return ::testing::AssertionFailure()
		    << "pHYs presence differs: " << a.has_phys << " vs "
		    << b.has_phys;
	}

	if (a.has_phys && (a.phys_x_pixels_per_unit != b.phys_x_pixels_per_unit ||
	                   a.phys_y_pixels_per_unit != b.phys_y_pixels_per_unit ||
	                   a.phys_unit_type != b.phys_unit_type)) {

		return ::testing::AssertionFailure()
		    << "pHYs values differ: (" << a.phys_x_pixels_per_unit << ", "
		    << a.phys_y_pixels_per_unit << ", " << a.phys_unit_type
		    << ") vs (" << b.phys_x_pixels_per_unit << ", "
		    << b.phys_y_pixels_per_unit << ", " << b.phys_unit_type << ")";
	}

	if (a.has_gama != b.has_gama) {
		return ::testing::AssertionFailure()
		    << "gAMA presence differs: " << a.has_gama << " vs "
		    << b.has_gama;
	}

	if (a.has_gama && std::abs(a.gama - b.gama) > 1e-9) {
		return ::testing::AssertionFailure()
		    << "gAMA value differs: " << a.gama << " vs " << b.gama;
	}

	if (a.text_chunks.size() != b.text_chunks.size()) {
		return ::testing::AssertionFailure()
		    << "text chunk count differs: " << a.text_chunks.size()
		    << " vs " << b.text_chunks.size();
	}

	for (size_t i = 0; i < a.text_chunks.size(); ++i) {
		if (a.text_chunks[i].key != b.text_chunks[i].key ||
		    a.text_chunks[i].text != b.text_chunks[i].text) {

			return ::testing::AssertionFailure()
			    << "text chunk " << i << " differs: ('"
			    << a.text_chunks[i].key << "' = '"
			    << a.text_chunks[i].text << "') vs ('"
			    << b.text_chunks[i].key << "' = '"
			    << b.text_chunks[i].text << "')";
		}
	}

	return ::testing::AssertionSuccess();
}

void expect_matches_reference(const std_fs::path& rendered, const std::string& test_name)
{
	const auto ref_path = reference_path(test_name);
	std::error_code ec;

	const char* FgRed = "\033[31m";
	const char* Reset = "\033[0m";

	if (!std_fs::exists(ref_path, ec)) {
		std_fs::create_directories(ref_path.parent_path(), ec);

		std_fs::copy_file(rendered,
		                  ref_path,
		                  std_fs::copy_options::overwrite_existing,
		                  ec);

		SCOPED_TRACE(format_str("%sreference PNG missing — wrote new candidate to %s%s",
		                        FgRed,
		                        ref_path.string().c_str(),
		                        Reset));
		FAIL();
		return;
	}

	DecodedPng got = {};
	DecodedPng ref = {};

	ASSERT_TRUE(decode_png(rendered, got))
	        << "failed to decode rendered " << rendered.string();

	ASSERT_TRUE(decode_png(ref_path, ref))
	        << "failed to decode reference " << ref_path.string();

	const auto match = pngs_match(got, ref);
	if (!match) {
		const auto actual_path = ref_path.parent_path() /
		                         (ref_path.filename().string() + "-ACTUAL.png");
		std_fs::copy_file(rendered,
		                  actual_path,
		                  std_fs::copy_options::overwrite_existing,
		                  ec);

		SCOPED_TRACE(format_str("%sexpected and actual PNGs differ (%s) — wrote actual to %s%s",
		                        FgRed,
		                        match.message(),
		                        actual_path.string().c_str(),
		                        Reset));
		FAIL();
	}
}

void write_indexed8(const std_fs::path& out, const PngMetadata& metadata)
{
	FILE* fp = fopen(out.string().c_str(), "wb");
	ASSERT_NE(fp, nullptr);

	const auto palette = make_test_palette();
	const auto pixels  = make_test_pixels_indexed8();

	{
		PngWriter w;
		ASSERT_TRUE(w.InitIndexed8(fp, TestWidth, TestHeight, palette, metadata));

		for (int y = 0; y < TestHeight; ++y) {
			w.WriteRow(pixels.begin() + y * TestWidth);
		}
	}
	fclose(fp);
}

void write_rgb888(const std_fs::path& out, const PngMetadata& metadata)
{
	FILE* fp = fopen(out.string().c_str(), "wb");
	ASSERT_NE(fp, nullptr);

	const auto pixels = make_test_pixels_rgb888();

	{
		PngWriter w;
		ASSERT_TRUE(w.InitRgb888(fp, TestWidth, TestHeight, metadata));

		for (int y = 0; y < TestHeight; ++y) {
			w.WriteRow(pixels.begin() +
			           static_cast<ptrdiff_t>(y * TestWidth * 3));
		}
	}
	fclose(fp);
}

} // namespace

TEST(PngWriterTest, Indexed8_FromVideoMode)
{
	const auto mode     = make_test_video_mode();
	const auto metadata = PngMetadata::FromVideoMode(mode);
	const auto out      = tmp_output_path("Indexed8_FromVideoMode");

	write_indexed8(out, metadata);
	expect_matches_reference(out, "Indexed8_FromVideoMode");
}

TEST(PngWriterTest, Indexed8_FromVideoModeAsSquare)
{
	const auto mode     = make_test_video_mode();
	const auto metadata = PngMetadata::FromVideoModeAsSquare(mode);
	const auto out      = tmp_output_path("Indexed8_FromVideoModeAsSquare");

	write_indexed8(out, metadata);
	expect_matches_reference(out, "Indexed8_FromVideoModeAsSquare");
}

TEST(PngWriterTest, Rgb888_FromVideoMode)
{
	const auto mode     = make_test_video_mode();
	const auto metadata = PngMetadata::FromVideoMode(mode);
	const auto out      = tmp_output_path("Rgb888_FromVideoMode");

	write_rgb888(out, metadata);
	expect_matches_reference(out, "Rgb888_FromVideoMode");
}

TEST(PngWriterTest, Rgb888_FromVideoModeAsSquare)
{
	const auto mode     = make_test_video_mode();
	const auto metadata = PngMetadata::FromVideoModeAsSquare(mode);
	const auto out      = tmp_output_path("Rgb888_FromVideoModeAsSquare");

	write_rgb888(out, metadata);
	expect_matches_reference(out, "Rgb888_FromVideoModeAsSquare");
}
