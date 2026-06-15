// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Printer rendering tests.
//
// Drive the Printer class directly with a known ESC/P input byte sequence,
// render to PNG, compare against a checked-in reference image in
// tests/printer_render_data/expected/.
//
// If the reference and output images differ, the output image will be copied
// next to the reference with the `-ACTUAL.png` suffix.
//
// To regenerate references after intentional behaviour changes, delete the
// reference images and re-run the tests. The rendered output will be copied
// to the expected location AND the test will fail. Visually inspect the new
// reference and commit it. The next test run will pass as now the reference
// image exists.

#include "hardware/printer/printer.h"
#include "hardware/printer/printer_if.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <png.h>

#include "misc/std_filesystem.h"
#include "misc/support.h"
#include "utils/string_utils.h"

#include "dosbox_test_fixture.h"

namespace {

constexpr int TestDpi             = 360;
constexpr double TestPageWidthIn  = 5.0;
constexpr double TestPageHeightIn = 5.0;

constexpr auto NumRgbChannels = 3;

class PrinterRenderTest : public DOSBoxTestFixture {
protected:
	std_fs::path output_dir;

	// Holds the docpath string so the pointer passed into
	// PRINTER_Configure stays valid for the lifetime of the test.
	std::string docpath_holder;

	// Wipe stale -ACTUAL.png diagnostics from a prior run before the
	// suite starts. Without this, an old failing test's actual image
	// hangs around in `expected/` confusing inspection after a later
	// pass.
	static void SetUpTestSuite()
	{
		const std_fs::path expected_dir{"tests/printer_render_data/expected"};

		std::error_code ec;
		if (!std_fs::is_directory(expected_dir, ec)) {
			return;
		}

		for (const auto& entry :
		     std_fs::directory_iterator(expected_dir, ec)) {
			if (!entry.is_regular_file()) {
				continue;
			}
			if (entry.path().filename().string().ends_with("-ACTUAL.png")) {
				std_fs::remove(entry.path(), ec);
			}
		}
	}

	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();

		// Per-test output dir under the project's tests area.
		const auto* test_info =
		        testing::UnitTest::GetInstance()->current_test_info();

		output_dir = std_fs::temp_directory_path() /
		             ("dosbox_printer_test_" +
		              std::string(test_info->name()));

		std::error_code ec;
		std_fs::remove_all(output_dir, ec);
		std_fs::create_directories(output_dir, ec);

		// Sets document_path global so CPrinter writes pages under
		// output_dir as page1.png, page2.png, ...
		docpath_holder = output_dir.string();
		PRINTER_Configure(TestDpi,
		                  static_cast<uint16_t>(TestPageWidthIn * 10),
		                  static_cast<uint16_t>(TestPageHeightIn * 10),
		                  docpath_holder.c_str(),
		                  "png",
		                  false /*multipage*/,
		                  5000 /*timeout_ms*/);
	}

	void TearDown() override
	{
		PRINTER_Reset();
		DOSBoxTestFixture::TearDown();
	}

	// Drive the virtual printer with the given bytes, flush, and return the
	// path to the PNG it produced. Clears output_dir first so the file is
	// always page1.png.
	std_fs::path render(const std::vector<uint8_t>& bytes)
	{
		return render_with_pins(bytes, 24);
	}

	// Same as render() but with a configurable pin count. At chrono 1
	// only 24-pin Epson exists; the pin count is accepted but ignored
	// (only 24-pin variants of tests are kept at this commit).
	std_fs::path render_with_pins(const std::vector<uint8_t>& bytes, const int pins)
	{
		return render_with_page(
		        bytes, TestDpi, TestPageWidthIn, TestPageHeightIn, pins);
	}

	// Most general render helper. Lets tests pick a page size to match
	// the conditions in the byte stream's origin test (e.g. escapy's
	// default is A4 portrait; our default test page is 5x5 inches).
	std_fs::path render_with_page(const std::vector<uint8_t>& bytes,
	                              const uint16_t dpi, const double width_in,
	                              const double height_in, const int /*pins*/ = 24)
	{
		std::error_code ec;
		std_fs::remove_all(output_dir, ec);
		std_fs::create_directories(output_dir, ec);

		// Refresh global config in case the test uses a different page
		// size than the default established in SetUp().
		docpath_holder = output_dir.string();
		PRINTER_Configure(dpi,
		                  static_cast<uint16_t>(width_in * 10),
		                  static_cast<uint16_t>(height_in * 10),
		                  docpath_holder.c_str(),
		                  "png",
		                  false /*multipage*/,
		                  5000 /*timeout_ms*/);

		// CPrinter takes a writable char* for the output format.
		char output_format[8];
		std::strncpy(output_format, "png", sizeof(output_format) - 1);
		output_format[sizeof(output_format) - 1] = '\0';

		VirtualPrinter::Printer printer(dpi,
		                 static_cast<uint16_t>(width_in * 10),
		                 static_cast<uint16_t>(height_in * 10),
		                 output_format,
		                 false /*multipageOutput*/);

		for (const auto byte : bytes) {
			printer.PrintChar(byte);
		}
		printer.FormFeed();

		return output_dir / "page1.png";
	}

	// Render one of the PC5150 density-test .bin fixtures at the
	// shared prime page DPI and compare to its checked-in golden.
	void render_pc5150_density(int density);
};

// Decode an indexed-palette PNG into a flat RGB buffer.
// Returns false on failure or on dimension mismatch with the
// other image's expected dimensions.
struct DecodedImage {
	int width  = 0;
	int height = 0;

	std::vector<uint8_t> rgb = {};
};

bool decode_png(const std_fs::path& path, DecodedImage& out)
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

// Compare two PNGs. Returns the fraction of pixels that differ
// (0.0 = identical, 1.0 = every pixel differs). Returns 1.0 (worst)
// on any decode failure or dimension mismatch.
float compare_pngs(const std_fs::path& a, const std_fs::path& b)
{
	DecodedImage ia = {};
	DecodedImage ib = {};

	if (!decode_png(a, ia) || !decode_png(b, ib)) {
		return 1.0f;
	}
	if (ia.width != ib.width || ia.height != ib.height) {
		return 1.0f;
	}

	size_t diff_pixels = 0;

	const auto pixel_count = static_cast<size_t>(ia.width) * ia.height;

	for (size_t i = 0; i < pixel_count; ++i) {
		if (ia.rgb[i * NumRgbChannels + 0] != ib.rgb[i * NumRgbChannels + 0] ||
		    ia.rgb[i * NumRgbChannels + 1] != ib.rgb[i * NumRgbChannels + 1] ||
		    ia.rgb[i * NumRgbChannels + 2] != ib.rgb[i * NumRgbChannels + 2]) {

			++diff_pixels;
		}
	}

	return static_cast<float>(diff_pixels) / pixel_count;
}

// Count "darkish" pixels (any channel below 128). Used to assert that
// e.g. double-strike text is meaningfully darker than single-strike.
size_t count_dark_pixels(const std_fs::path& path)
{
	DecodedImage img = {};

	if (!decode_png(path, img)) {
		return 0;
	}

	size_t dark                  = 0;
	constexpr auto DarkThreshold = 128;

	const auto pixel_count = static_cast<size_t>(img.width) * img.height;

	for (size_t i = 0; i < pixel_count; ++i) {
		const auto r = img.rgb[i * NumRgbChannels + 0];
		const auto g = img.rgb[i * NumRgbChannels + 1];
		const auto b = img.rgb[i * NumRgbChannels + 2];

		if (r < DarkThreshold || g < DarkThreshold || b < DarkThreshold) {
			++dark;
		}
	}
	return dark;
}

// Resolve a path to a reference PNG. Tests run from the project root
// (gtest_discover_tests sets the WORKING_DIRECTORY).
std_fs::path reference_path(const std::string& name)
{
	return std_fs::path{"tests/printer_render_data/expected"} / (name + ".png");
}

// Same diff tolerance for every test. compare_pngs counts mismatched
// pixels, so 0.5% lets a handful of edge pixels move under genuine
// rendering-pipeline noise (Freetype version drift, sub-pixel rounding)
// without leaving any cover for real regressions to hide in.
constexpr float ImageDiffTolerance = 0.005f;

