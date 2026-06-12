// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Integration tests for the virtual printer. Each test feeds a
// known ESC/P byte sequence to a freshly-constructed CPrinter and
// inspects the rendered PNG.
//
// The tests run early in the branch's history (right after the
// printer code first goes in) and evolve forward as APIs change.
// References here use derived metrics (dark-pixel counts) rather
// than exact-pixel comparisons so they survive font / rendering
// tweaks in later commits without churning reference PNGs.

#include "hardware/parport/printer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <png.h>

#include "misc/std_filesystem.h"
#include "hardware/parport/printer_if.h"

#include "dosbox_test_fixture.h"

namespace {

// Test pages: 180 dpi, 5"x4" = 900x720 pixels. Big enough to fit
// a couple of lines of plain text plus a bit-image stripe.
constexpr uint16_t TestDpi          = 180;
constexpr uint16_t TestPageWidth10  = 50; // tenths of inch = 5.0"
constexpr uint16_t TestPageHeight10 = 40; // tenths of inch = 4.0"

class PrinterRenderTest : public DOSBoxTestFixture {
protected:
	std_fs::path output_dir;
	std::string  docpath_storage;
	char         output_format[8] = "png";

	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();

		const auto* test_info =
		        ::testing::UnitTest::GetInstance()->current_test_info();
		output_dir = std_fs::temp_directory_path() /
		             ("dosbox_printer_test_" +
		              std::string(test_info->name()));
		std::error_code ec;
		std_fs::remove_all(output_dir, ec);
		std_fs::create_directories(output_dir, ec);

		docpath_storage = output_dir.string();
		PRINTER_Configure(TestDpi,
		                  TestPageWidth10,
		                  TestPageHeight10,
		                  docpath_storage.c_str(),
		                  "png",
		                  false,
		                  0);
	}

	void TearDown() override
	{
		PRINTER_Reset();
		DOSBoxTestFixture::TearDown();
	}

	// Drive a printer with the given bytes, flush, and return the
	// path to the PNG it produced.
	std_fs::path render(const std::vector<uint8_t>& bytes)
	{
		std::error_code ec;
		std_fs::remove_all(output_dir, ec);
		std_fs::create_directories(output_dir, ec);

		CPrinter printer(TestDpi,
		                 TestPageWidth10,
		                 TestPageHeight10,
		                 output_format,
		                 false);
		for (const auto b : bytes) {
			printer.printChar(b);
		}
		printer.formFeed();

		return output_dir / "page1.png";
	}
};

// Count "dark" pixels in a PNG (any channel < 128). Used as a
// font/rendering-agnostic intensity metric.
size_t count_dark_pixels(const std_fs::path& path)
{
	FILE* fp = fopen(path.string().c_str(), "rb");
	if (!fp) {
		return 0;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	                                             nullptr,
	                                             nullptr,
	                                             nullptr);
	if (!png_ptr) {
		fclose(fp);
		return 0;
	}
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		fclose(fp);
		return 0;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		fclose(fp);
		return 0;
	}

	png_init_io(png_ptr, fp);
	png_read_info(png_ptr, info_ptr);

	png_set_palette_to_rgb(png_ptr);
	png_set_expand(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	const auto w        = png_get_image_width(png_ptr, info_ptr);
	const auto h        = png_get_image_height(png_ptr, info_ptr);
	const auto channels = png_get_channels(png_ptr, info_ptr);
	const auto row_bytes = png_get_rowbytes(png_ptr, info_ptr);

	std::vector<uint8_t> raw(row_bytes * h);
	std::vector<png_bytep> rows(h);
	for (size_t i = 0; i < h; ++i) {
		rows[i] = raw.data() + i * row_bytes;
	}
	png_read_image(png_ptr, rows.data());
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	fclose(fp);

	size_t dark = 0;
	for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
		const auto base = i * channels;
		if (raw[base] < 128 || raw[base + 1] < 128 ||
		    raw[base + 2] < 128) {
			++dark;
		}
	}
	return dark;
}

// Helper: append a sequence of bytes to a vector.
void append(std::vector<uint8_t>& bytes,
            std::initializer_list<uint8_t> seq)
{
	bytes.insert(bytes.end(), seq.begin(), seq.end());
}

void append(std::vector<uint8_t>& bytes, std::string_view s)
{
	bytes.insert(bytes.end(), s.begin(), s.end());
}

} // namespace

// Smoke test: the printer must produce a PNG with some ink on it
// when fed plain ASCII followed by a form feed.
TEST_F(PrinterRenderTest, PlainTextProducesPng)
{
	std::vector<uint8_t> bytes;
	append(bytes, "Hello, world!");
	append(bytes, {0x0D, 0x0A});

	const auto rendered = render(bytes);
	ASSERT_TRUE(std_fs::exists(rendered));
	EXPECT_GT(count_dark_pixels(rendered), 100u);
}

// Bold text should leave noticeably more ink than the same text
// without the bold attribute.
TEST_F(PrinterRenderTest, BoldDarkerThanPlain)
{
	std::vector<uint8_t> plain_bytes;
	append(plain_bytes, "ABCDEFGHIJ");
	append(plain_bytes, {0x0D, 0x0A});
	const auto plain_dark = count_dark_pixels(render(plain_bytes));

	std::vector<uint8_t> bold_bytes;
	append(bold_bytes, {0x1B, 'E'}); // ESC E - bold on
	append(bold_bytes, "ABCDEFGHIJ");
	append(bold_bytes, {0x1B, 'F'}); // ESC F - bold off
	append(bold_bytes, {0x0D, 0x0A});
	const auto bold_dark = count_dark_pixels(render(bold_bytes));

	EXPECT_GT(bold_dark, plain_dark);
}

// ESC K bit-image: feed a known pattern, verify the page has ink
// in roughly the expected band of pixels.
TEST_F(PrinterRenderTest, BitImageEscKProducesInk)
{
	std::vector<uint8_t> bytes;
	// ESC K n1 n2 — 60-dpi bit image, length = n1 + n2*256
	constexpr uint16_t image_cols = 60;
	append(bytes, {0x1B, 'K',
	               static_cast<uint8_t>(image_cols & 0xFF),
	               static_cast<uint8_t>(image_cols >> 8)});
	// All 8 pins on for every column = solid horizontal bar.
	for (int i = 0; i < image_cols; ++i) {
		bytes.push_back(0xFF);
	}
	append(bytes, {0x0D, 0x0A});

	const auto rendered = render(bytes);
	ASSERT_TRUE(std_fs::exists(rendered));
	EXPECT_GT(count_dark_pixels(rendered), 200u);
}

// Double-strike (ESC G) should accumulate ink relative to plain.
TEST_F(PrinterRenderTest, DoubleStrikeAccumulates)
{
	std::vector<uint8_t> plain_bytes;
	append(plain_bytes, "WWWWWWWWWW");
	append(plain_bytes, {0x0D, 0x0A});
	const auto plain_dark = count_dark_pixels(render(plain_bytes));

	std::vector<uint8_t> ds_bytes;
	append(ds_bytes, {0x1B, 'G'}); // ESC G - double-strike on
	append(ds_bytes, "WWWWWWWWWW");
	append(ds_bytes, {0x1B, 'H'}); // ESC H - double-strike off
	append(ds_bytes, {0x0D, 0x0A});
	const auto ds_dark = count_dark_pixels(render(ds_bytes));

	EXPECT_GT(ds_dark, plain_dark);
}
