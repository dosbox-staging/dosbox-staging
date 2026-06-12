// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Printer render integration tests.
//
// Drive the Printer class directly with a known ESC/P input byte
// sequence, render to PNG, compare against a checked-in reference
// image in tests/printer_render_data/expected/.
//
// To regenerate references after intentional behaviour changes:
//   1. Delete the reference files in tests/printer_render_data/expected/.
//   2. Run the tests once — they'll fail but write the new outputs to
//      the same directory (the failure happens because the reference
//      file is missing; the test still writes its output).
//   3. Manually inspect each generated PNG.
//   4. Commit.

#include "hardware/printer/printer.h"

#if C_PRINTER

#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <png.h>

#include "hardware/printer/postscript_passthrough.h"
#include "hardware/printer/printer_if.h"
#include "misc/std_filesystem.h"
#include "misc/support.h"
#include "utils/string_utils.h"

#include "dosbox_test_fixture.h"

namespace {

// Test pages: 180 dpi, 5"x4" = 900x720 pixels. Big enough to fit
// the multi-font style grids and a 16x16 ASCII chart.
constexpr uint16_t TestDpi          = 180;
constexpr double TestPageWidthIn    = 5.0;
constexpr double TestPageHeightIn   = 4.0;

class PrinterRenderTest : public DOSBoxTestFixture {
protected:
	std_fs::path output_dir;

	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();

		// Per-test output dir under the project's tests area.
		// (testing::TempDir() varies by gtest version; this is simpler.)
		const auto* test_info =
		        ::testing::UnitTest::GetInstance()->current_test_info();
		output_dir = std_fs::temp_directory_path() /
		             ("dosbox_printer_test_" +
		              std::string(test_info->name()));
		std::error_code ec;
		std_fs::remove_all(output_dir, ec);
		std_fs::create_directories(output_dir, ec);

		PRINTER_Configure(PrinterModelKind::EpsonDotMatrix,
		                  TestDpi,
		                  TestPageWidthIn,
		                  TestPageHeightIn,
		                  output_dir.string(),
		                  0);
	}

	void TearDown() override
	{
		PRINTER_Reset();
		DOSBoxTestFixture::TearDown();
	}

	// Drive a printer with the given bytes, flush, and return the path
	// to the PNG it produced. Clears output_dir first so the file is
	// always page1.png.
	std_fs::path render(const std::vector<uint8_t>& bytes)
	{
		std::error_code ec;
		std_fs::remove_all(output_dir, ec);
		std_fs::create_directories(output_dir, ec);

		VirtualPrinter::Printer printer(TestDpi,
		                                TestPageWidthIn,
		                                TestPageHeightIn);
		for (const auto b : bytes) {
			printer.PrintChar(b);
		}
		printer.FormFeed();

		return output_dir / "page1.png";
	}
};

// Decode an indexed-palette PNG into a flat RGB buffer.
// Returns false on failure or on dimension mismatch with the
// other image's expected dimensions.
struct DecodedImage {
	int width  = 0;
	int height = 0;
	std::vector<uint8_t> rgb;
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

	const auto w = png_get_image_width(png_ptr, info_ptr);
	const auto h = png_get_image_height(png_ptr, info_ptr);
	png_set_palette_to_rgb(png_ptr);
	png_set_expand(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	const auto channels = png_get_channels(png_ptr, info_ptr);
	if (channels < 3) {
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		fclose(fp);
		return false;
	}

	const auto row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	std::vector<uint8_t> raw(row_bytes * h);
	std::vector<png_bytep> rows(h);
	for (size_t i = 0; i < h; ++i) {
		rows[i] = raw.data() + i * row_bytes;
	}
	png_read_image(png_ptr, rows.data());
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	fclose(fp);

	// Convert to flat RGB (drop alpha if present).
	out.width  = static_cast<int>(w);
	out.height = static_cast<int>(h);
	out.rgb.resize(static_cast<size_t>(w) * h * 3);
	for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
		out.rgb[i * 3 + 0] = raw[i * channels + 0];
		out.rgb[i * 3 + 1] = raw[i * channels + 1];
		out.rgb[i * 3 + 2] = raw[i * channels + 2];
	}
	return true;
}

