// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/image_decoder.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "misc/rendered_image.h"
#include "misc/video.h"
#include "utils/rgb888.h"

namespace {

// Distinct channel values so RGB channel swaps and skipped pixels are
// detectable
constexpr uint8_t RedA   = 0xcc;
constexpr uint8_t GreenA = 0x88;
constexpr uint8_t BlueA  = 0x44;

constexpr uint8_t RedB   = 0x11;
constexpr uint8_t GreenB = 0x22;
constexpr uint8_t BlueB  = 0x33;

// The BGRX32 in-memory byte order is B,G,R,X, which reads as 0x00RRGGBB
// on little-endian (see `Bgrx8888`)
constexpr uint32_t ExpectedBgrxA = (RedA << 16) | (GreenA << 8) | BlueA;
constexpr uint32_t ExpectedBgrxB = (RedB << 16) | (GreenB << 8) | BlueB;

// 5-bit R,G,B channel values 25,12,6 packed as -RRRRRGGGGGBBBBB
constexpr uint16_t PackedRgb555 = (25 << 10) | (12 << 5) | 6;

// The 8-bit channel values follow the `(c * 255 + 15) / 31` expansion
// (see `rgb5_to_8()`)
constexpr uint32_t ExpectedBgrxFromRgb555 = (206 << 16) | (99 << 8) | 49;

// 5/6/5-bit R,G,B channel values 25,33,6 packed as RRRRRGGGGGGBBBBB
constexpr uint16_t PackedRgb565 = (25 << 11) | (33 << 5) | 6;

// The 8-bit green value follows the `(c * 255 + 31) / 63` expansion
// (see `rgb6_to_8()`)
constexpr uint32_t ExpectedBgrxFromRgb565 = (206 << 16) | (134 << 8) | 49;

} // namespace

TEST(ImageDecoderTest, DecodesIndexed8ToBgrx32InBgrxByteOrder)
{
	std::vector<uint8_t> pixels = {1, 0};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.pitch      = 2;
	image.image_data = pixels.data();
	image.palette[1] = Rgb888(RedA, GreenA, BlueA);

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxA);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, DecodesRgb555ToBgrx32)
{
	std::vector<uint16_t> pixels = {PackedRgb555, 0x0000};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::RGB555_Packed16;

	image.pitch      = 2 * static_cast<int>(sizeof(uint16_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxFromRgb555);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, DecodesRgb565ToBgrx32)
{
	std::vector<uint16_t> pixels = {PackedRgb565, 0x0000};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::RGB565_Packed16;

	image.pitch      = 2 * static_cast<int>(sizeof(uint16_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxFromRgb565);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, DecodesBgr24ToBgrx32)
{
	// Pixels are stored in B,G,R byte order
	std::vector<uint8_t> pixels = {BlueA, GreenA, RedA, 0, 0, 0};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::BGR24_ByteArray;

	image.pitch      = 2 * 3;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxA);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, DecodesBgrx32ToBgrx32Unchanged)
{
	std::vector<uint32_t> pixels = {ExpectedBgrxA, 0x00000000};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::BGRX32_ByteArray;

	image.pitch      = 2 * static_cast<int>(sizeof(uint32_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxA);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, DecodesIndexed8ToIndexed8Unchanged)
{
	std::vector<uint8_t> pixels = {7, 42};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.pitch      = 2;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint8_t> row(2);
	decoder.GetNextRowAsIndexed8Pixels(row.begin());

	EXPECT_EQ(row[0], 7);
	EXPECT_EQ(row[1], 42);
}

TEST(ImageDecoderTest, AdvancesRowsUsingPitch)
{
	// Two 2-pixel rows with two padding bytes at the end of each row
	std::vector<uint8_t> pixels = {1, 2, 0xee, 0xee, 3, 4, 0xee, 0xee};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 2;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.pitch      = 4;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint8_t> row(2);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 1);
	EXPECT_EQ(row[1], 2);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 3);
	EXPECT_EQ(row[1], 4);
}

TEST(ImageDecoderTest, SkipsRowsWithRowSkipCount)
{
	std::vector<uint8_t> pixels = {1, 2, 3, 4, 5, 6, 7, 8};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 4;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.pitch      = 2;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 1, 0);

	std::vector<uint8_t> row(2);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 1);
	EXPECT_EQ(row[1], 2);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 5);
	EXPECT_EQ(row[1], 6);
}

