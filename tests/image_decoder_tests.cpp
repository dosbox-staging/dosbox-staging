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

// Distinct channel values so red/blue swaps are detectable
constexpr uint8_t Red   = 0xcc;
constexpr uint8_t Green = 0x88;
constexpr uint8_t Blue  = 0x44;

// The BGRX32 in-memory byte order is B,G,R,X, which reads as 0x00RRGGBB
// on little-endian (see `Bgrx8888`)
constexpr uint32_t ExpectedBgrx32 = (Red << 16) | (Green << 8) | Blue;

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
	image.palette[1] = Rgb888(Red, Green, Blue);

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrx32);
	EXPECT_EQ(row[1], 0x00000000u);
}

TEST(ImageDecoderTest, DecodesBgrx32ToBgrx32Unchanged)
{
	std::vector<uint32_t> pixels = {ExpectedBgrx32, 0x00000000};

	RenderedImage image = {};

	image.params.width        = 2;
	image.params.height       = 1;
	image.params.pixel_format = PixelFormat::BGRX32_ByteArray;

	image.pitch      = 2 * static_cast<int>(sizeof(uint32_t));
	image.image_data = reinterpret_cast<uint8_t*>(pixels.data());

	ImageDecoder decoder(image, 0, 0);

	std::vector<uint32_t> row(2);
	decoder.GetNextRowAsBgrx32Pixels(row.begin());

	EXPECT_EQ(row[0], ExpectedBgrx32);
	EXPECT_EQ(row[1], 0x00000000u);
}