// Compare rendered output against the reference. If the reference is missing,
// copy the rendered output to the expected location AND fail the test (so the
// user sees the failure and can commit the new reference deliberately).
void expect_matches_reference(const std_fs::path& rendered_path,
                              const std::string& test_name)
{
	const auto ref_path = reference_path(test_name);
	std::error_code ec;

	const char* FgRed = "\033[31m";
	const char* Reset = "\033[0m";

	if (!std_fs::exists(ref_path, ec)) {
		std_fs::create_directories(ref_path.parent_path(), ec);
		std_fs::copy_file(rendered_path,
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

	const auto diff = compare_pngs(rendered_path, ref_path);
	SCOPED_TRACE(format_str("test=%s diff=%.4f%% tolerance=%.4f%%",
	                        test_name.c_str(),
	                        diff * 100.0f,
	                        ImageDiffTolerance * 100.0f));

	if (diff > ImageDiffTolerance) {
		const auto actual_path = ref_path.parent_path() /
		                         (ref_path.filename().string() + "-ACTUAL.png");

		std_fs::copy_file(rendered_path,
		                  actual_path,
		                  std_fs::copy_options::overwrite_existing,
		                  ec);

		SCOPED_TRACE(format_str("%sexpected and actual images differ — wrote actual to %s%s",
		                        FgRed,
		                        actual_path.string().c_str(),
		                        Reset));

		FAIL();
		return;
	}

	EXPECT_LE(diff, ImageDiffTolerance);
}

// Helper: append a byte-literal sequence to `bytes`.
void append(std::vector<uint8_t>& bytes, std::initializer_list<uint8_t> seq)
{
	bytes.insert(bytes.end(), seq.begin(), seq.end());
}

void append(std::vector<uint8_t>& bytes, std::string_view s)
{
	bytes.insert(bytes.end(), s.begin(), s.end());
}

// Helper: load a binary fixture file as a byte vector.
std::vector<uint8_t> load_fixture(const std_fs::path& rel_path)
{
	std::ifstream in(rel_path, std::ios::binary);
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
	                           std::istreambuf_iterator<char>());
	return bytes;
}

// ---------------------------------------------------------------------------
// A black-and-white photograph dithered to 1-bit and replayed across every
// ESC * bit-image density mode the printer supports -- the six 8-pin modes
// (0, 1, 2, 3, 4, 6), the five 24-pin modes (32, 33, 38, 39, 40), and the
// three 48-pin modes (71, 72, 73). The source PC5150_withwoman.jpg is
// encoded for each density by tests/printer_render_data/tools/png_to_escp.py
// at a fixed 3" target width. Page DPI is a prime (233) so no data dpi
// divides it cleanly -- exercises the PrintBitGraph anti-aliased coverage
// at every non-integer dot/pixel ratio.
//
// Pure pixel output, no font involvement -- exact-pixel comparison.
constexpr uint16_t Pc5150PageDpi  = 233;
constexpr double Pc5150PageWidth  = 5.0;
constexpr double Pc5150PageHeight = 5.0;

void PrinterRenderTest::render_pc5150_density(const int density)
{
	const auto suffix = std::to_string(density);
	const auto bin    = "tests/printer_render_data/inputs/PC5150_density" +
	                 suffix + ".bin";
	const auto bytes = load_fixture(bin);
	ASSERT_FALSE(bytes.empty());
	const auto rendered = render_with_page(bytes,
	                                       Pc5150PageDpi,
	                                       Pc5150PageWidth,
	                                       Pc5150PageHeight);
	expect_matches_reference(rendered, "Graphics_Pc5150_Density" + suffix);
}

TEST_F(PrinterRenderTest, Graphics_Pc5150_Density0)
{
	render_pc5150_density(0);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density1)
{
	render_pc5150_density(1);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density2)
{
	render_pc5150_density(2);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density3)
{
	render_pc5150_density(3);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density4)
{
	render_pc5150_density(4);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density6)
{
	render_pc5150_density(6);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density32)
{
	render_pc5150_density(32);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density33)
{
	render_pc5150_density(33);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density38)
{
	render_pc5150_density(38);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density39)
{
	render_pc5150_density(39);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density40)
{
	render_pc5150_density(40);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density71)
{
	render_pc5150_density(71);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density72)
{
	render_pc5150_density(72);
}
TEST_F(PrinterRenderTest, Graphics_Pc5150_Density73)
{
	render_pc5150_density(73);
}

// ---------------------------------------------------------------------------
// The full 7-colour CMY/RGB palette plus the overprint "OR rule"
// demonstration.
//
// The printer uses a subtractive CMY (cyan / magenta / yellow)
// ribbon-overprint colour model where overprinted colours are OR'd together
// by sub-palette ID:
//
//   sub-palette 1 = magenta  (M = 001)
//   sub-palette 2 = cyan     (C = 010)
//   sub-palette 4 = yellow   (Y = 100)
//   sub-palette 3 = blue     (M|C  = 011)
//   sub-palette 5 = red      (M|Y  = 101)
//   sub-palette 6 = green    (C|Y  = 110)
//   sub-palette 7 = black    (M|C|Y = 111)
//
// The test draws:
//
//   1. A row of seven solid colour swatches showing every reachable
//      sub-palette colour individually (ESC r 1..7).
//
//   2. Below that, three overlapping bars in C / M / Y that produce the same
//      colour set in their overlap regions — proving the OR rule live (the
//      colours in the overlaps should match the swatches above them).
//
TEST_F(PrinterRenderTest, Graphics_CmyPalette)
{
	std::vector<uint8_t> bytes = {};

	constexpr uint16_t SwatchCols      = 60; // ~ 0.33"
	constexpr uint16_t SwatchGap       = 12; // ~ 0.07"
	constexpr uint16_t SwatchPitchCols = SwatchCols + SwatchGap;

	const uint8_t colour_order[]       = {2, 3, 1, 5, 4, 6, 7};
	constexpr uint8_t SwatchStartX60th = 12; // 0.2"

	// Helper: draw one swatch with a given byte pattern repeating across
	// SwatchCols columns of a 24-pin band. `pattern` is the single byte
	// tiled vertically and horizontally.
	auto draw_swatch_row = [&](uint8_t colour_id, uint8_t x_60th, uint8_t pattern) {
		std::vector<uint8_t> col(3 * SwatchCols, pattern);

		append(bytes, {0x1B, 'r', colour_id});
		append(bytes, {0x1B, '$', x_60th, 0});
		append(bytes,
		       {0x1B,
		        '*',
		        39,
		        static_cast<uint8_t>(SwatchCols & 0xFF),
		        static_cast<uint8_t>(SwatchCols >> 8)});

		bytes.insert(bytes.end(), col.begin(), col.end());
	};

	// Bit-image graphics is binary -- every set bit lights a pixel at the
	// sub-palette's max intensity. To show different shades we use spatial
	// dithering: a sparser bit pattern leaves more white showing through
	// and looks lighter at viewing distance. This is what real DOS printers
	// did for "grey" output.
	const uint8_t intensity_patterns[] = {
	        0xFF, // 100% — every pin lit
	        0x55, //  50% — alternating
	        0x11, //  25% — every 4th
	        0x05, //  12.5% — every 8th
	};

	auto print_row_of_swatches = [&](uint8_t pattern) {
		for (size_t i = 0; i < std::size(colour_order); ++i) {
			const uint16_t x_180 = SwatchStartX60th * 3 +
			                       static_cast<uint16_t>(i) *
			                               SwatchPitchCols;

			const auto x_60th = static_cast<uint8_t>(x_180 / 3);

			draw_swatch_row(colour_order[i], x_60th, pattern);
		}

		// Vertical step between rows (24-pin band + spacing).
		append(bytes, {0x0D, 0x0A, 0x0A});
	};

	// Four rows: full intensity → quarter-eighth halftone.
	for (const auto pat : intensity_patterns) {
		print_row_of_swatches(pat);
	}

	// Bottom: three wide overlapping C / M / Y bars. Same seven
	// sub-palette colours appear in the overlap regions.
	append(bytes, {0x0D, 0x0A, 0x0A, 0x0A});

	constexpr uint16_t BarCols = 180; // ~ 1.0"

	const std::vector<uint8_t> bar_col(3 * BarCols, 0xFF);

	auto draw_bar = [&](uint8_t colour, uint8_t x_60th) {
		append(bytes, {0x1B, 'r', colour});
		append(bytes, {0x1B, '$', x_60th, 0});

		append(bytes,
		       {0x1B,
		        '*',
		        39,
		        static_cast<uint8_t>(BarCols & 0xFF),
		        static_cast<uint8_t>(BarCols >> 8)});

		bytes.insert(bytes.end(), bar_col.begin(), bar_col.end());
	};

	draw_bar(2, 30); // cyan
	draw_bar(1, 54); // magenta
	draw_bar(4, 78); // yellow

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Graphics_CmyPalette");
}

// ---------------------------------------------------------------------------
// Bit-image graphics density tests.
//
// All four tests below drive ESC * with the SAME 44-column input stream taken
// verbatim from escapy's reference test suite at (`data_44_columns`), only
// the density code and byte-padding-per-column change between modes.
//
// The escapy stream renders a reversed-NOT (⌐) symbol -- a known fixture used
// by escapy's own visual mode-1/mode-2 tests, so the output is one we can
// cross-check against the escapy PDF rendering.
//
// For each density: cross-reference reference renders are produced via escapy
// (in a Python venv) at the same page DPI, then visually compared to our
// impl's PNG before the expected/*.png reference is committed.

namespace {

// 44-column bit-image data fixture. Verbatim copy of escapy's data_44_columns
// test data.
//
constexpr std::array<uint8_t, 44> Data44Columns = {
        0x00, 0x00, 0x7f, 0x7f, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
        0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00,
        0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
        0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00,
};
} // namespace

// Helper: drive ESC * at the given density with 44 columns, replicating
// Data44Columns across the column's byte count. For density < 32 the column
// is 1 byte; 32-63 -> 3 bytes; 64-73 -> 6 bytes. This mirrors what escapy
// does for each mode tested.
//
std::vector<uint8_t> bit_image_44col_stream(uint8_t density)
{
	std::vector<uint8_t> bytes = {};

	// ESC @ reset, then ESC * <density> nL=44 nH=0 + data
	append(bytes, {0x1b, '@'});
	append(bytes, {0x1b, '*', density, 44, 0});

	const int bytes_per_col = density < 32 ? 1 : (density < 64 ? 3 : 6);
	for (int rep = 0; rep < bytes_per_col; ++rep) {
		bytes.insert(bytes.end(), Data44Columns.begin(), Data44Columns.end());
	}

	return bytes;
}

// Density 1 (120h x 60v, 1 byte/col, adjacent=true). Used by escapy's
// test_select_bit_image as the canonical baseline.
TEST_F(PrinterRenderTest, Graphics_Density1_Escapy44Col)
{
	const auto rendered_path = render(bit_image_44col_stream(1));
	expect_matches_reference(rendered_path, "Graphics_Density1_Escapy44Col");
}

// Density 3 (240h x 60v, 1 byte/col, adjacent=false)
TEST_F(PrinterRenderTest, Graphics_Density3_Escapy44Col)
{
	const auto rendered_path = render(bit_image_44col_stream(3));
	expect_matches_reference(rendered_path, "Graphics_Density3_Escapy44Col");
}

// Density 40 (360h x 180v, 3 bytes/col, adjacent=false)
TEST_F(PrinterRenderTest, Graphics_Density40_Escapy44Col)
{
	const auto rendered_path = render(bit_image_44col_stream(40));
	expect_matches_reference(rendered_path, "Graphics_Density40_Escapy44Col");
}

// Density 72 (360h x 360v, 6 bytes/col, adjacent=false)
TEST_F(PrinterRenderTest, Graphics_Density72_Escapy44Col)
{
	const auto rendered_path = render(bit_image_44col_stream(72));
	expect_matches_reference(rendered_path, "Graphics_Density72_Escapy44Col");
}

// ESC J line-spacing-divisor cross-check. Two rows of escapy's
// data_44_columns fixture separated by `ESC J 24`. The vertical
// gap between the rows is:
//   24-pin: 24/180 in = 0.133 in (escp2ref R-62)
//   9-pin:  24/216 in = 0.111 in
//
// Two checked-in references prove the divisor branch from commit
// c67aef038 fires the right way per pin count.
std::vector<uint8_t> bit_image_44col_two_rows_esc_j(uint8_t density, uint8_t j_n)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1b, '@'});
	// First row
	append(bytes, {0x1b, '*', density, 44, 0});
	bytes.insert(bytes.end(), Data44Columns.begin(), Data44Columns.end());
	// CR back to left margin, then ESC J n vertical feed
	append(bytes, {0x0d, 0x1b, 'J', j_n});
	// Second row
	append(bytes, {0x1b, '*', density, 44, 0});
	bytes.insert(bytes.end(), Data44Columns.begin(), Data44Columns.end());
	append(bytes, {0x0c});
	return bytes;
}

TEST_F(PrinterRenderTest, Graphics_EscJVerticalFeed_Escapy44Col)
{
	const auto rendered = render(bit_image_44col_two_rows_esc_j(1, 24));
	expect_matches_reference(rendered, "Graphics_EscJVerticalFeed_Escapy44Col");
}

// Helper: emit one "<typeface name>: <style description> <SAMPLE>"
// line with the style applied to the SAMPLE text. The label uses
// the previously-set typeface (so the label takes on the typeface
// itself, making the typeface differences obvious).
void emit_style_row(std::vector<uint8_t>& bytes, std::string_view label,
                    std::function<void(std::vector<uint8_t>&)> set_style,
                    std::function<void(std::vector<uint8_t>&)> clear_style)
{
	append(bytes, label);
	append(bytes, "  ");

	set_style(bytes);
	append(bytes, "AaBbCc 123 The quick brown fox");
	clear_style(bytes);

	append(bytes, {0x0D, 0x0A});
}

// Common body for the proportional and fixed-pitch style grids. Switches
// typeface and runs through nine style variations. The caller sets the
// proportional flag before calling.
void emit_style_grid(std::vector<uint8_t>& bytes)
{
	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	for (const auto& [typeface_id, typeface_name] :
	     std::initializer_list<std::pair<uint8_t, std::string_view>>{
	             {0,      "Roman"},
                     {1, "Sans Serif"},
                     {2,    "Courier"}
        }) {

		// Select typeface for both label and sample.
		append(bytes, {0x1B, 'k', typeface_id});

		// 11-char wide labels (the longest is "Dbl-strike:").
		emit_style_row(bytes, "Plain:     ", [](auto&) {}, [](auto&) {});

		emit_style_row(
		        bytes,
		        "Bold:      ",
		        [](auto& b) { append(b, {0x1B, 'E'}); },
		        [](auto& b) { append(b, {0x1B, 'F'}); });

		emit_style_row(
		        bytes,
		        "Italic:    ",
		        [](auto& b) { append(b, {0x1B, '4'}); },
		        [](auto& b) { append(b, {0x1B, '5'}); });

		emit_style_row(
		        bytes,
		        "Bold+It:   ",
		        [](auto& b) { append(b, {0x1B, 'E', 0x1B, '4'}); },
		        [](auto& b) { append(b, {0x1B, '5', 0x1B, 'F'}); });

		emit_style_row(
		        bytes,
		        "Underline: ",
		        [](auto& b) { append(b, {0x1B, '-', 1}); },
		        [](auto& b) { append(b, {0x1B, '-', 0}); });

		emit_style_row(
		        bytes,
		        "Dbl-strike:",
		        [](auto& b) { append(b, {0x1B, 'G'}); },
		        [](auto& b) { append(b, {0x1B, 'H'}); });
		nl();

		(void)typeface_name; // unused -- label takes typeface naturally
	}
}

// ---------------------------------------------------------------------------
// Style permutation grid in **proportional** mode.
//
// Three Liberation typefaces × six style variations: plain, bold, italic,
// bold+italic, underline, double-strike. Each row prints the sample text in
// the current typeface so the typeface itself is visible in the label. ESC p
// 1 (proportional) means each glyph gets its natural width — Serif and Sans
// Serif look like real proportional type; Courier is monospace by design.
TEST_F(PrinterRenderTest, Text_StylesProportional)
{
	std::vector<uint8_t> bytes = {};

	append(bytes, {0x1B, 'p', 1}); // ESC p 1 — proportional ON
	append(bytes, {0x1B, 'P'});    // ESC P  — 10 cpi base
	emit_style_grid(bytes);

	const auto rendered_path = render(bytes);
	expect_matches_reference(rendered_path, "Text_StylesProportional");
}