// Compare two PNGs. Returns the fraction of pixels that differ
// (0.0 = identical, 1.0 = every pixel differs). Returns 1.0 (worst)
// on any decode failure or dimension mismatch.
float compare_pngs(const std_fs::path& a, const std_fs::path& b)
{
	DecodedImage ia, ib;
	if (!decode_png(a, ia) || !decode_png(b, ib)) {
		return 1.0f;
	}
	if (ia.width != ib.width || ia.height != ib.height) {
		return 1.0f;
	}
	size_t diff_pixels = 0;
	const auto pixel_count = static_cast<size_t>(ia.width) * ia.height;
	for (size_t i = 0; i < pixel_count; ++i) {
		if (ia.rgb[i * 3 + 0] != ib.rgb[i * 3 + 0] ||
		    ia.rgb[i * 3 + 1] != ib.rgb[i * 3 + 1] ||
		    ia.rgb[i * 3 + 2] != ib.rgb[i * 3 + 2]) {
			++diff_pixels;
		}
	}
	return static_cast<float>(diff_pixels) / pixel_count;
}

// Count "darkish" pixels (any channel below 128). Used to assert that
// e.g. double-strike text is meaningfully darker than single-strike.
size_t count_dark_pixels(const std_fs::path& path)
{
	DecodedImage img;
	if (!decode_png(path, img)) {
		return 0;
	}
	size_t dark = 0;
	const auto pixel_count = static_cast<size_t>(img.width) * img.height;
	for (size_t i = 0; i < pixel_count; ++i) {
		const auto r = img.rgb[i * 3 + 0];
		const auto g = img.rgb[i * 3 + 1];
		const auto b = img.rgb[i * 3 + 2];
		if (r < 128 || g < 128 || b < 128) {
			++dark;
		}
	}
	return dark;
}

// Resolve a path to a reference PNG in tests/printer_render_data/expected/.
// Tests run from the project root (gtest_discover_tests sets the
// WORKING_DIRECTORY).
std_fs::path reference_path(const std::string& name)
{
	return std_fs::path{"tests/printer_render_data/expected"} /
	       (name + ".png");
}

// Compare rendered output against the reference. If the reference is
// missing, copy the rendered output to the expected location AND fail
// the test (so the user sees the failure and can commit the new
// reference deliberately).
void expect_matches_reference(const std_fs::path& rendered,
                              const std::string& test_name,
                              const float max_diff_fraction = 0.005f)
{
	const auto ref = reference_path(test_name);
	std::error_code ec;
	if (!std_fs::exists(ref, ec)) {
		std_fs::create_directories(ref.parent_path(), ec);
		std_fs::copy_file(rendered, ref,
		                  std_fs::copy_options::overwrite_existing, ec);
		SCOPED_TRACE(format_str(
		        "reference PNG missing — wrote new candidate to %s",
		        ref.string().c_str()));
		FAIL();
		return;
	}
	const auto diff = compare_pngs(rendered, ref);
	SCOPED_TRACE(format_str("test=%s diff=%.4f%% tolerance=%.4f%%",
	                        test_name.c_str(),
	                        diff * 100.0f,
	                        max_diff_fraction * 100.0f));
	EXPECT_LE(diff, max_diff_fraction);
}

// Helper: append a byte-literal sequence to `bytes`.
void append(std::vector<uint8_t>& bytes,
            std::initializer_list<uint8_t> seq)
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
	std::vector<uint8_t> bytes(
	        (std::istreambuf_iterator<char>(in)),
	        std::istreambuf_iterator<char>());
	return bytes;
}

// ---------------------------------------------------------------------------
// Test 1: a black-and-white picture made by replaying a pre-encoded
// ESC/P bit-image stream. Source: the Gridmonger project logo plus
// its warrior-emblem artwork, dithered to 1-bit and packed into
// ESC * mode 39 calls offline. See
// tests/printer_render_data/tools/png_to_escp.py for the converter.
//
// Pure pixel output, no font involvement — exact-pixel comparison.
TEST_F(PrinterRenderTest, Graphics_GridmongerLogo)
{
	const auto bytes = load_fixture(
	        "tests/printer_render_data/inputs/gridmonger_picture.bin");
	ASSERT_FALSE(bytes.empty());

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Graphics_GridmongerLogo", 0.0f);
}

