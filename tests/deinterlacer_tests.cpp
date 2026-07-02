// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/render/private/deinterlacer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "misc/rendered_image.h"
#include "misc/video.h"

namespace {

// Width must be a multiple of 64 (the deinterlacer processes rows in
// 64-pixel bit-buffer chunks)
constexpr int LineWidth  = 320;
constexpr int LineHeight = 400;

constexpr int DotWidth  = 320;
constexpr int DotHeight = 200;

constexpr uint32_t Black   = 0x00000000;
constexpr uint32_t Content = 0x00cc8844;

// The line-interlaced block spans these rows: even rows hold content,
// odd rows are "empty" (background-coloured)
constexpr int LineBlockFirstContentRow = 40;
constexpr int LineBlockLastContentRow  = 298;

// A solid, non-interlaced block that must be left intact
constexpr int SolidBlockFirstRow = 320;
constexpr int SolidBlockLastRow  = 379;

// The dot-interlaced block spans row pairs (even row: dot pattern, odd
// row: all black)
constexpr int DotBlockFirstRow = 10;
constexpr int DotBlockLastRow  = 99;

// A solid content block after the dot pattern that stops the dot
// deinterlacer
constexpr int DotSolidBlockFirstRow = 120;
constexpr int DotSolidBlockLastRow  = 179;

// Mirrors the deinterlacer's internal `scale_rgb()` (scales the RGB
// components by factor/256 with rounding; the X byte is dropped)
uint32_t ScaleRgb(const uint32_t color, const int factor)
{
	auto scale = [&](const uint32_t c) {
		return static_cast<uint32_t>((c * factor + 128) >> 8);
	};

	const auto r = scale((color >> 16) & 0xff) << 16;
	const auto g = scale((color >> 8) & 0xff) << 8;
	const auto b = scale(color & 0xff);

	return r | g | b;
}

// Mirrors `to_rgb_scale_factor_linear()` in deinterlacer.cpp
constexpr int LineScaleFactor(const DeinterlacingStrength strength)
{
	using enum DeinterlacingStrength;

	switch (strength) {
	case Light: return 153;
	case Medium: return 204;
	case Strong: return 230;
	case Full: return 256;
	default: return 0;
	}
}

// Mirrors `to_rgb_scale_factor_dot()` in deinterlacer.cpp
constexpr int DotScaleFactor(const DeinterlacingStrength strength)
{
	using enum DeinterlacingStrength;

	switch (strength) {
	case Light: return 193;
	case Medium: return 234;
	case Strong: return 245;
	case Full: return 256;
	default: return 0;
	}
}

VideoMode LineFmvVideoMode()
{
	VideoMode mode = {};

	mode.bios_mode_number = 0x101;
	mode.is_graphics_mode = true;
	mode.width            = LineWidth;
	mode.height           = LineHeight;
	mode.color_depth      = ColorDepth::IndexedColor256;

	return mode;
}

VideoMode DotFmvVideoMode()
{
	VideoMode mode = {};

	mode.bios_mode_number = 0x13;
	mode.is_graphics_mode = true;
	mode.width            = DotWidth;
	mode.height           = DotHeight;
	mode.color_depth      = ColorDepth::IndexedColor256;

	return mode;
}

VideoMode NonFmvVideoMode()
{
	VideoMode mode = {};

	mode.bios_mode_number = 0x0d;
	mode.is_graphics_mode = true;
	mode.width            = 320;
	mode.height           = 200;
	mode.color_depth      = ColorDepth::IndexedColor16;

	return mode;
}

// Returns a non-owning BGRX32 view over `pixels`; the vector must
// outlive the returned image and must not be reallocated afterwards.
RenderedImage MakeBgrx32ImageView(std::vector<uint32_t>& pixels, const int width,
                                  const int height, const VideoMode& video_mode)
{
	assert(pixels.size() == static_cast<size_t>(width) * height);

	RenderedImage image = {};

	image.params.width        = width;
	image.params.height       = height;
	image.params.pixel_format = PixelFormat::BGRX32_ByteArray;
	image.params.video_mode   = video_mode;

	image.pitch      = width * static_cast<int>(sizeof(uint32_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	return image;
}

// Palette index that maps to `Content` in Indexed8 test images
constexpr uint8_t ContentIndex = 1;

// Returns a non-owning Indexed8 view over `pixels`; the vector must
// outlive the returned image and must not be reallocated afterwards.
RenderedImage MakeIndexed8ImageView(std::vector<uint8_t>& pixels, const int width,
                                    const int height, const VideoMode& video_mode)
{
	assert(pixels.size() == static_cast<size_t>(width) * height);

	RenderedImage image = {};

	image.params.width        = width;
	image.params.height       = height;
	image.params.pixel_format = PixelFormat::Indexed8;
	image.params.video_mode   = video_mode;

	image.pitch      = width;
	image.image_data = pixels.data();

	image.palette[ContentIndex] = Rgb888(static_cast<uint8_t>(Content >> 16),
	                                     static_cast<uint8_t>(Content >> 8),
	                                     static_cast<uint8_t>(Content));

	return image;
}

// Interprets the pixel data of `image` as uint32_t BGRX32 pixels
std::vector<uint32_t> ToBgrx32Pixels(const RenderedImage& image)
{
	const auto* data = reinterpret_cast<const uint32_t*>(image.image_data);
	const auto num_pixels = static_cast<size_t>(image.params.width) *
	                        image.params.height;

	return {data, data + num_pixels};
}

void FillRow(std::vector<uint32_t>& pixels, const int width, const int row,
             const uint32_t color)
{
	const auto start = pixels.begin() + static_cast<ptrdiff_t>(row) * width;
	std::fill(start, start + width, color);
}

// Builds the line-interlaced test frame:
//
//   rows 0..9                    background (background colour detection)
//   rows 40..298, even rows      content (the interlaced FMV area)
//   rows 40..299, odd rows       background ("missing" lines)
//   rows 320..379                solid content (must be left intact)
//   everything else              background
//
std::vector<uint32_t> MakeLineInterlacedPixels(const uint32_t content,
                                               const uint32_t background)
{
	std::vector<uint32_t> pixels(static_cast<size_t>(LineWidth) * LineHeight,
	                             background);

	for (auto y = LineBlockFirstContentRow; y <= LineBlockLastContentRow; y += 2) {
		FillRow(pixels, LineWidth, y, content);
	}
	for (auto y = SolidBlockFirstRow; y <= SolidBlockLastRow; ++y) {
		FillRow(pixels, LineWidth, y, content);
	}
	return pixels;
}

// The expected output of line-deinterlacing the above frame. The
// detected mask covers the whole interlaced block, so within it every
// row gets the dimmed row above OR-ed into it: "missing" rows become
// `background | ScaleRgb(content)`, content rows become
// `content | ScaleRgb(background)` (a no-op for a black background).
//
std::vector<uint32_t> MakeExpectedLineDeinterlacedPixels(const uint32_t content,
                                                         const uint32_t background,
                                                         const int factor)
{
	auto pixels = MakeLineInterlacedPixels(content, background);

	for (auto y = LineBlockFirstContentRow; y <= LineBlockLastContentRow + 1;
	     ++y) {
		const auto is_content_row = (y % 2 == 0);

		const auto color = is_content_row
		                         ? (content | ScaleRgb(background, factor))
		                         : (background | ScaleRgb(content, factor));

		FillRow(pixels, LineWidth, y, color);
	}
	return pixels;
}

// Builds the dot-interlaced test frame (320x200, mode 13h):
//
//   rows 0..9                        black (skipped leading rows)
//   rows 10..98, even rows           content at even x, black at odd x
//   rows 11..99, odd rows            all black
//   rows 120..179                    solid content (stops the pattern)
//   everything else                  black
//
std::vector<uint32_t> MakeDotInterlacedPixels(const uint32_t content)
{
	std::vector<uint32_t> pixels(static_cast<size_t>(DotWidth) * DotHeight, Black);

	for (auto y = DotBlockFirstRow; y <= DotBlockLastRow; y += 2) {
		for (auto x = 0; x < DotWidth; x += 2) {
			pixels[static_cast<size_t>(y) * DotWidth + x] = content;
		}
	}
	for (auto y = DotSolidBlockFirstRow; y <= DotSolidBlockLastRow; ++y) {
		FillRow(pixels, DotWidth, y, content);
	}
	return pixels;
}

// The expected output of dot-deinterlacing the above frame: within the
// pattern block, even rows become solid content (the odd pixels are
// copied from their left neighbours) and odd rows become solid dimmed
// content.
//
std::vector<uint32_t> MakeExpectedDotDeinterlacedPixels(const uint32_t content,
                                                        const int factor)
{
	auto pixels = MakeDotInterlacedPixels(content);

	for (auto y = DotBlockFirstRow; y <= DotBlockLastRow; ++y) {
		const auto is_pattern_row = (y % 2 == 0);

		const auto color = is_pattern_row ? content
		                                  : ScaleRgb(content, factor);

		FillRow(pixels, DotWidth, y, color);
	}
	return pixels;
}

void ExpectPixelsEqual(const std::vector<uint32_t>& actual,
                       const std::vector<uint32_t>& expected, const int width)
{
	ASSERT_EQ(actual.size(), expected.size());

	for (size_t i = 0; i < actual.size(); ++i) {
		ASSERT_EQ(actual[i], expected[i])
		        << "first pixel difference at x=" << (i % width)
		        << ", y=" << (i / width) << std::hex << ", actual=0x"
		        << actual[i] << ", expected=0x" << expected[i];
	}
}

} // namespace

// ----------------------------------------------------------------------------
// Line deinterlacing (400+ line, 256+ colour modes)
// ----------------------------------------------------------------------------

TEST(DeinterlacerTest, LineDeinterlaceFillsMissingLinesAtFullStrength)
{
	auto pixels      = MakeLineInterlacedPixels(Content, Black);
	const auto image = MakeBgrx32ImageView(pixels,
	                                       LineWidth,
	                                       LineHeight,
	                                       LineFmvVideoMode());

	Deinterlacer deinterlacer = {};
	deinterlacer.DeinterlaceInPlace(image, DeinterlacingStrength::Full);

	const auto expected = MakeExpectedLineDeinterlacedPixels(
	        Content, Black, LineScaleFactor(DeinterlacingStrength::Full));

	ExpectPixelsEqual(pixels, expected, LineWidth);
}

TEST(DeinterlacerTest, LineDeinterlaceDimsFilledLinesPerStrength)
{
	using enum DeinterlacingStrength;

	for (const auto strength : {Light, Medium, Strong, Full}) {
		auto pixels      = MakeLineInterlacedPixels(Content, Black);
		const auto image = MakeBgrx32ImageView(pixels,
		                                       LineWidth,
		                                       LineHeight,
		                                       LineFmvVideoMode());

		Deinterlacer deinterlacer = {};
		deinterlacer.DeinterlaceInPlace(image, strength);

		const auto expected = MakeExpectedLineDeinterlacedPixels(
		        Content, Black, LineScaleFactor(strength));

		ExpectPixelsEqual(pixels, expected, LineWidth);
	}
}

TEST(DeinterlacerTest, LineDeinterlaceDetectsNonBlackBackground)
{
	// Crusader: No Regret style RGB(8,8,8) background
	constexpr uint32_t Background = 0x00080808;

	auto pixels      = MakeLineInterlacedPixels(Content, Background);
	const auto image = MakeBgrx32ImageView(pixels,
	                                       LineWidth,
	                                       LineHeight,
	                                       LineFmvVideoMode());

	Deinterlacer deinterlacer = {};
	deinterlacer.DeinterlaceInPlace(image, DeinterlacingStrength::Full);

	const auto expected = MakeExpectedLineDeinterlacedPixels(
	        Content, Background, LineScaleFactor(DeinterlacingStrength::Full));

	ExpectPixelsEqual(pixels, expected, LineWidth);
}

TEST(DeinterlacerTest, LineDeinterlaceIgnoresTheUnusedXByte)
{
	// On the display path, the pixels of Indexed8 modes go through the
	// palette LUT which sets the unused X byte to 255. The X byte must
	// not defeat the background colour detection, otherwise these modes
	// are silently never deinterlaced.
	constexpr uint32_t XContent    = 0xff000000 | Content;
	constexpr uint32_t XBackground = 0xff000000;

	auto pixels      = MakeLineInterlacedPixels(XContent, XBackground);
	const auto image = MakeBgrx32ImageView(pixels,
	                                       LineWidth,
	                                       LineHeight,
	                                       LineFmvVideoMode());

	Deinterlacer deinterlacer = {};
	deinterlacer.DeinterlaceInPlace(image, DeinterlacingStrength::Full);

	const auto expected = MakeExpectedLineDeinterlacedPixels(
	        XContent, XBackground, LineScaleFactor(DeinterlacingStrength::Full));

	ExpectPixelsEqual(pixels, expected, LineWidth);
}

TEST(DeinterlacerTest, LineDeinterlaceLeavesPatternlessImageUntouched)
{
	// Solid content only, no interlaced area
	std::vector<uint32_t> pixels(static_cast<size_t>(LineWidth) * LineHeight,
	                             Black);
	for (auto y = SolidBlockFirstRow; y <= SolidBlockLastRow; ++y) {
		FillRow(pixels, LineWidth, y, Content);
	}
	const auto original = pixels;

	const auto image = MakeBgrx32ImageView(pixels,
	                                       LineWidth,
	                                       LineHeight,
	                                       LineFmvVideoMode());

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.DeinterlaceInPlace(image,
                                                            DeinterlacingStrength::Full);

	EXPECT_EQ(result.image_data, image.image_data);
	ExpectPixelsEqual(pixels, original, LineWidth);
}

// ----------------------------------------------------------------------------
// Dot deinterlacing (mode 13h; KGB and Dune CD-ROM FMVs)
// ----------------------------------------------------------------------------

TEST(DeinterlacerTest, DotDeinterlaceFillsDotPatternAtFullStrength)
{
	auto pixels      = MakeDotInterlacedPixels(Content);
	const auto image = MakeBgrx32ImageView(pixels,
	                                       DotWidth,
	                                       DotHeight,
	                                       DotFmvVideoMode());

	Deinterlacer deinterlacer = {};
	deinterlacer.DeinterlaceInPlace(image, DeinterlacingStrength::Full);

	const auto expected = MakeExpectedDotDeinterlacedPixels(
	        Content, DotScaleFactor(DeinterlacingStrength::Full));

	ExpectPixelsEqual(pixels, expected, DotWidth);
}

TEST(DeinterlacerTest, DotDeinterlaceDimsFilledLinesPerStrength)
{
	using enum DeinterlacingStrength;

	for (const auto strength : {Light, Medium, Strong, Full}) {
		auto pixels      = MakeDotInterlacedPixels(Content);
		const auto image = MakeBgrx32ImageView(pixels,
		                                       DotWidth,
		                                       DotHeight,
		                                       DotFmvVideoMode());

		Deinterlacer deinterlacer = {};
		deinterlacer.DeinterlaceInPlace(image, strength);

		const auto expected = MakeExpectedDotDeinterlacedPixels(
		        Content, DotScaleFactor(strength));

		ExpectPixelsEqual(pixels, expected, DotWidth);
	}
}

TEST(DeinterlacerTest, DotDeinterlaceLeavesPatternlessImageUntouched)
{
	// Solid content from the top row: no leading black rows, no dot
	// pattern, so the deinterlacer must bail out immediately
	std::vector<uint32_t> pixels(static_cast<size_t>(DotWidth) * DotHeight,
	                             Content);
	const auto original = pixels;

	const auto image = MakeBgrx32ImageView(pixels,
	                                       DotWidth,
	                                       DotHeight,
	                                       DotFmvVideoMode());

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.DeinterlaceInPlace(image,
                                                            DeinterlacingStrength::Full);

	EXPECT_EQ(result.image_data, image.image_data);
	ExpectPixelsEqual(pixels, original, DotWidth);
}

// ----------------------------------------------------------------------------
// Mode dispatch
// ----------------------------------------------------------------------------

TEST(DeinterlacerTest, NonFmvModeIsPassedThrough)
{
	// Interlaced-looking content that must NOT be processed because the
	// video mode doesn't qualify for deinterlacing
	std::vector<uint32_t> pixels(static_cast<size_t>(320) * 200, Black);
	for (auto y = 40; y <= 98; y += 2) {
		FillRow(pixels, 320, y, Content);
	}
	const auto original = pixels;

	const auto image = MakeBgrx32ImageView(pixels, 320, 200, NonFmvVideoMode());

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.DeinterlaceInPlace(image,
                                                            DeinterlacingStrength::Full);

	EXPECT_EQ(result.image_data, image.image_data);
	EXPECT_EQ(result.params.pixel_format, PixelFormat::BGRX32_ByteArray);
	ExpectPixelsEqual(pixels, original, 320);
}

// ----------------------------------------------------------------------------
// Non-mutating capture-path deinterlacing
// ----------------------------------------------------------------------------

TEST(DeinterlacerTest, CopyingDeinterlaceDoesNotMutateBgrx32Input)
{
	auto pixels         = MakeLineInterlacedPixels(Content, Black);
	const auto original = pixels;

	const auto image = MakeBgrx32ImageView(pixels,
	                                       LineWidth,
	                                       LineHeight,
	                                       LineFmvVideoMode());

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.Deinterlace(image,
                                                     DeinterlacingStrength::Full);

	// The input must be untouched
	ExpectPixelsEqual(pixels, original, LineWidth);

	// The result is a view into the deinterlacer's internal buffer
	// holding the deinterlaced frame
	ASSERT_NE(result.image_data, image.image_data);

	EXPECT_EQ(result.params.pixel_format, PixelFormat::BGRX32_ByteArray);
	EXPECT_EQ(result.params.width, LineWidth);
	EXPECT_EQ(result.params.height, LineHeight);
	EXPECT_FALSE(result.params.rendered_double_scan);
	EXPECT_FALSE(result.params.rendered_pixel_doubling);
	EXPECT_EQ(result.pitch, LineWidth * static_cast<int>(sizeof(uint32_t)));

	const auto expected = MakeExpectedLineDeinterlacedPixels(
	        Content, Black, LineScaleFactor(DeinterlacingStrength::Full));

	ExpectPixelsEqual(ToBgrx32Pixels(result), expected, LineWidth);
}

TEST(DeinterlacerTest, CopyingLineDeinterlaceConvertsIndexed8ToBgrx32)
{
	std::vector<uint8_t> pixels(static_cast<size_t>(LineWidth) * LineHeight, 0);

	for (auto y = LineBlockFirstContentRow; y <= LineBlockLastContentRow; y += 2) {
		const auto row = pixels.begin() + static_cast<ptrdiff_t>(y) * LineWidth;
		std::fill(row, row + LineWidth, ContentIndex);
	}
	for (auto y = SolidBlockFirstRow; y <= SolidBlockLastRow; ++y) {
		const auto row = pixels.begin() + static_cast<ptrdiff_t>(y) * LineWidth;
		std::fill(row, row + LineWidth, ContentIndex);
	}
	const auto original = pixels;

	const auto image = MakeIndexed8ImageView(pixels,
	                                         LineWidth,
	                                         LineHeight,
	                                         LineFmvVideoMode());

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.Deinterlace(image,
                                                     DeinterlacingStrength::Full);

	EXPECT_EQ(pixels, original);
	ASSERT_NE(result.image_data, image.image_data);

	EXPECT_EQ(result.params.pixel_format, PixelFormat::BGRX32_ByteArray);
	EXPECT_EQ(result.params.width, LineWidth);
	EXPECT_EQ(result.params.height, LineHeight);

	const auto expected = MakeExpectedLineDeinterlacedPixels(
	        Content, Black, LineScaleFactor(DeinterlacingStrength::Full));

	ExpectPixelsEqual(ToBgrx32Pixels(result), expected, LineWidth);
}

TEST(DeinterlacerTest, CopyingDotDeinterlaceUndoublesBakedDoubleScanning)
{
	// Mode 13h with "baked-in" double scanning: the source holds 320x400
	// Indexed8 pixels with every video mode row rendered twice. This is
	// the exact input shape of KGB/Dune FMV captures (which used to get
	// corrupted by in-place 32-bit writes into the Indexed8 buffer).
	const auto source_height = DotHeight * 2;

	std::vector<uint8_t> pixels(static_cast<size_t>(DotWidth) * source_height, 0);

	for (auto y = DotBlockFirstRow; y <= DotBlockLastRow; y += 2) {
		const auto row0 = static_cast<size_t>(y) * 2 * DotWidth;

		for (auto x = 0; x < DotWidth; x += 2) {
			pixels[row0 + x]            = ContentIndex;
			pixels[row0 + DotWidth + x] = ContentIndex;
		}
	}
	for (auto y = DotSolidBlockFirstRow; y <= DotSolidBlockLastRow; ++y) {
		const auto row0 = pixels.begin() +
		                  static_cast<ptrdiff_t>(y) * 2 * DotWidth;
		std::fill(row0, row0 + DotWidth * 2, ContentIndex);
	}
	const auto original = pixels;

	auto image                        = MakeIndexed8ImageView(pixels,
                                           DotWidth,
                                           source_height,
                                           DotFmvVideoMode());
	image.params.rendered_double_scan = true;

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.Deinterlace(image,
                                                     DeinterlacingStrength::Full);

	EXPECT_EQ(pixels, original);
	ASSERT_NE(result.image_data, image.image_data);

	EXPECT_EQ(result.params.pixel_format, PixelFormat::BGRX32_ByteArray);
	EXPECT_EQ(result.params.width, DotWidth);
	EXPECT_EQ(result.params.height, DotHeight);
	EXPECT_FALSE(result.params.rendered_double_scan);

	// Baked double scanning must be re-expressed as post-render height
	// doubling so the params stay consistent with the video mode
	EXPECT_TRUE(result.params.double_height);

	const auto expected = MakeExpectedDotDeinterlacedPixels(
	        Content, DotScaleFactor(DeinterlacingStrength::Full));

	ExpectPixelsEqual(ToBgrx32Pixels(result), expected, DotWidth);
}

TEST(DeinterlacerTest, CopyingDeinterlaceConvertsEvenWithoutPattern)
{
	// A qualifying video mode with no interlacing pattern must still
	// return the decoded BGRX32 copy, so the capture output format stays
	// stable within a video mode.
	std::vector<uint8_t> pixels(static_cast<size_t>(DotWidth) * DotHeight,
	                            ContentIndex);
	const auto original = pixels;

	const auto image = MakeIndexed8ImageView(pixels,
	                                         DotWidth,
	                                         DotHeight,
	                                         DotFmvVideoMode());

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.Deinterlace(image,
                                                     DeinterlacingStrength::Full);

	EXPECT_EQ(pixels, original);
	ASSERT_NE(result.image_data, image.image_data);
	EXPECT_EQ(result.params.pixel_format, PixelFormat::BGRX32_ByteArray);

	const std::vector<uint32_t> expected(static_cast<size_t>(DotWidth) * DotHeight,
	                                     Content);

	ExpectPixelsEqual(ToBgrx32Pixels(result), expected, DotWidth);
}

TEST(DeinterlacerTest, CopyingDeinterlacePassesThroughNonFmvModes)
{
	std::vector<uint32_t> pixels(static_cast<size_t>(320) * 200, Content);

	const auto image = MakeBgrx32ImageView(pixels, 320, 200, NonFmvVideoMode());

	Deinterlacer deinterlacer = {};
	const auto result         = deinterlacer.Deinterlace(image,
                                                     DeinterlacingStrength::Full);

	EXPECT_EQ(result.image_data, image.image_data);
	EXPECT_EQ(result.params.pixel_format, PixelFormat::BGRX32_ByteArray);
}