// ---------------------------------------------------------------------------
// The same style permutation grid in **fixed-pitch** mode.
//
// ESC p 0 means each glyph fills a 1/cpi-inch cell regardless of its natural
// width. With Phase 3-era glyph centring, narrow proportional glyphs sit in
// the middle of their cells rather than being left-aligned with a gap on the
// right.
TEST_F(PrinterRenderTest, Text_StylesFixedPitch)
{
	std::vector<uint8_t> bytes = {};

	append(bytes, {0x1B, 'p', 0}); // ESC p 0 — proportional OFF
	append(bytes, {0x1B, 'P'});    // ESC P  — 10 cpi
	emit_style_grid(bytes);

	const auto rendered_path = render(bytes);
	expect_matches_reference(rendered_path, "Text_StylesFixedPitch");
}

// ---------------------------------------------------------------------------
// Pitch and size variations.
//
// Demonstrates the printer's CPI switches (10 / 12 / 15) and the double-width
// / double-height attributes. Single typeface (Roman) to keep the focus on
// the size/pitch dimensions. Fixed-pitch mode so each CPI value clearly
// shows.
TEST_F(PrinterRenderTest, Text_PitchAndSize)
{
	std::vector<uint8_t> bytes = {};

	append(bytes, {0x1B, 'p', 0}); // proportional off

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	// CPI variations across the three typefaces.
	const uint8_t typefaces[] = {0, 1, 2};
	const char* tf_names[]    = {"Roman ", "Sans  ", "Courier"};

	for (size_t i = 0; i < std::size(typefaces); ++i) {
		append(bytes, {0x1B, 'k', typefaces[i]});
		append(bytes, {0x1B, 'P'}); // 10 cpi
		append(bytes, tf_names[i]);
		append(bytes, " 10cpi The quick brown fox");
		nl();

		append(bytes, {0x1B, 'M'}); // 12 cpi
		append(bytes, tf_names[i]);
		append(bytes, " 12cpi The quick brown fox");
		nl();

		append(bytes, {0x1B, 'g'}); // 15 cpi
		append(bytes, tf_names[i]);
		append(bytes, " 15cpi The quick brown fox");
		nl();
	}
	nl();

	// Double-width / double-height variants in each typeface.
	append(bytes, {0x1B, 'P'}); // back to 10 cpi
	for (size_t i = 0; i < std::size(typefaces); ++i) {
		append(bytes, {0x1B, 'k', typefaces[i]});
		append(bytes, {0x1B, 'W', 1}); // double width on
		append(bytes, tf_names[i]);
		append(bytes, " Dbl-width");
		append(bytes, {0x1B, 'W', 0}); // off
		nl();
	}
	nl();

	// Bump line spacing so double-height rows don't overlap.
	append(bytes, {0x1B, '3', 60}); // 60/180" line feed
	for (size_t i = 0; i < std::size(typefaces); ++i) {
		append(bytes, {0x1B, 'k', typefaces[i]});
		append(bytes, {0x1B, 'w', 1}); // double height on
		append(bytes, tf_names[i]);
		append(bytes, " Dbl-height");
		append(bytes, {0x1B, 'w', 0}); // off
		nl();
	}
	append(bytes, {0x1B, '2'}); // restore default 1/6"

	const auto rendered_path = render(bytes);
	expect_matches_reference(rendered_path, "Text_PitchAndSize");
}

// ---------------------------------------------------------------------------
// Line/score variants via ESC ( -.
//
// The command format is ESC ( - 3 0 1 d1 d2, where d1 selects the location
// (1=underline, 2=strikethrough, 3=overscore) and d2 the type (1=single
// continuous, 2=double continuous, 5=single broken, 6=double broken). Shows
// the full grid.
TEST_F(PrinterRenderTest, Text_LineScores)
{
	std::vector<uint8_t> bytes = {};

	append(bytes, {0x1B, 'p', 1}); // proportional on
	append(bytes, {0x1B, 'k', 0}); // Roman

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	// ESC ( - 3 0 1 d1 d2 — set line-score; d2=0 clears.
	auto set_score = [&](uint8_t location, uint8_t type) {
		append(bytes, {0x1B, '(', '-', 3, 0, 1, location, type});
	};
	auto clear_score = [&]() {
		append(bytes, {0x1B, '(', '-', 3, 0, 1, 1, 0}); // clear underline
		append(bytes, {0x1B, '(', '-', 3, 0, 1, 2, 0}); // clear strike
		append(bytes, {0x1B, '(', '-', 3, 0, 1, 3, 0}); // clear over
	};

	const std::string_view types[] = {
	        "single continuous (1)",
	        "double continuous (2)",
	        "single broken     (5)",
	        "double broken     (6)",
	};
	const uint8_t type_codes[] = {1, 2, 5, 6};

	for (size_t t = 0; t < std::size(type_codes); ++t) {
		append(bytes, "Underline ");
		set_score(1, type_codes[t]);
		append(bytes, types[t]);
		clear_score();
		nl();
		nl();

		append(bytes, "Strike    ");
		set_score(2, type_codes[t]);
		append(bytes, types[t]);
		clear_score();
		nl();
		nl();

		append(bytes, "Overscore ");
		set_score(3, type_codes[t]);
		append(bytes, types[t]);
		clear_score();
		nl();
		nl();
	}

	const auto rendered_path = render(bytes);
	expect_matches_reference(rendered_path, "Text_LineScores");
}

// ---------------------------------------------------------------------------
// Scalable fonts via ESC X (multipoint). Selects each of
//
// 8, 10.5, 16, 24-point sizes and prints a sample at each. ESC X format: ESC
// X m nL nH, where m is the pitch (0 or 1 = keep current) and nL+nH*256 is
// the point-size in half-points (16-point = 32 half-points, 24-point = 48).
TEST_F(PrinterRenderTest, Text_MultipointFonts)
{
	std::vector<uint8_t> bytes = {};

	append(bytes, {0x1B, 'p', 1}); // proportional on
	append(bytes, {0x1B, 'k', 0}); // Roman

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	auto multipoint = [&](uint16_t halfpoints) {
		append(bytes,
		       {0x1B,
		        'X',
		        0,
		        static_cast<uint8_t>(halfpoints & 0xFF),
		        static_cast<uint8_t>(halfpoints >> 8)});
	};

	multipoint(16); // 8 point
	append(bytes, "8pt    The quick brown fox");
	nl();

	multipoint(21); // 10.5 point
	append(bytes, "10.5pt The quick brown fox");
	nl();

	multipoint(32); // 16 point
	append(bytes, "16pt   Quick fox");
	nl();

	multipoint(48); // 24 point
	append(bytes, "24pt   Quick");
	nl();

	const auto rendered_path = render(bytes);
	expect_matches_reference(rendered_path, "Text_MultipointFonts");
}

// ---------------------------------------------------------------------------
// International character sets via ESC R.
//
// Selects various national variants and prints the high-bit ASCII region
// (chars 0x23 onwards) — each national variant remaps a handful of
// punctuation slots to local-language characters (£ for UK, ¤ for France, ñ
// for Spain, etc.).
TEST_F(PrinterRenderTest, Text_InternationalCharsets)
{
	std::vector<uint8_t> bytes = {};

	append(bytes, {0x1B, 'p', 0}); // fixed pitch so columns line up
	append(bytes, {0x1B, 'k', 2}); // Courier
	append(bytes, {0x1B, 'M'});    // 12 cpi

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	// Top section: ESC R national variants show how the
	// 12 low-ASCII "punctuation slots" remap per country.
	append(bytes, "ESC R national variants (low-ASCII slots):");
	nl();
	nl();

	const std::string sample = " # $ @ [ \\ ] ^ ` { | } ~";

	const struct {
		uint8_t code;
		std::string_view name;
	} variants[] = {
	        {0, "USA       "},
	        {1, "France    "},
	        {2, "Germany   "},
	        {3, "UK        "},
	        {5, "Sweden    "},
	        {6, "Italy     "},
	        {7, "Spain     "},
	};

	for (const auto& v : variants) {
		append(bytes, {0x1B, 'R', v.code});
		append(bytes, v.name);
		append(bytes, sample);
		nl();
	}
	append(bytes, {0x1B, 'R', 0}); // back to USA
	nl();

	// Bottom section: the CP437 high-ASCII accented characters
	// (bytes 0x80-0xAF). These come from the active character
	// table (default CP437) and don't change with ESC R.
	append(bytes, "CP437 high-ASCII accented characters:");
	nl();
	nl();

	for (uint8_t row = 0x80; row < 0xB0; row += 0x10) {
		bytes.push_back(static_cast<uint8_t>(
		        (row >> 4) < 10 ? '0' + (row >> 4) : 'A' + (row >> 4) - 10));

		append(bytes, "x: ");

		for (uint8_t col = 0; col < 0x10; ++col) {
			bytes.push_back(static_cast<uint8_t>(row + col));
			bytes.push_back(' ');
		}
		nl();
	}

	const auto rendered_path = render(bytes);
	expect_matches_reference(rendered_path, "Text_InternationalCharsets");
}

// ---------------------------------------------------------------------------
// Superscript / subscript. Exercises the ESC S param-index
// fix from Phase 3a.
//
// Ported verbatim from escapy (test_text_scripting). Sub/superscript
// interleaved with condensed, double-width, and double-height. Uses CuSO4
// (copper sulfate) with both H2O-style subscript and superscript on a single
// line.
TEST_F(PrinterRenderTest, Text_Scripting_Escapy)
{
	// copper_sulfate from escapy = "CuSO" + ESC S 1 + "4" + ESC T
	// copper_and_sulfate from escapy = "Cu" + ESC S 0 "2+" + ESC T
	//                                 + " + SO" + ESC S 1 "4" + BS
	//                                 + ESC T + ESC S 0 "2-" + ESC T
	const auto add_copper_sulfate = [](std::vector<uint8_t>& b) {
		append(b, "CuSO");
		append(b, {0x1b, 'S', 1});
		append(b, "4");
		append(b, {0x1b, 'T'});
	};

	const auto add_copper_and_sulfate = [](std::vector<uint8_t>& b) {
		append(b, "Cu");
		append(b, {0x1b, 'S', 0});
		append(b, "2+");
		append(b, {0x1b, 'T'});
		append(b, " + SO");
		append(b, {0x1b, 'S', 1});
		append(b, "4");
		b.push_back(0x08); // BS
		append(b, {0x1b, 'T'});
		append(b, {0x1b, 'S', 0});
		append(b, "2-");
		append(b, {0x1b, 'T'});
	};

	std::vector<uint8_t> bytes = {};

	auto nl = [&]() { append(bytes, {0x0d, 0x0a}); };

	append(bytes, {0x1b, '@'}); // esc_reset
	nl();

	append(bytes, "sub/superscript examples - ESC S");
	nl();
	nl();

	append(bytes, "Default");
	nl();

	bytes.push_back(0x09); // TAB
	add_copper_sulfate(bytes);
	append(bytes, " => ");
	add_copper_and_sulfate(bytes);
	nl();
	nl();

	append(bytes, "1/15 character_pitch");
	nl();

	append(bytes, "select_15cpi (ESC g) + <text> + select_10cpi (ESC P)");
	nl();

	append(bytes, {0x1b, 'g'}); // select_15cpi
	bytes.push_back(0x09);
	add_copper_sulfate(bytes);
	append(bytes, " => ");
	add_copper_and_sulfate(bytes);
	append(bytes, {0x1b, 'P'}); // select_10cpi
	nl();
	nl();

	append(bytes, "condensed printing - ESC SI, SI");
	nl();

	append(bytes, {0x09, 0x1b, 0x0f}); // tab + ESC SI (condensed on)
	add_copper_sulfate(bytes);
	append(bytes, {' ', 0x12, '=', '>', 0x0f, ' '}); // DC2 off, => , SI on
	add_copper_and_sulfate(bytes);
	bytes.push_back(' ');
	bytes.push_back(0x12); // DC2
	nl();
	nl();

	append(bytes, "double width printing - ESC SO, SO - ESC W");
	nl();

	append(bytes, {0x09, 0x1b, 0x0e}); // tab + ESC SO (double-width 1 line)
	add_copper_sulfate(bytes);
	append(bytes, {' ', 0x14, '=', '>', 0x0e, ' '}); // DC4 off, => , SO on
	add_copper_and_sulfate(bytes);
	bytes.push_back(' ');
	bytes.push_back(0x14); // DC4
	nl();

	// Test values 0 vs "0", 1 vs "1"
	append(bytes, {0x09, 0x1b, 'W', 0x01});
	add_copper_sulfate(bytes);
	append(bytes, {' ', 0x1b, 'W', 0x00, '=', '>', 0x1b, 'W', 0x31, ' '});
	add_copper_and_sulfate(bytes);
	append(bytes, {' ', 0x1b, 'W', 0x30});
	nl();
	nl();

	append(bytes, "double height printing - ESC w");
	nl();
	nl();

	// Test values 0 vs "0", 1 vs "1"
	append(bytes, {0x09, 0x1b, 'w', 0x01});
	add_copper_sulfate(bytes);
	append(bytes, {' ', 0x1b, 'w', 0x00, '=', '>', 0x1b, 'w', 0x31, ' '});
	add_copper_and_sulfate(bytes);
	append(bytes, {' ', 0x1b, 'w', 0x30});

	const auto rendered = render_with_page(bytes,
	                                       TestDpi,
	                                       TestPageWidthIn,
	                                       TestPageHeightIn);

	expect_matches_reference(rendered, "Text_Scripting_Escapy");
}