// ---------------------------------------------------------------------------
// Test 2: the full 7-colour CMY/RGB palette plus the overprint
// "OR rule" demonstration.
//
// The printer uses a subtractive CMY (cyan / magenta / yellow)
// ribbon-overprint colour model where overprinted colours are
// OR'd together by sub-palette ID:
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
//   1. A row of seven solid colour swatches showing every
//      reachable sub-palette colour individually (ESC r 1..7).
//   2. Below that, three overlapping bars in C / M / Y that
//      produce the same colour set in their overlap regions —
//      proving the OR rule live (the colours in the overlaps
//      should match the swatches above them).
TEST_F(PrinterRenderTest, Graphics_CmyPalette)
{
	std::vector<uint8_t> bytes;
	constexpr uint16_t SwatchCols      = 60;  // ~ 0.33"
	constexpr uint16_t SwatchGap       = 12;  // ~ 0.07"
	constexpr uint16_t SwatchPitchCols = SwatchCols + SwatchGap;

	const uint8_t colour_order[] = {2, 3, 1, 5, 4, 6, 7};
	constexpr uint8_t SwatchStartX60th = 12; // 0.2"

	// Helper: draw one swatch with a given byte pattern repeating
	// across SwatchCols columns of a 24-pin band. `pattern` is the
	// single byte tiled vertically and horizontally.
	auto draw_swatch_row = [&](uint8_t colour_id,
	                           uint8_t x_60th,
	                           uint8_t pattern) {
		std::vector<uint8_t> col(3 * SwatchCols, pattern);
		append(bytes, {0x1B, 'r', colour_id});
		append(bytes, {0x1B, '$', x_60th, 0});
		append(bytes, {0x1B, '*', 39,
		               static_cast<uint8_t>(SwatchCols & 0xFF),
		               static_cast<uint8_t>(SwatchCols >> 8)});
		bytes.insert(bytes.end(), col.begin(), col.end());
	};

	// Bit-image graphics is binary — every set bit lights a pixel
	// at the sub-palette's max intensity. To show different shades
	// we use spatial dithering: a sparser bit pattern leaves more
	// white showing through and looks lighter at viewing distance.
	// This is what real DOS printers did for "grey" output.
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
		append(bytes, {0x1B, '*', 39,
		               static_cast<uint8_t>(BarCols & 0xFF),
		               static_cast<uint8_t>(BarCols >> 8)});
		bytes.insert(bytes.end(), bar_col.begin(), bar_col.end());
	};
	draw_bar(2, 30); // cyan
	draw_bar(1, 54); // magenta
	draw_bar(4, 78); // yellow

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Graphics_CmyPalette", 0.0f);
}

// Helper: emit one "<typeface name>: <style description> <SAMPLE>"
// line with the style applied to the SAMPLE text. The label uses
// the previously-set typeface (so the label takes on the typeface
// itself, making the typeface differences obvious).
void emit_style_row(std::vector<uint8_t>& bytes,
                    std::string_view label,
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

// Common body for the proportional and fixed-pitch style grids.
// Switches typeface and runs through nine style variations. The
// caller sets the proportional flag before calling.
void emit_style_grid(std::vector<uint8_t>& bytes)
{
	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	for (const auto& [typeface_id, typeface_name] :
	     std::initializer_list<std::pair<uint8_t, std::string_view>>{
	             {0, "Roman"},
	             {1, "Sans Serif"},
	             {2, "Courier"}}) {
		// Select typeface for both label and sample.
		append(bytes, {0x1B, 'k', typeface_id});

		// 11-char wide labels (the longest is "Dbl-strike:").
		emit_style_row(bytes, "Plain:     ",
		               [](auto&) {},
		               [](auto&) {});
		emit_style_row(bytes, "Bold:      ",
		               [](auto& b) { append(b, {0x1B, 'E'}); },
		               [](auto& b) { append(b, {0x1B, 'F'}); });
		emit_style_row(bytes, "Italic:    ",
		               [](auto& b) { append(b, {0x1B, '4'}); },
		               [](auto& b) { append(b, {0x1B, '5'}); });
		emit_style_row(bytes, "Bold+It:   ",
		               [](auto& b) { append(b, {0x1B, 'E', 0x1B, '4'}); },
		               [](auto& b) { append(b, {0x1B, '5', 0x1B, 'F'}); });
		emit_style_row(bytes, "Underline: ",
		               [](auto& b) { append(b, {0x1B, '-', 1}); },
		               [](auto& b) { append(b, {0x1B, '-', 0}); });
		emit_style_row(bytes, "Dbl-strike:",
		               [](auto& b) { append(b, {0x1B, 'G'}); },
		               [](auto& b) { append(b, {0x1B, 'H'}); });
		nl();
		(void)typeface_name; // unused — label takes typeface naturally
	}
}

// ---------------------------------------------------------------------------
// Test 3a: the style permutation grid in **proportional** mode.
// Three Liberation typefaces × six style variations: plain, bold,
// italic, bold+italic, underline, double-strike. Each row prints the
// sample text in the current typeface so the typeface itself is
// visible in the label. ESC p 1 (proportional) means each glyph
// gets its natural width — Serif and Sans Serif look like real
// proportional type; Courier is monospace by design.
TEST_F(PrinterRenderTest, Text_StylesProportional)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1B, 'p', 1});           // ESC p 1 — proportional ON
	append(bytes, {0x1B, 'P'});              // ESC P  — 10 cpi base
	emit_style_grid(bytes);

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_StylesProportional", 0.015f);
}