TEST(ImageDecoderTest, DecodesFlippedImageBottomUp)
{
	std::vector<uint8_t> pixels = {1, 2, 3, 4};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 2;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.is_flipped_vertically = true;

	image.pitch      = 2;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint8_t> row(2);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 3);
	EXPECT_EQ(row[1], 4);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 1);
	EXPECT_EQ(row[1], 2);
}

TEST(ImageDecoderTest, SkipsRowsInFlippedImage)
{
	std::vector<uint8_t> pixels = {1, 2, 3, 4, 5, 6, 7, 8};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 4;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.is_flipped_vertically = true;

	image.pitch      = 2;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 1, 0);

	std::vector<uint8_t> row(2);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 7);
	EXPECT_EQ(row[1], 8);

	decoder.GetNextRowAsIndexed8Pixels(row.begin());
	EXPECT_EQ(row[0], 3);
	EXPECT_EQ(row[1], 4);
}

TEST(ImageDecoderTest, SkipsPixelsIndexed8ToBgrx32)
{
	// Every second pixel is a "poison pixel" that must be skipped
	std::vector<uint8_t> pixels = {1, 3, 2, 3};

	RenderedImage image = {};

	image.params.width        = 4;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.pitch      = 4;
	image.image_data = pixels.data();
	image.palette[1] = Rgb888(RedA, GreenA, BlueA);
	image.palette[2] = Rgb888(RedB, GreenB, BlueB);
	image.palette[3] = Rgb888(0xff, 0xff, 0xff);

	ImageDecoder decoder(image, 0, 1);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxA);
	EXPECT_EQ(row[1], ExpectedBgrxB);
}

TEST(ImageDecoderTest, SkipsPixelsRgb555ToBgrx32)
{
	// Every second pixel is a "poison pixel" that must be skipped
	std::vector<uint16_t> pixels = {PackedRgb555, 0x7fff, 0x0000, 0x7fff};

	RenderedImage image = {};

	image.params.width        = 4;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::RGB555_Packed16;

	image.pitch      = 4 * static_cast<int>(sizeof(uint16_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	ImageDecoder decoder(image, 0, 1);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxFromRgb555);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, SkipsPixelsRgb565ToBgrx32)
{
	// Every second pixel is a "poison pixel" that must be skipped
	std::vector<uint16_t> pixels = {PackedRgb565, 0xffff, 0x0000, 0xffff};

	RenderedImage image = {};

	image.params.width        = 4;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::RGB565_Packed16;

	image.pitch      = 4 * static_cast<int>(sizeof(uint16_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	ImageDecoder decoder(image, 0, 1);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxFromRgb565);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, SkipsPixelsBgr24ToBgrx32)
{
	// Every second pixel is a "poison pixel" that must be skipped
	std::vector<uint8_t> pixels = {
	        BlueA, GreenA, RedA, 0xff, 0xff, 0xff, BlueB, GreenB, RedB, 0xff, 0xff, 0xff};

	RenderedImage image = {};

	image.params.width        = 4;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::BGR24_ByteArray;

	image.pitch      = 4 * 3;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 0, 1);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxA);
	EXPECT_EQ(row[1], ExpectedBgrxB);
}

TEST(ImageDecoderTest, SkipsPixelsBgrx32ToBgrx32)
{
	// Every second pixel is a "poison pixel" that must be skipped
	std::vector<uint32_t> pixels = {ExpectedBgrxA, 0x00ffffff, ExpectedBgrxB, 0x00ffffff};

	RenderedImage image = {};

	image.params.width        = 4;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::BGRX32_ByteArray;

	image.pitch      = 4 * static_cast<int>(sizeof(uint32_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	ImageDecoder decoder(image, 0, 1);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrxA);
	EXPECT_EQ(row[1], ExpectedBgrxB);
}

TEST(ImageDecoderTest, SkipsPixelsIndexed8ToIndexed8)
{
	// Every second pixel is a "poison pixel" that must be skipped
	std::vector<uint8_t> pixels = {1, 9, 2, 9};

	RenderedImage image = {};

	image.params.width        = 4;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::Indexed8;

	image.pitch      = 4;
	image.image_data = pixels.data();

	ImageDecoder decoder(image, 0, 1);

	std::vector<uint8_t> row(2);
	decoder.GetNextRowAsIndexed8Pixels(row.begin());

	EXPECT_EQ(row[0], 1);
	EXPECT_EQ(row[1], 2);
}