// ---------------------------------------------------------------------------
// Escapy-ported tests.
//
// The block below ports several escapy text-rendering fixtures
// (escapy/tests/test_text_commands.py) into our test harness. Each
// reproduces escapy's byte stream verbatim; the rendered PNG is
// compared against our own checked-in reference (escapy's PDFs are
// not consulted -- the byte stream is the contract). escapy uses
// Noto / system fonts; we use the bundled Liberation pair, so the
// rendered glyphs differ, but the layout logic (pitch, spacing,
// scaling, charset switches) is what the test actually exercises.
//
// Most of these tests need a larger page than the default 5x5
// because they print long pangrams that need to wrap and stack
// vertically across many sections.

constexpr int EscapyPageDpi         = 360;
constexpr double EscapyPageWidthIn  = 9.0;
constexpr double EscapyPageHeightIn = 12.0;

// Append a sequence of byte vectors, joined by CRLF. Mirrors the
// escapy idiom `b"\r\n".join(lines)`.
void append_crlf_joined(std::vector<uint8_t>& bytes,
                        const std::vector<std::vector<uint8_t>>& lines)
{
	bool first = true;
	for (const auto& line : lines) {
		if (!first) {
			bytes.push_back(0x0D);
			bytes.push_back(0x0A);
		}
		bytes.insert(bytes.end(), line.begin(), line.end());
		first = false;
	}
}

// Build a byte vector from an initializer-list of bytes. Saves
// boilerplate in the per-line vectors below.
std::vector<uint8_t> b(std::initializer_list<uint8_t> seq)
{
	return std::vector<uint8_t>(seq.begin(), seq.end());
}

// Build a byte vector from a string literal.
std::vector<uint8_t> b(std::string_view s)
{
	return std::vector<uint8_t>(s.begin(), s.end());
}

// Concatenate byte vectors.
std::vector<uint8_t> cat(std::initializer_list<std::vector<uint8_t>> parts)
{
	std::vector<uint8_t> out;
	for (const auto& p : parts) {
		out.insert(out.end(), p.begin(), p.end());
	}
	return out;
}

// Full-page paragraphed lorem-ipsum body across four typefaces (Roman,
// Sans Serif, Courier, Prestige) on a standard 8.5" x 11" Letter page
// with default 0.25" margins (8.0" x 10.5" printable area, 1/6" line
// feed, 64 printable rows). Body text is pre-wrapped at word
// boundaries so the printer's own right-margin char-by-char wrap never
// triggers — words stay intact, no continuation line starts with a
// stray space. Inline runs of bold, italic, underline, and
// double-strike sit on a single line each. Each font section's
// heading line is double-underlined (ESC ( - ... 1 2) for emphasis.
// Body is repeated cyclically per font to fill close to a full page,
// so drift in character advance or line spacing shows up as
// wrap-shift, uneven paragraph spacing, or a premature page break.
//
// The render_lorem_ipsum_body helper builds the byte stream; the two
// TEST_F entries flip ESC p 0/1 to exercise the fixed-pitch and
// proportional-spacing pipelines from the same content.

namespace {
std::vector<uint8_t> build_lorem_ipsum_bytes(const bool proportional)
{
	const auto bold_on   = b({0x1B, 'E'});
	const auto bold_off  = b({0x1B, 'F'});
	const auto ital_on   = b({0x1B, '4'});
	const auto ital_off  = b({0x1B, '5'});
	const auto ul_on     = b({0x1B, '-', 1});
	const auto ul_off    = b({0x1B, '-', 0});
	const auto dstrk_on  = b({0x1B, 'G'});
	const auto dstrk_off = b({0x1B, 'H'});

	// ESC ( - 3 0 1 d1 d2: d1=1 underline location, d2=2 double
	// continuous line. d2=0 clears the underline.
	const auto du_on  = b({0x1B, '(', '-', 3, 0, 1, 1, 2});
	const auto du_off = b({0x1B, '(', '-', 3, 0, 1, 1, 0});

	struct Face {
		uint8_t id;
		std::string_view name;
	};
	const Face faces[] = {
	        {0,      "Roman"},
	        {1, "Sans Serif"},
	        {2,    "Courier"},
	        {3,   "Prestige"},
	};

	// Two pre-wrapped bodies. The fixed-pitch one wraps at ~80 visible
	// chars per line (the 10 cpi column count over an 8.0" printable
	// width). The proportional one wraps wider — ~95 chars — to push
	// closer to the right margin under proportional advance widths,
	// while still leaving ~1" of slack so a swap to a wider font
	// doesn't force the printer's auto-wrap to kick in mid-word.
	const auto fixed_body = [&]() -> std::vector<std::vector<uint8_t>> {
		std::vector<std::vector<uint8_t>> ls;

		// Paragraph 1 — bold inside line 1, italic inside line 3.
		ls.push_back(cat({
		        b("Lorem ipsum dolor sit amet, "),
		        bold_on,
		        b("consectetur adipiscing elit"),
		        bold_off,
		        b(", sed do"),
		}));
		ls.push_back(b("eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad"));
		ls.push_back(cat({
		        b("minim veniam, quis nostrud exercitation "),
		        ital_on,
		        b("ullamco laboris nisi"),
		        ital_off,
		        b(" ut"),
		}));
		ls.push_back(b("aliquip ex ea commodo consequat."));
		ls.push_back({});

		// Paragraph 2 — underline inside line 1, double-strike inside
		// line 2.
		ls.push_back(cat({
		        b("Duis aute irure dolor in "),
		        ul_on,
		        b("reprehenderit in voluptate"),
		        ul_off,
		        b(" velit esse cillum"),
		}));
		ls.push_back(cat({
		        b("dolore eu fugiat nulla pariatur. "),
		        dstrk_on,
		        b("Excepteur sint occaecat"),
		        dstrk_off,
		        b(" cupidatat non"),
		}));
		ls.push_back(b("proident, sunt in culpa qui officia deserunt mollit anim id est laborum."));
		ls.push_back({});

		// Paragraph 3 — plain flowing text.
		ls.push_back(b("Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium"));
		ls.push_back(b("doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore"));
		ls.push_back(b("veritatis et quasi architecto beatae vitae dicta sunt explicabo."));

		return ls;
	}();

	const auto proportional_body = [&]() -> std::vector<std::vector<uint8_t>> {
		std::vector<std::vector<uint8_t>> ls;

		// Paragraph 1.
		ls.push_back(cat({
		        b("Lorem ipsum dolor sit amet, "),
		        bold_on,
		        b("consectetur adipiscing elit"),
		        bold_off,
		        b(", sed do eiusmod tempor incididunt ut"),
		}));
		ls.push_back(b("labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation"));
		ls.push_back(cat({
		        ital_on,
		        b("ullamco laboris nisi"),
		        ital_off,
		        b(" ut aliquip ex ea commodo consequat."),
		}));
		ls.push_back({});

		// Paragraph 2.
		ls.push_back(cat({
		        b("Duis aute irure dolor in "),
		        ul_on,
		        b("reprehenderit in voluptate"),
		        ul_off,
		        b(" velit esse cillum dolore"),
		}));
		ls.push_back(cat({
		        b("eu fugiat nulla pariatur. "),
		        dstrk_on,
		        b("Excepteur sint occaecat"),
		        dstrk_off,
		        b(" cupidatat non proident, sunt in"),
		}));
		ls.push_back(b("culpa qui officia deserunt mollit anim id est laborum."));
		ls.push_back({});

		// Paragraph 3.
		ls.push_back(b("Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque"));
		ls.push_back(b("laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi"));
		ls.push_back(b("architecto beatae vitae dicta sunt explicabo."));

		return ls;
	}();

	const auto& body = proportional ? proportional_body : fixed_body;

	constexpr int RowsPerPage     = 64;
	const int num_fonts           = static_cast<int>(std::size(faces));
	const int inter_font_blanks   = num_fonts - 1;
	const int budget_after_blanks = RowsPerPage - inter_font_blanks;
	const int per_font_total      = budget_after_blanks / num_fonts;
	const int extra_first_n       = budget_after_blanks % num_fonts;

	const std::string_view mode_label = proportional
	                                          ? " -- proportional spacing"
	                                          : " -- 10 cpi fixed pitch";

	std::vector<std::vector<uint8_t>> lines;

	for (int i = 0; i < num_fonts; ++i) {
		const auto& f = faces[i];

		lines.push_back(cat(
		        {b({0x1B, 'k', f.id}), du_on, b(f.name), b(mode_label), du_off}));

		const int per_font_rows = per_font_total +
		                          (i < extra_first_n ? 1 : 0);
		const int body_rows = per_font_rows - 1; // minus heading

		for (int j = 0; j < body_rows; ++j) {
			lines.push_back(body[static_cast<size_t>(j) % body.size()]);
		}

		if (i + 1 < num_fonts) {
			lines.push_back({});
		}
	}

	std::vector<uint8_t> bytes;
	// Init bytes execute on row 0 without consuming it for visible text.
	const uint8_t prop_byte = proportional ? 1 : 0;
	append(bytes, {0x1B, '@', 0x1B, 'p', prop_byte, 0x1B, 'P'});
	append_crlf_joined(bytes, lines);
	return bytes;
}
} // namespace

TEST_F(PrinterRenderTest, Text_LoremIpsumParagraphs_FixedPitch)
{
	const auto bytes    = build_lorem_ipsum_bytes(false);
	const auto rendered = render_with_page(bytes, 360, 8.5, 11.0);
	expect_matches_reference(rendered, "Text_LoremIpsumParagraphs_FixedPitch");
}

TEST_F(PrinterRenderTest, Text_LoremIpsumParagraphs_Proportional)
{
	const auto bytes    = build_lorem_ipsum_bytes(true);
	const auto rendered = render_with_page(bytes, 360, 8.5, 11.0);
	expect_matches_reference(rendered, "Text_LoremIpsumParagraphs_Proportional");
}