// ---------------------------------------------------------------------------
// Test 3b: the same style permutation grid in **fixed-pitch** mode.
// ESC p 0 means each glyph fills a 1/cpi-inch cell regardless of
// its natural width. With Phase 3-era glyph centring, narrow
// proportional glyphs sit in the middle of their cells rather than
// being left-aligned with a gap on the right.
TEST_F(PrinterRenderTest, Text_StylesFixedPitch)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1B, 'p', 0});           // ESC p 0 — proportional OFF
	append(bytes, {0x1B, 'P'});              // ESC P  — 10 cpi
	emit_style_grid(bytes);

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_StylesFixedPitch", 0.015f);
}

// ---------------------------------------------------------------------------
// Test 3c: pitch and size variations.
// Demonstrates the printer's CPI switches (10 / 12 / 15) and the
// double-width / double-height attributes. Single typeface (Roman)
// to keep the focus on the size/pitch dimensions. Fixed-pitch mode
// so each CPI value clearly shows.
TEST_F(PrinterRenderTest, Text_PitchAndSize)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1B, 'p', 0});           // proportional off

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	// CPI variations across the three typefaces.
	const uint8_t typefaces[] = {0, 1, 2};
	const char* tf_names[]    = {"Roman ", "Sans  ", "Courier"};
	for (size_t i = 0; i < std::size(typefaces); ++i) {
		append(bytes, {0x1B, 'k', typefaces[i]});
		append(bytes, {0x1B, 'P'});              // 10 cpi
		append(bytes, tf_names[i]);
		append(bytes, " 10cpi The quick brown fox"); nl();
		append(bytes, {0x1B, 'M'});              // 12 cpi
		append(bytes, tf_names[i]);
		append(bytes, " 12cpi The quick brown fox"); nl();
		append(bytes, {0x1B, 'g'});              // 15 cpi
		append(bytes, tf_names[i]);
		append(bytes, " 15cpi The quick brown fox"); nl();
	}
	nl();

	// Double-width / double-height variants in each typeface.
	append(bytes, {0x1B, 'P'});                    // back to 10 cpi
	for (size_t i = 0; i < std::size(typefaces); ++i) {
		append(bytes, {0x1B, 'k', typefaces[i]});
		append(bytes, {0x1B, 'W', 1});             // double width on
		append(bytes, tf_names[i]);
		append(bytes, " Dbl-width");
		append(bytes, {0x1B, 'W', 0});             // off
		nl();
	}
	nl();
	// Bump line spacing so double-height rows don't overlap.
	append(bytes, {0x1B, '3', 60});                // 60/180" line feed
	for (size_t i = 0; i < std::size(typefaces); ++i) {
		append(bytes, {0x1B, 'k', typefaces[i]});
		append(bytes, {0x1B, 'w', 1});             // double height on
		append(bytes, tf_names[i]);
		append(bytes, " Dbl-height");
		append(bytes, {0x1B, 'w', 0});             // off
		nl();
	}
	append(bytes, {0x1B, '2'});                    // restore default 1/6"

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_PitchAndSize", 0.015f);
}