// Ported from escapy test_fonts. Switches typeface via ESC k across
// the full set of ESC/P2 font codes (Roman, Sans serif, Courier,
// Prestige, OCR-B, OCR-A, Orator, Script-C, Roman T) and prints a
// pangram with each. Also covers the fallback path for unsupported
// font codes (ESC k 0x1F).
TEST_F(PrinterRenderTest, Text_Fonts_Escapy)
{
	const auto pangram = b(
	        "The quick brown fox jumps over the lazy dog; "
	        "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG; "
	        "1234567890");
	const auto cancel_left_margin = b({0x1B, 'l', 0x00});
	// ESC X m=24 nL=16 nH=0: pitch = 360/24 = 15 cpi alongside the 8-pt
	// point size so the cell width matches the natural width of the
	// 8-pt monospace glyphs. The pangram fits on one line without
	// wrapping (101 chars at 15 cpi = 6.73", well inside the 8.5"
	// printable area of the escapy page).
	const auto point_8 = b({0x1B, 'X', 24, 0x10, 0x00});
	const auto roman   = b({0x1B, 'k', 0x00});

	std::vector<uint8_t> bytes;
	append_crlf_joined(
	        bytes,
	        {
	                cat({b({0x1B, '@'}), cancel_left_margin, point_8}),

	                b("Default font (Roman):"),
	                pangram,
	                b(""),

	                b("Roman:"),
	                cat({roman, pangram}),
	                b(""),

	                cat({roman, b("Sans serif:")}),
	                cat({b({0x1B, 'k', 0x01}), pangram}),
	                b(""),

	                cat({roman, b("Courier:")}),
	                cat({b({0x1B, 'k', 0x02}), pangram}),
	                b(""),

	                cat({roman, b("Prestige:")}),
	                cat({b({0x1B, 'k', 0x03}), pangram}),
	                b(""),

	                cat({roman, b("OCR-B:")}),
	                cat({b({0x1B, 'k', 0x05}), pangram}),
	                b(""),

	                cat({roman, b("OCR-A:")}),
	                cat({b({0x1B, 'k', 0x06}), pangram}),
	                b(""),

	                cat({roman, b("Orator:")}),
	                cat({b({0x1B, 'k', 0x07}), pangram}),
	                b(""),

	                cat({roman, b("Script-C:")}),
	                cat({b({0x1B, 'k', 0x09}), pangram}),
	                b(""),

	                cat({roman, b("Roman T:")}),
	                cat({b({0x1B, 'k', 0x0A}), pangram}),
	                b(""),

	                cat({roman, b("SV Jittra (not available, fallback to default):")}),
	                cat({b({0x1B, 'k', 0x1F}), pangram}),
	        });

	const auto rendered = render_with_page(bytes,
	                                       EscapyPageDpi,
	                                       EscapyPageWidthIn,
	                                       EscapyPageHeightIn);
	expect_matches_reference(rendered, "Text_Fonts_Escapy");
}

// Ported from escapy test_character_pitch (autoscaling_forced
// variant). Exercises non-multipoint pitch selection via ESC P (10
// cpi), ESC M (12 cpi), ESC g (15 cpi), with and without double-
// width (ESC W) and condensed (SI / DC2). The forced-fallback case
// is the one where the printer must autoscale a non-condensed font
// instead of switching to a real condensed variant.
TEST_F(PrinterRenderTest, Text_CharacterPitchAutoscalingForced_Escapy)
{
	const auto lorem = b(
	        "Lorem ipsum dolor sit amet, consectetur "
	        "adipiscing elit.\nSed non risus. Suspendisse...");
	const auto select_10cpi         = b({0x1B, 'P'});
	const auto select_12cpi         = b({0x1B, 'M'});
	const auto select_15cpi         = b({0x1B, 'g'});
	const auto double_width_m       = b({0x1B, 'W', 0x01});
	const auto reset_double_width_m = b({0x1B, 'W', 0x00});
	const auto select_condensed     = b({0x0F});
	const auto unset_condensed      = b({0x12});

	std::vector<uint8_t> bytes;
	append_crlf_joined(
	        bytes,
	        {
	                b("select_10cpi+double_width_5cpi"),
	                cat({select_10cpi, double_width_m, lorem, reset_double_width_m}),
	                b(""),

	                b("select_12cpi+double_width_6cpi"),
	                cat({select_12cpi, double_width_m, lorem, reset_double_width_m, select_10cpi}),
	                b(""),

	                b("select_15cpi+double_width_7.5cpi"),
	                cat({select_15cpi, double_width_m, lorem, reset_double_width_m, select_10cpi}),
	                b(""),

	                b("select_10cpi_10cpi"),
	                cat({select_10cpi, lorem}),
	                b(""),

	                b("select_12cpi_12cpi"),
	                cat({select_12cpi, lorem, select_10cpi}),
	                b(""),

	                b("select_15cpi_15cpi"),
	                cat({select_15cpi, lorem, select_10cpi}),
	                b(""),

	                b("select_10cpi+condensed_17.14cpi"),
	                cat({select_10cpi, select_condensed, lorem, unset_condensed}),
	                b(""),

	                b("select_12cpi+condensed_20cpi"),
	                cat({select_12cpi, select_condensed, lorem}),
	        });

	const auto rendered = render_with_page(bytes,
	                                       EscapyPageDpi,
	                                       EscapyPageWidthIn,
	                                       EscapyPageHeightIn);
	expect_matches_reference(rendered,
	                         "Text_CharacterPitchAutoscalingForced_Escapy");
}

// Ported from escapy test_multipoint_mode. Exercises pitch and
// point-size selection in multipoint mode (ESC X with non-zero
// parameters), HMI override (ESC c), and proportional mode toggle
// via ESC X m=1.
TEST_F(PrinterRenderTest, Text_MultipointMode_Escapy)
{
	const auto lorem = b(
	        "Lorem ipsum dolor sit amet, consectetur "
	        "adipiscing elit.\nSed non risus. Suspendisse...");

	const auto pitch_5     = b({0x1B, 'X', 0x48});
	const auto pitch_6     = b({0x1B, 'X', 0x3C});
	const auto pitch_7_5   = b({0x1B, 'X', 0x30});
	const auto pitch_10    = b({0x1B, 'X', 0x24});
	const auto pitch_12    = b({0x1B, 'X', 0x1E});
	const auto pitch_15    = b({0x1B, 'X', 0x18});
	const auto pitch_17_14 = b({0x1B, 'X', 0x15});
	const auto pitch_20    = b({0x1B, 'X', 0x12});

	const auto point_10_5 = b({0x15, 0x00});
	const auto point_21   = b({0x2A, 0x00});

	const auto multipoint_only        = b({0x1B, 'X', 0x00, 0x00, 0x00});
	const auto multipoint_prop_enable = b({0x1B, 'X', 0x01, 0x00, 0x00});
	const auto select_15cpi           = b({0x1B, 'g'});

	const auto default_state = cat({pitch_10, point_10_5});
	const auto hmi_15        = b({0x1B, 'c', 0x18, 0x00}); // HMI = 24/360"

	std::vector<uint8_t> bytes;
	append_crlf_joined(
	        bytes,
	        {
	                b("equiv select_10cpi+double_width - 5cpi"),
	                cat({pitch_5, point_10_5, lorem, default_state}),
	                b(""),

	                b("equiv select_12cpi+double_width - 6cpi"),
	                cat({pitch_6, point_10_5, lorem, default_state}),
	                b(""),

	                b("equiv select_15cpi+double_width - 7.5cpi"),
	                cat({pitch_7_5, point_10_5, lorem, default_state}),
	                b(""),

	                b("equiv select_10cpi - 10cpi"),
	                cat({pitch_10, point_10_5, lorem, default_state}),
	                b(""),

	                b("equiv select_12cpi - 12cpi"),
	                cat({pitch_12, point_10_5, lorem, default_state}),
	                b(""),

	                b("equiv select_10cpi+condensed - 17.14cpi"),
	                cat({pitch_17_14, point_10_5, lorem, default_state}),
	                b(""),

	                b("equiv select_12cpi+condensed - 20cpi"),
	                cat({pitch_20, point_10_5, lorem, default_state}),
	                b(""),
	                b(""),

	                b("15cpi 10.5pt - normal size"),
	                cat({pitch_15, point_10_5, hmi_15, lorem, default_state}),
	                b(""),

	                b("15cpi 10.5pt - reduced size (2/3 size)"),
	                cat({pitch_15, point_10_5, lorem, default_state}),
	                b(""),

	                b("15cpi 10.5pt - reduced size (2/3 size)"),
	                cat({select_15cpi, multipoint_only, lorem, default_state}),
	                b(""),
	                b(""),

	                b("15cpi 21pt - normal size"),
	                cat({pitch_15, point_21, hmi_15, lorem, default_state}),
	                b(""),

	                b("15cpi 21pt - reduced size (2/3 size)"),
	                cat({pitch_15, point_21, lorem, default_state}),
	                b(""),
	                b(""),

	                b("proportional mode (#1)"),
	                cat({multipoint_prop_enable, lorem, default_state}),
	                b(""),

	                b("proportional mode; pitch ignored; identical to (#1)"),
	                cat({pitch_15, point_10_5, multipoint_prop_enable, lorem, default_state}),
	                b(""),

	                b("proportional mode; pitch ignored; identical to (#1)"),
	                cat({pitch_15, point_10_5, multipoint_prop_enable, hmi_15, lorem, default_state}),
	        });

	const auto rendered = render_with_page(bytes,
	                                       EscapyPageDpi,
	                                       EscapyPageWidthIn,
	                                       EscapyPageHeightIn);
	expect_matches_reference(rendered, "Text_MultipointMode_Escapy");
}

// Ported from escapy test_charset_tables. Exercises ESC ( t (assign
// character table) and ESC t (select character table) across a long
// list of pangrams in different code pages and scripts. Some scripts
// (Devanagari) trigger font-fallback paths; some (Arabic, Hebrew,
// Thai) exercise high-byte handling without requiring full shaping
// support.
TEST_F(PrinterRenderTest, Text_CharsetTables_Escapy)
{
	// Pre-encoded byte sequences for each pangram, mirroring
	// escapy/tests/test_text_commands.test_charset_tables. Each block
	// is the output of Python's str.encode(<codec>) for the pangram
	// shown in its comment header — these are static and never change.
	const auto english_pangram = b("The quick brown fox jumps over the lazy dog.");

	// Italic table is symmetric (upper half == lower half + 0x80).
	std::vector<uint8_t> english_italic_pangram;
	for (const auto c : english_pangram) {
		english_italic_pangram.push_back(static_cast<uint8_t>(c + 0x80));
	}

	// "Portez ce vieux whisky au juge blond qui fume. Voie ambigüe d'un
	//  coeur qui au zéphyr préfère les jattes de kiwis."
	const auto french_pangram = b({
	        0x50, 0x6f, 0x72, 0x74, 0x65, 0x7a, 0x20, 0x63, 0x65, 0x20, 0x76, 0x69,
	        0x65, 0x75, 0x78, 0x20, 0x77, 0x68, 0x69, 0x73, 0x6b, 0x79, 0x20, 0x61,
	        0x75, 0x20, 0x6a, 0x75, 0x67, 0x65, 0x20, 0x62, 0x6c, 0x6f, 0x6e, 0x64,
	        0x20, 0x71, 0x75, 0x69, 0x20, 0x66, 0x75, 0x6d, 0x65, 0x2e, 0x20, 0x56,
	        0x6f, 0x69, 0x65, 0x20, 0x61, 0x6d, 0x62, 0x69, 0x67, 0x81, 0x65, 0x20,
	        0x64, 0x27, 0x75, 0x6e, 0x20, 0x63, 0x6f, 0x65, 0x75, 0x72, 0x20, 0x71,
	        0x75, 0x69, 0x20, 0x61, 0x75, 0x20, 0x7a, 0x82, 0x70, 0x68, 0x79, 0x72,
	        0x20, 0x70, 0x72, 0x82, 0x66, 0x8a, 0x72, 0x65, 0x20, 0x6c, 0x65, 0x73,
	        0x20, 0x6a, 0x61, 0x74, 0x74, 0x65, 0x73, 0x20, 0x64, 0x65, 0x20, 0x6b,
	        0x69, 0x77, 0x69, 0x73, 0x2e,
	});

	// "Příliš žluťoučký kůň úpěl ďábelské ódy."
	const auto czech_pangram = b({
	        0x50, 0xfd, 0xa1, 0x6c, 0x69, 0xe7, 0x20, 0xa7, 0x6c, 0x75, 0x9c, 0x6f,
	        0x75, 0x9f, 0x6b, 0xec, 0x20, 0x6b, 0x85, 0xe5, 0x20, 0xa3, 0x70, 0xd8,
	        0x6c, 0x20, 0xd4, 0xa0, 0x62, 0x65, 0x6c, 0x73, 0x6b, 0x82, 0x20, 0xa2,
	        0x64, 0x79, 0x2e,
	});

	// "Ξεσκεπάζω την ψυχοφθόρα βδελυγμία." in cp737.
	const auto greek_pangram_cp737 = b({
	        0x8d, 0x9c, 0xa9, 0xa1, 0x9c, 0xa7, 0xe1, 0x9d, 0xe0, 0x20, 0xab, 0x9e,
	        0xa4, 0x20, 0xaf, 0xac, 0xae, 0xa6, 0xad, 0x9f, 0xe6, 0xa8, 0x98, 0x20,
	        0x99, 0x9b, 0x9c, 0xa2, 0xac, 0x9a, 0xa3, 0xe5, 0x98, 0x2e,
	});

	// "Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich."
	const auto german_pangram = b({
	        0x56, 0x69, 0x63, 0x74, 0x6f, 0x72, 0x20, 0x6a, 0x61, 0x67, 0x74, 0x20,
	        0x7a, 0x77, 0xf6, 0x6c, 0x66, 0x20, 0x42, 0x6f, 0x78, 0x6b, 0xe4, 0x6d,
	        0x70, 0x66, 0x65, 0x72, 0x20, 0x71, 0x75, 0x65, 0x72, 0x20, 0xfc, 0x62,
	        0x65, 0x72, 0x20, 0x64, 0x65, 0x6e, 0x20, 0x67, 0x72, 0x6f, 0xdf, 0x65,
	        0x6e, 0x20, 0x53, 0x79, 0x6c, 0x74, 0x65, 0x72, 0x20, 0x44, 0x65, 0x69,
	        0x63, 0x68, 0x2e,
	});

	// "Kæmi ný öxi hér, ykist þjófum nú bæði víl og ádrepa."
	const auto icelandic_pangram = b({
	        0x4b, 0x91, 0x6d, 0x69, 0x20, 0x6e, 0x98, 0x20, 0x94, 0x78, 0x69, 0x20,
	        0x68, 0x82, 0x72, 0x2c, 0x20, 0x79, 0x6b, 0x69, 0x73, 0x74, 0x20, 0x95,
	        0x6a, 0xa2, 0x66, 0x75, 0x6d, 0x20, 0x6e, 0xa3, 0x20, 0x62, 0x91, 0x8c,
	        0x69, 0x20, 0x76, 0xa1, 0x6c, 0x20, 0x6f, 0x67, 0x20, 0xa0, 0x64, 0x72,
	        0x65, 0x70, 0x61, 0x2e,
	});

	// "Pijamalı hasta yağız şoföre çabucak güvendi."
	const auto turkish_pangram = b({
	        0x50, 0x69, 0x6a, 0x61, 0x6d, 0x61, 0x6c, 0x8d, 0x20, 0x68, 0x61, 0x73,
	        0x74, 0x61, 0x20, 0x79, 0x61, 0xa7, 0x8d, 0x7a, 0x20, 0x9f, 0x6f, 0x66,
	        0x94, 0x72, 0x65, 0x20, 0x87, 0x61, 0x62, 0x75, 0x63, 0x61, 0x6b, 0x20,
	        0x67, 0x81, 0x76, 0x65, 0x6e, 0x64, 0x69, 0x2e,
	});

	// "Съешь ещё этих мягких французских булок, да выпей же чаю"
	const auto russian_pangram = b({
	        0x91, 0xea, 0xa5, 0xe8, 0xec, 0x20, 0xa5, 0xe9, 0xf1, 0x20, 0xed, 0xe2,
	        0xa8, 0xe5, 0x20, 0xac, 0xef, 0xa3, 0xaa, 0xa8, 0xe5, 0x20, 0xe4, 0xe0,
	        0xa0, 0xad, 0xe6, 0xe3, 0xa7, 0xe1, 0xaa, 0xa8, 0xe5, 0x20, 0xa1, 0xe3,
	        0xab, 0xae, 0xaa, 0x2c, 0x20, 0xa4, 0xa0, 0x20, 0xa2, 0xeb, 0xaf, 0xa5,
	        0xa9, 0x20, 0xa6, 0xa5, 0x20, 0xe7, 0xa0, 0xee,
	});

	const auto table_0            = b({0x1B, 't', 0x00});
	const auto table_1            = b({0x1B, 't', 0x01});
	const auto table_3            = b({0x1B, 't', 0x03});
	// ESC X m=24 (15 cpi) nL=16 nH=0 (8 pt) — tightens spacing so the
	// proportional glyphs don't sit in oversized 10-cpi cells.
	const auto pitch_15_size_8    = b({0x1B, 'X', 0x18, 0x10, 0x00});
	const auto cancel_left_margin = b({0x1B, 'l', 0x00});
	const auto sans_serif         = b({0x1B, 'k', 0x01});

	const auto assign_t1 = [](uint8_t d2, uint8_t d3) {
		return b({0x1B, '(', 't', 0x03, 0x00, 0x01, d2, d3});
	};

	std::vector<uint8_t> bytes;
	append_crlf_joined(
	        bytes,
	        {
	                b({0x1B, '@'}),
	                cancel_left_margin,
	                pitch_15_size_8,
	                sans_serif,

	                b("English, cp437 (default)"),
	                english_pangram,
	                b(""),

	                cat({table_3, b("English table 2, cp437 (default)")}),
	                cat({table_1, english_pangram}),
	                b(""),

	                cat({table_3, b("Italic table 0, italic (default)")}),
	                cat({table_0, english_italic_pangram}),
	                b(""),

	                cat({table_3, b("French, cp863")}),
	                cat({table_1, assign_t1(0x08, 0x00), french_pangram}),
	                b(""),

	                cat({table_3, b("Czech, cp852")}),
	                cat({table_1, assign_t1(0x0A, 0x00), czech_pangram}),
	                b(""),

	                cat({table_3, b("Greek, cp737")}),
	                cat({table_1, assign_t1(0x01, 0x10), greek_pangram_cp737}),
	                b(""),

	                cat({table_3, b("German, latin_1")}),
	                cat({table_1, assign_t1(0x1D, 0x10), german_pangram}),
	                b(""),

	                cat({table_3, b("Icelandic, cp861")}),
	                cat({table_1, assign_t1(0x18, 0x00), icelandic_pangram}),
	                b(""),

	                cat({table_3, b("Turkish, cp857")}),
	                cat({table_1, assign_t1(0x0B, 0x00), turkish_pangram}),
	                b(""),

	                cat({table_3, b("Russian, cp866")}),
	                cat({table_1, assign_t1(0x0E, 0x00), russian_pangram}),
	                b(""),

	                cat({table_3,
	                     b("Greek - Not supported charset 4,0 (should not crash)")}),
	                cat({table_1,
	                     assign_t1(0x04, 0x00),
	                     table_1,
	                     english_pangram}),
	        });

	const auto rendered = render_with_page(bytes,
	                                       EscapyPageDpi,
	                                       EscapyPageWidthIn,
	                                       EscapyPageHeightIn);
	expect_matches_reference(rendered, "Text_CharsetTables_Escapy");
}

// Ported from escapy test_set_intercharacter_space. Exercises ESC SP
// across the legal 0-127 range in steps of 20, both in the default
// state and combined with superscripting (ESC S 0). A NULL byte
// breaks the alphabet in two to verify it does not perturb the
// per-character spacing.
TEST_F(PrinterRenderTest, Text_IntercharacterSpace_Escapy)
{
	const auto intercharacter_space_prefix = b({0x1B, ' '});
	const auto reset_intercharacter_space  = b({0x1B, 'p', 0x00});
	const auto enable_upperscripting       = b({0x1B, 'S', 0x00});
	const auto disable_upperscripting      = b({0x1B, 'T'});
	const auto point_8  = b({0x1B, 'X', 0x00, 0x10, 0x00});
	const auto alphabet = b({
	        'a', 'b',  'c', 'd', 'e', 'f', 'g', 'h', 'i',
	        'j', 0x00, 'k', 'l', 'm', 'n', 'o', 'p', 'q',
	        'r', 's',  't', 'u', 'v', 'w', 'x', 'z',
	});

	std::vector<std::vector<uint8_t>> lines;

	lines.push_back(b({0x1B, '@'}));

	lines.push_back(cat({point_8,
	                     b("Intercharacter space from 0 to 128 (steps 20)"),
	                     reset_intercharacter_space}));

	for (int i = 0; i < 128; i += 20) {
		lines.push_back(cat({intercharacter_space_prefix,
		                     b({static_cast<uint8_t>(i)}),
		                     alphabet}));
	}

	lines.push_back(cat({point_8,
	                     b("Intercharacter space from 0 to 128 "
	                       "(steps 20) for scripting text"),
	                     reset_intercharacter_space,
	                     enable_upperscripting}));

	for (int i = 0; i < 128; i += 20) {
		lines.push_back(cat({intercharacter_space_prefix,
		                     b({static_cast<uint8_t>(i)}),
		                     alphabet}));
	}

	lines.push_back(disable_upperscripting);

	std::vector<uint8_t> bytes;
	append_crlf_joined(bytes, lines);

	const auto rendered = render_with_page(bytes,
	                                       EscapyPageDpi,
	                                       EscapyPageWidthIn,
	                                       EscapyPageHeightIn);
	expect_matches_reference(rendered, "Text_IntercharacterSpace_Escapy");
}

// Ported from escapy test_double_width_height (ESCP2 variant).
// Exercises ESC W (double width) and ESC w (double height) singly
// and combined, then interleaves them with superscripting (ESC S /
// ESC T) to verify scripting persists through double-height under
// ESC/P2 rules (under 9-pin rules, double-height suspends scripting
// instead — see the 9-pin sister test).
TEST_F(PrinterRenderTest, Text_DoubleWidthHeight_Escapy)
{
	const auto reset_intercharacter_space = b({0x1B, 'p', 0x00});
	const auto point_8                = b({0x1B, 'X', 0x00, 0x10, 0x00});
	const auto point_21               = b({0x1B, 'X', 0x00, 0x2A, 0x00});
	const auto double_width_m         = b({0x1B, 'W', 0x01});
	const auto reset_double_width_m   = b({0x1B, 'W', 0x00});
	const auto double_height          = b({0x1B, 'w', 0x01});
	const auto reset_double_height    = b({0x1B, 'w', 0x00});
	const auto enable_upperscripting  = b({0x1B, 'S', 0x00});
	const auto disable_upperscripting = b({0x1B, 'T'});

	const auto pangram = b("The quick brown fox jumps over the lazy dog");

	std::vector<uint8_t> bytes;
	append_crlf_joined(
	        bytes,
	        {
	                b({0x1B, '@'}),

	                cat({point_8, b("Normal width (10.5 cpi)"), reset_intercharacter_space}),
	                pangram,

	                cat({point_8,
	                     b("Double point-size (21 cpi)"),
	                     reset_intercharacter_space}),
	                cat({point_21, pangram}),

	                cat({point_8,
	                     b("Double width (ESC W) (horizontal scale * 2)"),
	                     reset_intercharacter_space}),
	                cat({double_width_m, pangram, reset_double_width_m}),

	                cat({point_8,
	                     b("Double height (ESC w) (point-size * 2 + horizontal "
	                       "scale / 2)"),
	                     reset_intercharacter_space}),
	                cat({double_height, pangram, reset_double_height}),

	                cat({point_8,
	                     b("Double height + width (point-size * 2 + horizontal "
	                       "scale * 2)"),
	                     reset_intercharacter_space}),
	                cat({double_width_m,
	                     double_height,
	                     pangram,
	                     reset_double_height,
	                     reset_double_width_m}),

	                cat({point_8,
	                     b("Back to normal width (10.5 cpi)"),
	                     reset_intercharacter_space}),
	                pangram,
	                b("\r\n"),

	                cat({point_8,
	                     b("NOTE: In ESCP2, double-height can be used with "
	                       "upper/subscripting, condensed font and Draft printing.")}),

	                cat({point_8,
	                     b("upperscripting enabled for ref"),
	                     reset_intercharacter_space}),
	                cat({enable_upperscripting, pangram, disable_upperscripting}),

	                cat({point_8,
	                     b("double-height enabled for ref"),
	                     reset_intercharacter_space}),
	                cat({double_height, pangram, reset_double_height}),

	                cat({point_8,
	                     b("upperscripting should have effect in ESCP2"),
	                     reset_intercharacter_space}),
	                cat({enable_upperscripting,
	                     double_height,
	                     pangram,
	                     reset_double_height,
	                     disable_upperscripting}),

	                cat({point_8,
	                     b("in ESCP2 make sure scripting is not interrupted "
	                       "during double-height"),
	                     reset_intercharacter_space}),
	                cat({enable_upperscripting,
	                     b("The quick "),
	                     double_height,
	                     b("brown fox jumps "),
	                     disable_upperscripting,
	                     reset_double_height,
	                     b("over the lazy dog")}),

	                cat({point_8,
	                     b("upperscripting should have effect in ESCP2"),
	                     reset_intercharacter_space}),
	                cat({double_height,
	                     enable_upperscripting,
	                     pangram,
	                     reset_double_height,
	                     disable_upperscripting}),

	                cat({point_8,
	                     b("upperscripting should have effect in the first "
	                       "part in ESCP2"),
	                     reset_intercharacter_space}),
	                cat({double_height,
	                     enable_upperscripting,
	                     pangram,
	                     reset_double_height,
	                     pangram,
	                     disable_upperscripting}),
	        });

	const auto rendered = render_with_page(bytes,
	                                       EscapyPageDpi,
	                                       EscapyPageWidthIn,
	                                       EscapyPageHeightIn);
	expect_matches_reference(rendered, "Text_DoubleWidthHeight_Escapy");
}