// ---------------------------------------------------------------------------
// Test 3d: line/score variants via ESC ( -.
// The command format is ESC ( - 3 0 1 d1 d2, where d1 selects the
// location (1=underline, 2=strikethrough, 3=overscore) and d2 the
// type (1=single continuous, 2=double continuous, 5=single broken,
// 6=double broken). Shows the full grid.
TEST_F(PrinterRenderTest, Text_LineScores)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1B, 'p', 1});  // proportional on
	append(bytes, {0x1B, 'k', 0});  // Roman

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
		nl(); nl();

		append(bytes, "Strike    ");
		set_score(2, type_codes[t]);
		append(bytes, types[t]);
		clear_score();
		nl(); nl();

		append(bytes, "Overscore ");
		set_score(3, type_codes[t]);
		append(bytes, types[t]);
		clear_score();
		nl(); nl();
	}

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_LineScores", 0.015f);
}

// ---------------------------------------------------------------------------
// Test 3e: scalable fonts via ESC X (multipoint). Selects each of
// 8, 10.5, 16, 24-point sizes and prints a sample at each. ESC X
// format: ESC X m nL nH, where m is the pitch (0 or 1 = keep
// current) and nL+nH*256 is the point-size in half-points
// (16-point = 32 half-points, 24-point = 48).
TEST_F(PrinterRenderTest, Text_MultipointFonts)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1B, 'p', 1});  // proportional on
	append(bytes, {0x1B, 'k', 0});  // Roman

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	auto multipoint = [&](uint16_t halfpoints) {
		append(bytes, {0x1B, 'X', 0,
		               static_cast<uint8_t>(halfpoints & 0xFF),
		               static_cast<uint8_t>(halfpoints >> 8)});
	};

	multipoint(16); // 8 point
	append(bytes, "8pt    The quick brown fox"); nl();
	multipoint(21); // 10.5 point
	append(bytes, "10.5pt The quick brown fox"); nl();
	multipoint(32); // 16 point
	append(bytes, "16pt   Quick fox"); nl();
	multipoint(48); // 24 point
	append(bytes, "24pt   Quick"); nl();

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_MultipointFonts", 0.02f);
}

// ---------------------------------------------------------------------------
// Test 3f: international character sets via ESC R. Selects various
// national variants and prints the high-bit ASCII region (chars
// 0x23 onwards) — each national variant remaps a handful of
// punctuation slots to local-language characters (£ for UK, ¤ for
// France, ñ for Spain, etc.).
TEST_F(PrinterRenderTest, Text_InternationalCharsets)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1B, 'p', 0});  // fixed pitch so columns line up
	append(bytes, {0x1B, 'k', 2});  // Courier
	append(bytes, {0x1B, 'M'});     // 12 cpi

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	// Top section: ESC R national variants show how the
	// 12 low-ASCII "punctuation slots" remap per country.
	append(bytes, "ESC R national variants (low-ASCII slots):"); nl(); nl();
	const std::string sample = " # $ @ [ \\ ] ^ ` { | } ~";
	const struct {
		uint8_t code;
		std::string_view name;
	} variants[] = {
	        {0,  "USA       "},
	        {1,  "France    "},
	        {2,  "Germany   "},
	        {3,  "UK        "},
	        {5,  "Sweden    "},
	        {6,  "Italy     "},
	        {7,  "Spain     "},
	};
	for (const auto& v : variants) {
		append(bytes, {0x1B, 'R', v.code});
		append(bytes, v.name);
		append(bytes, sample);
		nl();
	}
	append(bytes, {0x1B, 'R', 0});   // back to USA
	nl();

	// Bottom section: the CP437 high-ASCII accented characters
	// (bytes 0x80-0xAF). These come from the active character
	// table (default CP437) and don't change with ESC R.
	append(bytes, "CP437 high-ASCII accented characters:"); nl(); nl();
	for (uint8_t row = 0x80; row < 0xB0; row += 0x10) {
		bytes.push_back(static_cast<uint8_t>(
		        (row >> 4) < 10 ? '0' + (row >> 4)
		                         : 'A' + (row >> 4) - 10));
		append(bytes, "x: ");
		for (uint8_t col = 0; col < 0x10; ++col) {
			bytes.push_back(static_cast<uint8_t>(row + col));
			bytes.push_back(' ');
		}
		nl();
	}

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_InternationalCharsets", 0.015f);
}