// ---------------------------------------------------------------------------
// A CP437 character chart. 16x16 grid of all 256 byte values, with
// box-drawing borders (also from CP437). Demonstrates the high-bit character
// region — accented letters, math symbols, Greek letters, and the box-drawing
// characters themselves.
//
// Control bytes (0x00-0x1F, 0x7F) are rendered as '.' placeholders because we
// don't implement ESC ( ^ "print as character" — those bytes would otherwise
// be interpreted as commands.
TEST_F(PrinterRenderTest, Text_Cp437CharacterChart)
{
	std::vector<uint8_t> bytes = {};

	append(bytes, {0x1B, '@'});    // reset printer
	append(bytes, {0x1B, 'p', 0}); // fixed pitch
	append(bytes, {0x1B, 'k', 2}); // Courier (monospace)
	append(bytes, {0x1B, 'M'});    // 12 cpi
	append(bytes, {0x1B, '2'});    // 1/6" line spacing (explicit default)

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	// CP437 box-drawing characters.
	constexpr uint8_t Hs  = 0xC4; // ─ single horizontal
	constexpr uint8_t Vs  = 0xB3; // │ single vertical
	constexpr uint8_t Ts  = 0xDA; // ┌
	constexpr uint8_t TRs = 0xBF; // ┐
	constexpr uint8_t BLs = 0xC0; // └
	constexpr uint8_t BRs = 0xD9; // ┘
	constexpr uint8_t Xs  = 0xC5; // ┼
	constexpr uint8_t TCs = 0xC2; // ┬
	constexpr uint8_t BCs = 0xC1; // ┴
	constexpr uint8_t LCs = 0xC3; // ├
	constexpr uint8_t RCs = 0xB4; // ┤

	constexpr uint8_t Hd  = 0xCD; // ═
	constexpr uint8_t Vd  = 0xBA; // ║
	constexpr uint8_t TLd = 0xC9; // ╔
	constexpr uint8_t TRd = 0xBB; // ╗
	constexpr uint8_t BLd = 0xC8; // ╚
	constexpr uint8_t BRd = 0xBC; // ╝
	constexpr uint8_t TCd = 0xCB; // ╦
	constexpr uint8_t BCd = 0xCA; // ╩
	constexpr uint8_t Xd  = 0xCE; // ╬

	// ESC ( ^ "Print Data as Characters": the next nL+nH*256 bytes
	// bypass ProcessCommandChar so 0x00..0x1F render as their cp437
	// graphics instead of being eaten as control codes (BS / HT / LF
	// / CR / ESC etc.). NUL is the only byte that still produces no
	// glyph after the bypass, so we substitute it with a dot to keep
	// the grid aligned.
	auto append_print_as_chars = [&](uint8_t b) {
		append(bytes, {0x1B, '(', '^', 0x01, 0x00});
		bytes.push_back(b == 0x00 ? static_cast<uint8_t>('.') : b);
	};

	// Title.
	append(bytes, "CP437 Character Chart");
	nl();
	nl();

	// Column header row: "      0 1 2 3 ... F".
	append(bytes, "    ");
	for (int c = 0; c < 16; ++c) {
		append(bytes, " ");
		const char digit = static_cast<char>(c < 10 ? '0' + c
		                                            : 'A' + (c - 10));
		bytes.push_back(static_cast<uint8_t>(digit));
	}
	nl();

	// Double-line horizontal rule below the column header.
	append(bytes, "   ");
	for (int c = 0; c < 33; ++c) {
		bytes.push_back(Hd);
	}
	nl();

	// 16 data rows: row label + 16 character cells.
	for (int r = 0; r < 16; ++r) {
		const char row_digit = static_cast<char>(r < 10 ? '0' + r
		                                                : 'A' + (r - 10));
		append(bytes, " ");
		bytes.push_back(static_cast<uint8_t>(row_digit));
		append(bytes, "x");
		for (int c = 0; c < 16; ++c) {
			append(bytes, " ");
			const uint8_t b = static_cast<uint8_t>(r * 16 + c);
			append_print_as_chars(b);
		}
		nl();
	}

	// Double-line horizontal rule below the data.
	append(bytes, "   ");
	for (int c = 0; c < 33; ++c) {
		bytes.push_back(Hd);
	}
	nl();
	nl();

	// Demo block: side-by-side single and double box-drawing
	// frames so both line styles are visible. Inherits the tight
	// 22/180" line spacing from above so vertical strokes connect
	// between rows.
	append(bytes, "Box-drawing chars (single and double line):");
	nl();
	nl();

	// Side-by-side: single-line frame on the left, double-line on
	// the right. Each 3 cells across, 2 cells down. Both
	// rendered from raw byte sequences.
	append(bytes, "  ");
	bytes.push_back(Ts);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(TCs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(TCs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(TRs);
	append(bytes, "      ");
	bytes.push_back(TLd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(TCd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(TCd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(TRd);
	nl();

	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "      ");
	bytes.push_back(Vd);
	append(bytes, "  ");
	bytes.push_back(Vd);
	append(bytes, "  ");
	bytes.push_back(Vd);
	append(bytes, "  ");
	bytes.push_back(Vd);
	nl();

	append(bytes, "  ");
	bytes.push_back(LCs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(Xs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(Xs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(RCs);
	append(bytes, "      ");
	bytes.push_back(0xCC); // ╠
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(Xd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(Xd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(0xB9); // ╣
	nl();

	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "      ");
	bytes.push_back(Vd);
	append(bytes, "  ");
	bytes.push_back(Vd);
	append(bytes, "  ");
	bytes.push_back(Vd);
	append(bytes, "  ");
	bytes.push_back(Vd);
	nl();

	append(bytes, "  ");
	bytes.push_back(BLs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(BCs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(BCs);
	bytes.push_back(Hs);
	bytes.push_back(Hs);
	bytes.push_back(BRs);
	append(bytes, "      ");
	bytes.push_back(BLd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(BCd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(BCd);
	bytes.push_back(Hd);
	bytes.push_back(Hd);
	bytes.push_back(BRd);
	nl();

	// 2x2 tiles of each shade and the full block, so we can eyeball
	// whether adjacent block-element glyphs tile cleanly in both
	// directions. CP437 codes: 0xB0=░ light shade, 0xB1=▒ medium,
	// 0xB2=▓ dark, 0xDB=█ full block.
	append(bytes, "Block elements (2x2 tiles):");
	nl();
	nl();
	for (int row = 0; row < 2; ++row) {
		append(bytes, "  ");
		for (uint8_t code : {0xB0, 0xB1, 0xB2, 0xDB}) {
			bytes.push_back(code);
			bytes.push_back(code);
			append(bytes, "  ");
		}
		nl();
	}

	// Render at 2x the default test DPI so the small box-drawing and
	// shade glyphs are easier to inspect visually in the golden PNG.
	// Page height bumped to 7.5 inches to fit the chart + box-drawing
	// frames + 2x2 block tiles at default 1/6" line spacing.
	const auto rendered = render_with_page(bytes, TestDpi * 2, TestPageWidthIn, 7.5);
	expect_matches_reference(rendered, "Text_Cp437CharacterChart");
}

// ---------------------------------------------------------------------------
// Text overprint via double-strike. Demonstrates that the text rendering path
// DOES accumulate intensity — printing the same text twice gives visibly
// darker output.
//
// Uses a derived metric (count of dark pixels) rather than reference
// PNG comparison, because the difference is robust but the exact
// pixel values depend heavily on Freetype anti-aliasing quirks.
TEST_F(PrinterRenderTest, Text_DoubleStrikeAccumulation)
{
	// First render: plain "OOOO" (four wide glyphs to give the
	// counter enough signal).
	const std::vector<uint8_t> plain_bytes = {'O', 'O', 'O', 'O'};

	const auto plain_png  = render(plain_bytes);
	const auto plain_dark = count_dark_pixels(plain_png);

	// Second render: same text with double-strike on.
	std::vector<uint8_t> ds_bytes = {};

	ds_bytes.push_back(0x1B);
	ds_bytes.push_back('G'); // double-strike on
	ds_bytes.push_back('O');
	ds_bytes.push_back('O');
	ds_bytes.push_back('O');
	ds_bytes.push_back('O');
	ds_bytes.push_back(0x1B);
	ds_bytes.push_back('H'); // double-strike off

	// Need to reconfigure for the second render — PRINTER_Reset
	// between the two; output dir picks the next filename automatically
	// (page0002.png).
	const auto ds_png  = render(ds_bytes);
	const auto ds_dark = count_dark_pixels(ds_png);

	// Double-strike should be at least slightly darker via accumulated
	// intensity. Loose threshold to survive Freetype version variance.
	SCOPED_TRACE(format_str("plain dark px=%zu, double-strike dark px=%zu",
	                        plain_dark,
	                        ds_dark));

	EXPECT_GT(ds_dark, plain_dark);
}

// ---------------------------------------------------------------------------
// ESC/P dispatch correctness tests.
//
// Each test isolates one mismatch between the dispatcher in
// printer_dispatch.cpp and the Epson ESC/P 2 reference manual
// (escp2ref.pdf). Tests drive the printer with a short byte stream
// crafted so that the bug produces a visibly different render than
// the spec-correct behaviour, then compare against a checked-in
// golden PNG. A small 2.5x1.0 inch page keeps the goldens compact
// and easy to eyeball.
constexpr uint16_t DispatchTestDpi      = 360;
constexpr double DispatchTestPageWidth  = 2.5;
constexpr double DispatchTestPageHeight = 1.0;

// Dispatch_EscColon: spec C-89 says 'ESC : NUL n m' takes 3 parameter
// bytes. The buggy dispatch ate only the first one (via the unknown-
// command default path), so the next two bytes leaked into the text
// stream as glyphs. Stream: ESC : NUL B C A — post-fix only 'A' prints.
TEST_F(PrinterRenderTest, Dispatch_EscColon_ConsumesThreeParamBytes)
{
	const std::vector<uint8_t> bytes = {0x1B, ':', 0x00, 'B', 'C', 'A'};
	const auto rendered              = render_with_page(bytes,
                                               DispatchTestDpi,
                                               DispatchTestPageWidth,
                                               DispatchTestPageHeight);
	expect_matches_reference(rendered,
	                         "Dispatch_EscColon_ConsumesThreeParamBytes");
}

// Dispatch_EscCaret: spec C-191 says 'ESC ^ m nL nH d1...d_{2k}' is
// the 9-pin 60/120-dpi graphics command, with k = nL + nH*256 columns
// and 2 data bytes per column. The buggy dispatch treated ESC ^ as a
// zero-param no-op named "EnablePrintingOfAllCharacterCodesOnNextChar"
// (a different, non-existent command), leaving every byte after `^`
// to fall through as text.
TEST_F(PrinterRenderTest, Dispatch_EscCaret_ConsumesHeaderAndData)
{
	// ESC ^ m=0x21 nL=0x04 nH=0x00 → 4 columns × 2 bytes = 8 data bytes.
	// Pre-fix every byte after '^' prints as text; post-fix the header
	// + 8 data bytes are consumed and only 'A' prints.
	const std::vector<uint8_t> bytes = {
	        0x1B, '^', 0x21, 0x04, 0x00, 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'A'};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered, "Dispatch_EscCaret_ConsumesHeaderAndData");
}

// Dispatch_EscF: spec C-47 says 'ESC f m n' takes 2 parameter bytes
// (m selects horizontal/vertical mode, n is the count). The buggy
// dispatch only collected one param byte, leaving every byte from
// 'n' onwards to leak into the text stream.
TEST_F(PrinterRenderTest, Dispatch_EscF_ConsumesTwoParamBytes)
{
	// Pre-fix only 'm'=0x21 is consumed; 'X' (the spec's `n`) leaks
	// as text and prints alongside 'A'. Post-fix both header bytes
	// are eaten and only 'A' prints.
	const std::vector<uint8_t> bytes = {0x1B, 'f', 0x21, 'X', 'A'};
	const auto rendered              = render_with_page(bytes,
                                               DispatchTestDpi,
                                               DispatchTestPageWidth,
                                               DispatchTestPageHeight);
	expect_matches_reference(rendered, "Dispatch_EscF_ConsumesTwoParamBytes");
}

// Dispatch_EscBackslash: spec C-33 says
//   horizontal position = ((nH × 256) + nL) × (defined unit) + (current
//   position)
// — multiplication by the unit (inches/unit), same shape as ESC $.
// The buggy dispatch divided by defined_unit. Pre-fix the relative
// move blows the cursor way past the right margin, the 'X' clips off
// the page and only the 'M' anchor remains.
TEST_F(PrinterRenderTest, Dispatch_EscBackslash_MultipliesByDefinedUnit)
{
	const std::vector<uint8_t> bytes = {
	        // 'M' anchor at the left margin so the page isn't blank
	        // even when ESC \ misbehaves and clips 'X' off the page.
	        'M',
	        // ESC ( U 1 0 10  → defined_unit = 10/3600 = 1/360 inch/unit.
	        0x1B,
	        0x28,
	        'U',
	        0x01,
	        0x00,
	        0x0A,
	        // ESC $ 180 — absolute move to left_margin + 180 × 1/360
	        //           = 0.25 + 0.5 = 0.75 inch
	        0x1B,
	        '$',
	        0xB4,
	        0x00,
	        // ESC \ 360 — relative move by 360 × 1/360 = +1.0 inch,
	        //             leaving cur_x at 1.75 inch
	        0x1B,
	        '\\',
	        0x68,
	        0x01,
	        'X',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered,
	                         "Dispatch_EscBackslash_MultipliesByDefinedUnit");
}

// Dispatch_EscDollar: spec C-31 says
//   horizontal position = ((nH × 256) + nL) × (defined unit) + (left margin)
// — multiplication by the unit (inches/unit). The buggy dispatch
// divided by defined_unit, which inverts the units when ESC ( U has
// set a custom unit. We set defined_unit to 1/360 via ESC ( U, then
// ESC $ 540 to move the cursor to 1.5 inches past the left margin,
// then print 'X'. With the bug the position calculation explodes,
// the move is ignored, and 'X' lands at the left margin instead.
TEST_F(PrinterRenderTest, Dispatch_EscDollar_MultipliesByDefinedUnit)
{
	const std::vector<uint8_t> bytes = {
	        // ESC ( U 1 0 10  → defined_unit = 10/3600 = 1/360 inch/unit.
	        0x1B,
	        0x28,
	        'U',
	        0x01,
	        0x00,
	        0x0A,
	        // ESC $ 540 (0x021C) — move to left_margin + 540 × 1/360
	        //                   = 0.25 + 1.5 = 1.75 inch
	        0x1B,
	        '$',
	        0x1C,
	        0x02,
	        'X',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered,
	                         "Dispatch_EscDollar_MultipliesByDefinedUnit");
}

// Dispatch_EscQ: spec C-21 says
//   right margin = n × (current pitch)
// — no off-by-one. The buggy dispatch used (n - 1) / cpi, lopping
// off the last column. Stream: 'M' anchor, ESC Q 8 (right margin at
// 0.7" pre-fix vs 0.8" post-fix), ESC $ 30 to target 0.75 inch
// (between the two margins). Pre-fix the move is past the bad
// right margin and gets ignored, so 'X' overprints 'M'; post-fix
// 'X' lands at 0.75".
TEST_F(PrinterRenderTest, Dispatch_EscQ_RightMarginAtColumnN)
{
	const std::vector<uint8_t> bytes = {
	        'M',
	        0x1B,
	        'Q',
	        0x08, // right margin at column 8 (cpi=10)
	        0x1B,
	        '$',
	        0x1E,
	        0x00, // ESC $ 30 → newX = 0.25 + 30/60 = 0.75
	        'X',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered, "Dispatch_EscQ_RightMarginAtColumnN");
}

// Dispatch_EscJ_ReversePaperFeed_TopMarginClamp: spec C-213 reverses
// the vertical position by n/216 inch. The buggy code clamped against
// left_margin (a horizontal value) instead of top_margin. After
// ESC N (which resets top_margin to 0), a reverse feed that lands
// above the default left-margin value but still inside the page is
// clamped to 0.25 (pre-fix) instead of taking effect (post-fix).
TEST_F(PrinterRenderTest, Dispatch_EscJ_ReverseClampsAgainstTopMargin)
{
	const std::vector<uint8_t> bytes = {
	        // ESC N 3 — top_margin = 0, bottom_margin = 3 × 1/6 = 0.5"
	        0x1B,
	        'N',
	        0x03,
	        // ESC j 22 — reverse 22/216 ≈ 0.102". cur_y goes from
	        //            0.25 to 0.148 (above default left_margin=0.25
	        //            but well above top_margin=0).
	        0x1B,
	        'j',
	        0x16,
	        'M',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered,
	                         "Dispatch_EscJ_ReverseClampsAgainstTopMargin");
}

// Dispatch_EscX_DoesNotTouchCondensed: spec C-93 says ESC x selects
// LQ or draft only. The buggy code also wrote style.condensed
// (clearing it on LQ, setting it on draft). Stream: SI to enable
// condensed, then ESC x 1 (LQ). Pre-fix the LQ branch clears
// condensed and the trailing Ms render at normal width; post-fix
// condensed survives and all five Ms stay narrow.
TEST_F(PrinterRenderTest, Dispatch_EscX_DoesNotTouchCondensed)
{
	const std::vector<uint8_t> bytes = {
	        0x0F, // SI — turn condensed on
	        'M',
	        'M',
	        'M',
	        'M',
	        'M', // five condensed Ms
	        0x1B,
	        'x',
	        0x01, // ESC x 1 — LQ
	        'M',
	        'M',
	        'M',
	        'M',
	        'M', // five more Ms
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered, "Dispatch_EscX_DoesNotTouchCondensed");
}

// Dispatch_EscR_PaletteMatchesSpec: spec C-193 maps ESC r n →
//   0=Black, 1=Magenta, 2=Cyan, 3=Violet, 4=Yellow, 5=Red, 6=Green.
// FillPalette in printer.cpp takes CMY-dye-coverage maxima (not RGB
// light), so the existing init already produces this mapping — but
// the init is non-obvious. This test renders one ESC * bit-image
// swatch per ESC r value and pins the result against a golden so
// any future palette regression jumps out.
TEST_F(PrinterRenderTest, Dispatch_EscR_PaletteMatchesSpec)
{
	std::vector<uint8_t> bytes;
	constexpr uint8_t SwatchCols = 10; // 10 cols @ 60 dpi = 0.167"
	constexpr uint8_t StartX60th = 6;  // 6/60" = 0.1" from left margin
	constexpr uint8_t Step60th   = 20; // 0.333" between swatch starts

	// One swatch per spec colour ID 1..6. Density 0 = 60 dpi, 1 byte
	// per column, full-black byte pattern → a solid 0.167"-wide bar
	// in the selected colour. Each swatch sits in its own
	// non-overlapping x band so colours don't OR-mix.
	for (uint8_t color_id = 1; color_id <= 6; ++color_id) {
		// ESC r <color_id>
		bytes.insert(bytes.end(), {0x1B, 'r', color_id});
		const auto x_60th = static_cast<uint8_t>(
		        StartX60th + (color_id - 1) * Step60th);
		bytes.insert(bytes.end(), {0x1B, '$', x_60th, 0x00});
		// ESC * density=0, nL=SwatchCols, nH=0
		bytes.insert(bytes.end(), {0x1B, '*', 0x00, SwatchCols, 0x00});
		bytes.insert(bytes.end(), SwatchCols, uint8_t{0xFF});
	}
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered, "Dispatch_EscR_PaletteMatchesSpec");
}

// Dispatch_EscC_LinesCancelsTopMargin: spec C-13 says
//   "Setting the page length cancels the top and bottom margin settings."
// The buggy ESC C lines variant reset only bottom_margin, leaving
// top_margin behind. We set top_margin = 0.5" via ESC ( c, anchor
// 'A' at that top, send ESC C 6 which should cancel both margins,
// then ESC ( V 72 (= 0.2 inch from top_margin). Pre-fix B lands at
// 0.7"; post-fix B lands at 0.2".
TEST_F(PrinterRenderTest, Dispatch_EscC_LinesCancelsTopMargin)
{
	const std::vector<uint8_t> bytes = {
	        // ESC ( U 1 0 10 → defined_unit = 1/360 inch/unit
	        0x1B,
	        0x28,
	        'U',
	        0x01,
	        0x00,
	        0x0A,
	        // ESC ( c 4 0 180 0 540 0 — top=0.5", bot=1.5"
	        0x1B,
	        0x28,
	        'c',
	        0x04,
	        0x00,
	        0xB4,
	        0x00,
	        0x1C,
	        0x02,
	        'A',
	        // ESC C 6 — page_length = 6 × 1/6 = 1.0" lines;
	        //           per spec both margins are cancelled.
	        0x1B,
	        'C',
	        0x06,
	        0x0D, // CR — cur_x back to left
	        // ESC ( V 2 0 72 0 — move to top_margin + 72/360 = +0.2"
	        0x1B,
	        0x28,
	        'V',
	        0x02,
	        0x00,
	        0x48,
	        0x00,
	        'B',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       2.0);
	expect_matches_reference(rendered, "Dispatch_EscC_LinesCancelsTopMargin");
}

// Dispatch_Backspace_IncludesIntercharacterSpace: spec C-48 says BS
// moves left by "one character in the current character pitch plus
// any additional intercharacter space". The buggy code subtracted
// only 1/cpi. Stream: ESC SP 30 (intercharacter = 30/180 ≈ 0.167"),
// then 'A B' (with the spacing applied), BS, 'X'. Post-fix BS undoes
// the full advance and 'X' overprints 'B'; pre-fix 'X' lands in
// the gap between A and B.
TEST_F(PrinterRenderTest, Dispatch_Backspace_IncludesIntercharacterSpace)
{
	const std::vector<uint8_t> bytes = {
	        0x1B,
	        ' ',
	        0x1E, // ESC SP 30
	        'A',
	        'B',
	        0x08, // BS
	        'X',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered,
	                         "Dispatch_Backspace_IncludesIntercharacterSpace");
}

// Dispatch_VT_PicksNextTab: spec C-45 says VT moves to the *next*
// vertical tab below the current print position. The buggy loop ran
// through every tab without breaking, ending at the *last* match.
// Stream: ESC B sets three vertical tabs at lines 2, 3, 4
// (= 0.333", 0.5", 0.667"). VT from cur_y=0.25 should land at the
// first tab (0.333"); pre-fix it lands at 0.667" instead.
TEST_F(PrinterRenderTest, Dispatch_VT_PicksNextTab)
{
	const std::vector<uint8_t> bytes = {
	        0x1B,
	        'B',
	        0x02,
	        0x03,
	        0x04,
	        0x00, // ESC B 2 3 4 NUL
	        0x0B, // VT
	        'M',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered, "Dispatch_VT_PicksNextTab");
}

// Dispatch_CAN_MovesToLeftMargin: spec C-200 says CAN clears the
// current line and moves the print position to the left margin. The
// dispatcher had it as a no-op. We can't unprint already-rendered
// pixels in this streaming model, but we can honour the
// "move-to-left-margin" half of the contract. Stream: "ABC" then
// CAN then "X". Pre-fix X lands after C; post-fix X overprints A.
TEST_F(PrinterRenderTest, Dispatch_CAN_MovesToLeftMargin)
{
	const std::vector<uint8_t> bytes = {
	        'A',
	        'B',
	        'C',
	        0x18, // CAN
	        'X',
	};
	const auto rendered = render_with_page(bytes,
	                                       DispatchTestDpi,
	                                       DispatchTestPageWidth,
	                                       DispatchTestPageHeight);
	expect_matches_reference(rendered, "Dispatch_CAN_MovesToLeftMargin");
}

} // namespace