// ---------------------------------------------------------------------------
// Test 4: superscript / subscript. Exercises the ESC S param-index
// fix from Phase 3a.
TEST_F(PrinterRenderTest, Text_SuperscriptSubscript)
{
	std::vector<uint8_t> bytes;
	// "H2O" with the 2 as subscript: H ESC S 0 2 ESC T O
	bytes.push_back('H');
	bytes.insert(bytes.end(), {0x1B, 'S', 0});   // ESC S 0 = subscript
	bytes.push_back('2');
	bytes.push_back(0x1B); bytes.push_back('T'); // ESC T = cancel
	bytes.push_back('O');

	bytes.push_back(' ');
	// "x2" with the 2 as superscript: x ESC S 1 2 ESC T
	bytes.push_back('x');
	bytes.insert(bytes.end(), {0x1B, 'S', 1});   // ESC S 1 = superscript
	bytes.push_back('2');
	bytes.push_back(0x1B); bytes.push_back('T'); // ESC T = cancel

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_SuperscriptSubscript", 0.01f);
}

// ---------------------------------------------------------------------------
// Test 3g: a CP437 character chart. 16x16 grid of all 256 byte
// values, with box-drawing borders (also from CP437). Demonstrates
// the high-bit character region — accented letters, math symbols,
// Greek letters, and the box-drawing characters themselves.
//
// Control bytes (0x00-0x1F, 0x7F) are rendered as '.' placeholders
// because we don't implement ESC ( ^ "print as character" — those
// bytes would otherwise be interpreted as commands.
TEST_F(PrinterRenderTest, Text_Cp437CharacterChart)
{
	std::vector<uint8_t> bytes;
	append(bytes, {0x1B, '@'});            // reset printer
	append(bytes, {0x1B, 'p', 0});         // fixed pitch
	append(bytes, {0x1B, 'k', 2});         // Courier (monospace)
	append(bytes, {0x1B, 'M'});            // 12 cpi (more breathing room)

	// Tighter than the default 30/180 — pulls rows close so the
	// chart reads as a block rather than a scattered grid.
	append(bytes, {0x1B, '3', 22});        // line spacing 22/180"

	auto nl = [&]() { append(bytes, {0x0D, 0x0A}); };

	// CP437 box-drawing characters.
	constexpr uint8_t Hs = 0xC4; // ─ single horizontal
	constexpr uint8_t Vs = 0xB3; // │ single vertical
	constexpr uint8_t Ts = 0xDA; // ┌
	constexpr uint8_t TRs = 0xBF; // ┐
	constexpr uint8_t BLs = 0xC0; // └
	constexpr uint8_t BRs = 0xD9; // ┘
	constexpr uint8_t Xs  = 0xC5; // ┼
	constexpr uint8_t TCs = 0xC2; // ┬
	constexpr uint8_t BCs = 0xC1; // ┴
	constexpr uint8_t LCs = 0xC3; // ├
	constexpr uint8_t RCs = 0xB4; // ┤

	constexpr uint8_t Hd = 0xCD; // ═
	constexpr uint8_t Vd = 0xBA; // ║
	constexpr uint8_t TLd = 0xC9; // ╔
	constexpr uint8_t TRd = 0xBB; // ╗
	constexpr uint8_t BLd = 0xC8; // ╚
	constexpr uint8_t BRd = 0xBC; // ╝
	constexpr uint8_t TCd = 0xCB; // ╦
	constexpr uint8_t BCd = 0xCA; // ╩
	constexpr uint8_t Xd  = 0xCE; // ╬

	auto is_printable = [](uint8_t b) {
		return b >= 0x20 && b != 0x7F;
	};

	// Title.
	append(bytes, "CP437 Character Chart");
	nl(); nl();

	// Column header row: "      0 1 2 3 ... F".
	append(bytes, "    ");
	for (int c = 0; c < 16; ++c) {
		append(bytes, " ");
		const char digit = static_cast<char>(
		        c < 10 ? '0' + c : 'A' + (c - 10));
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
		const char row_digit = static_cast<char>(
		        r < 10 ? '0' + r : 'A' + (r - 10));
		append(bytes, " ");
		bytes.push_back(static_cast<uint8_t>(row_digit));
		append(bytes, "x ");
		for (int c = 0; c < 16; ++c) {
			append(bytes, " ");
			const uint8_t b = static_cast<uint8_t>(r * 16 + c);
			bytes.push_back(is_printable(b)
			                        ? b
			                        : static_cast<uint8_t>('.'));
		}
		nl();
	}

	// Double-line horizontal rule below the data.
	append(bytes, "   ");
	for (int c = 0; c < 33; ++c) {
		bytes.push_back(Hd);
	}
	nl(); nl();

	// Demo block: side-by-side single and double box-drawing
	// frames so both line styles are visible. Inherits the tight
	// 22/180" line spacing from above so vertical strokes connect
	// between rows.
	append(bytes, "Box-drawing chars (single and double line):");
	nl(); nl();

	// Side-by-side: single-line frame on the left, double-line on
	// the right. Each 3 cells across, 2 cells down. Both
	// rendered from raw byte sequences.
	append(bytes, "  ");
	bytes.push_back(Ts);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(TCs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(TCs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(TRs);
	append(bytes, "      ");
	bytes.push_back(TLd);
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(TCd);
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(TCd);
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(TRd);
	nl();

	append(bytes, "  ");
	bytes.push_back(Vs); append(bytes, "  ");
	bytes.push_back(Vs); append(bytes, "  ");
	bytes.push_back(Vs); append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "      ");
	bytes.push_back(Vd); append(bytes, "  ");
	bytes.push_back(Vd); append(bytes, "  ");
	bytes.push_back(Vd); append(bytes, "  ");
	bytes.push_back(Vd);
	nl();

	append(bytes, "  ");
	bytes.push_back(LCs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(Xs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(Xs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(RCs);
	append(bytes, "      ");
	bytes.push_back(0xCC); // ╠
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(Xd);
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(Xd);
	bytes.push_back(Hd); bytes.push_back(Hd);
	bytes.push_back(0xB9); // ╣
	nl();

	append(bytes, "  ");
	bytes.push_back(Vs); append(bytes, "  ");
	bytes.push_back(Vs); append(bytes, "  ");
	bytes.push_back(Vs); append(bytes, "  ");
	bytes.push_back(Vs);
	append(bytes, "      ");
	bytes.push_back(Vd); append(bytes, "  ");
	bytes.push_back(Vd); append(bytes, "  ");
	bytes.push_back(Vd); append(bytes, "  ");
	bytes.push_back(Vd);
	nl();

	append(bytes, "  ");
	bytes.push_back(BLs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(BCs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(BCs);
	bytes.push_back(Hs); bytes.push_back(Hs); bytes.push_back(BRs);
	append(bytes, "      ");
	bytes.push_back(BLd);
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(BCd);
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(BCd);
	bytes.push_back(Hd); bytes.push_back(Hd); bytes.push_back(BRd);
	nl();

	const auto rendered = render(bytes);
	expect_matches_reference(rendered, "Text_Cp437CharacterChart", 0.02f);
}

// ---------------------------------------------------------------------------
// Test 5: text overprint via double-strike. Demonstrates that the
// text rendering path DOES accumulate intensity — printing the same
// text twice gives visibly darker output.
//
// Uses a derived metric (count of dark pixels) rather than reference
// PNG comparison, because the difference is robust but the exact
// pixel values depend heavily on Freetype anti-aliasing quirks.
TEST_F(PrinterRenderTest, Text_DoubleStrikeAccumulation)
{
	// First render: plain "OOOO" (four wide glyphs to give the
	// counter enough signal).
	const std::vector<uint8_t> plain_bytes = {'O', 'O', 'O', 'O'};
	const auto plain_png = render(plain_bytes);
	const auto plain_dark = count_dark_pixels(plain_png);

	// Second render: same text with double-strike on.
	std::vector<uint8_t> ds_bytes;
	ds_bytes.push_back(0x1B); ds_bytes.push_back('G'); // double-strike on
	ds_bytes.push_back('O'); ds_bytes.push_back('O');
	ds_bytes.push_back('O'); ds_bytes.push_back('O');
	ds_bytes.push_back(0x1B); ds_bytes.push_back('H'); // double-strike off
	// Need to reconfigure for the second render — PRINTER_Reset
	// between the two; output dir picks the next filename automatically
	// (page2.png).
	const auto ds_png = render(ds_bytes);
	const auto ds_dark = count_dark_pixels(ds_png);

	// Double-strike should be at least slightly darker via accumulated
	// intensity. Loose threshold to survive Freetype version variance.
	SCOPED_TRACE(format_str("plain dark px=%zu, double-strike dark px=%zu",
	                        plain_dark,
	                        ds_dark));
	EXPECT_GT(ds_dark, plain_dark);
}

// ---------------------------------------------------------------------------
// Test 6: PostScript passthrough.
//
// Our PostScript mode is a byte-for-byte passthrough — we don't
// interpret PostScript. The DOS app sends PostScript code that
// references the Adobe Base-14 fonts (Times, Helvetica, Courier
// and their bold/italic variants) which every PostScript renderer
// has built in.
//
// The test feeds a multi-font PostScript document through the
// PostScriptPassthrough class and asserts the output `.ps` file
// is byte-identical to the input (modulo the `^D` end-of-job
// marker which we strip). No rendering involved.
//
// This test uses a fresh fixture because the dot-matrix
// PrinterRenderTest's SetUp configures for EpsonDotMatrix.
class PrinterPostScriptTest : public DOSBoxTestFixture {
protected:
	std_fs::path output_dir;

	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();

		const auto* test_info =
		        ::testing::UnitTest::GetInstance()->current_test_info();
		output_dir = std_fs::temp_directory_path() /
		             ("dosbox_printer_ps_test_" +
		              std::string(test_info->name()));
		std::error_code ec;
		std_fs::remove_all(output_dir, ec);
		std_fs::create_directories(output_dir, ec);

		PRINTER_Configure(PrinterModelKind::PostScript,
		                  0, 0.0, 0.0,
		                  output_dir.string(),
		                  0);
	}

	void TearDown() override
	{
		PRINTER_Reset();
		DOSBoxTestFixture::TearDown();
	}
};

TEST_F(PrinterPostScriptTest, MultiFontDocument)
{
	// Multi-font / multi-style PostScript document referencing
	// the Adobe Base-14 fonts. Trailing 0x04 is the
	// end-of-job marker our passthrough strips before closing.
	const std::string ps_document = R"(%!PS-Adobe-3.0
%%Title: dosbox-staging printer test
%%Pages: 1
%%EndComments

%%Page: 1 1
/show-line {
  moveto show
} def

/Times-Roman findfont 18 scalefont setfont
72 720 (Times-Roman regular) show-line

/Times-Bold findfont 18 scalefont setfont
72 690 (Times-Bold) show-line

/Times-Italic findfont 18 scalefont setfont
72 660 (Times-Italic) show-line

/Times-BoldItalic findfont 18 scalefont setfont
72 630 (Times-BoldItalic) show-line

/Helvetica findfont 18 scalefont setfont
72 580 (Helvetica regular) show-line

/Helvetica-Bold findfont 18 scalefont setfont
72 550 (Helvetica-Bold) show-line

/Helvetica-Oblique findfont 18 scalefont setfont
72 520 (Helvetica-Oblique) show-line

/Helvetica-BoldOblique findfont 18 scalefont setfont
72 490 (Helvetica-BoldOblique) show-line

/Courier findfont 18 scalefont setfont
72 440 (Courier regular) show-line

/Courier-Bold findfont 18 scalefont setfont
72 410 (Courier-Bold) show-line

/Courier-Oblique findfont 18 scalefont setfont
72 380 (Courier-Oblique) show-line

/Courier-BoldOblique findfont 18 scalefont setfont
72 350 (Courier-BoldOblique) show-line

/Times-Roman findfont 10 scalefont setfont
72 300 (10pt) show-line
/Times-Roman findfont 14 scalefont setfont
72 270 (14pt) show-line
/Times-Roman findfont 24 scalefont setfont
72 230 (24pt) show-line

showpage
%%EOF
)";
	const auto eot = static_cast<char>(0x04);

	// Feed the document through the passthrough sink.
	VirtualPrinter::PostScriptPassthrough sink;
	for (const char c : ps_document) {
		sink.Write(static_cast<uint8_t>(c));
	}
	sink.Write(static_cast<uint8_t>(eot));

	// The sink closes itself on ^D; output file is doc1.ps in
	// the output_dir.
	const auto out_path = output_dir / "doc1.ps";
	ASSERT_TRUE(std_fs::exists(out_path));

	std::ifstream in(out_path, std::ios::binary);
	const std::string written(
	        (std::istreambuf_iterator<char>(in)),
	        std::istreambuf_iterator<char>());

	// Output should be the input verbatim, MINUS the ^D marker.
	EXPECT_EQ(written, ps_document);
}

} // namespace

#endif // C_PRINTER
