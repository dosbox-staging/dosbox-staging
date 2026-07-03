// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/render/format_convert.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace {

// All expected values below are hand-derived from the channel layouts, NOT
// from the conversion expressions under test.

TEST(FormatConvert, MakeBgrx32PaletteEntrySetsXTo255)
{
	// X = 255 matches what GFX_MakePixel() produced for the legacy
	// scaler's palette LUT
	EXPECT_EQ(MakeBgrx32PaletteEntry(0, 0, 0), 0xff000000u);
	EXPECT_EQ(MakeBgrx32PaletteEntry(255, 0, 0), 0xffff0000u);
	EXPECT_EQ(MakeBgrx32PaletteEntry(0, 255, 0), 0xff00ff00u);
	EXPECT_EQ(MakeBgrx32PaletteEntry(0, 0, 255), 0xff0000ffu);
	EXPECT_EQ(MakeBgrx32PaletteEntry(255, 255, 255), 0xffffffffu);
	EXPECT_EQ(MakeBgrx32PaletteEntry(0x12, 0x34, 0x56), 0xff123456u);
}

TEST(FormatConvert, ExpandIndexed8LooksUpPaletteLut)
{
	std::array<uint32_t, 256> lut = {};
	for (auto i = 0; i < 256; ++i) {
		lut[i] = MakeBgrx32PaletteEntry(static_cast<uint8_t>(i),
		                                static_cast<uint8_t>(255 - i),
		                                static_cast<uint8_t>(i / 2));
	}

	const std::array<uint8_t, 5> src = {0, 1, 127, 254, 255};
	std::array<uint32_t, 5> dst      = {};

	ExpandIndexed8ToBgrx32(src.data(), dst.data(), 5, lut.data());

	EXPECT_EQ(dst[0], 0xff00ff00u);
	EXPECT_EQ(dst[1], 0xff01fe00u);
	EXPECT_EQ(dst[2], 0xff7f803fu);
	EXPECT_EQ(dst[3], 0xfffe017fu);
	EXPECT_EQ(dst[4], 0xffff007fu);
}

TEST(FormatConvert, ExpandRgb555ChannelPlacement)
{
	// xRRRrrGGGggBBBbb; 5-bit channels expand to 8 bits with the top 3
	// bits replicated into the bottom 3
	const std::array<uint16_t, 7> src = {
	        0x7c00, // pure red
	        0x03e0, // pure green
	        0x001f, // pure blue
	        0x7fff, // white
	        0x0000, // black
	        0x8000, // only the unused x bit set -> black
	        0x4210, // r/g/b = 10000b each -> 10000100b = 0x84 each
	};
	std::array<uint32_t, 7> dst = {};

	ExpandRgb555ToBgrx32(src.data(), dst.data(), 7);

	EXPECT_EQ(dst[0], 0x00ff0000u);
	EXPECT_EQ(dst[1], 0x0000ff00u);
	EXPECT_EQ(dst[2], 0x000000ffu);
	EXPECT_EQ(dst[3], 0x00ffffffu);
	EXPECT_EQ(dst[4], 0x00000000u);
	EXPECT_EQ(dst[5], 0x00000000u);
	EXPECT_EQ(dst[6], 0x00848484u);
}

TEST(FormatConvert, ExpandRgb565ChannelPlacement)
{
	// RRRrrGGggggBBBbb; 5-bit channels replicate their top 3 bits, the
	// 6-bit green channel its top 2
	const std::array<uint16_t, 6> src = {
	        0xf800, // pure red
	        0x07e0, // pure green
	        0x001f, // pure blue
	        0xffff, // white
	        0x0000, // black
	        0x8410, // r/b = 10000b -> 0x84; g = 100000b -> 10000010b = 0x82
	};
	std::array<uint32_t, 6> dst = {};

	ExpandRgb565ToBgrx32(src.data(), dst.data(), 6);

	EXPECT_EQ(dst[0], 0x00ff0000u);
	EXPECT_EQ(dst[1], 0x0000ff00u);
	EXPECT_EQ(dst[2], 0x000000ffu);
	EXPECT_EQ(dst[3], 0x00ffffffu);
	EXPECT_EQ(dst[4], 0x00000000u);
	EXPECT_EQ(dst[5], 0x00848284u);
}

TEST(FormatConvert, ExpandBgr24ReadsPackedTriplets)
{
	// Tightly packed B, G, R byte triplets
	const std::array<uint8_t, 9> src = {
	        0x11,
	        0x22,
	        0x33, // pixel 0: B=11 G=22 R=33
	        0xff,
	        0x00,
	        0x00, // pixel 1: pure blue
	        0x00,
	        0x00,
	        0xff, // pixel 2: pure red
	};
	std::array<uint32_t, 3> dst = {};

	ExpandBgr24ToBgrx32(src.data(), dst.data(), 3);

	EXPECT_EQ(dst[0], 0x00332211u);
	EXPECT_EQ(dst[1], 0x000000ffu);
	EXPECT_EQ(dst[2], 0x00ff0000u);
}

TEST(FormatConvert, CopyBgrx32PreservesXByte)
{
	const std::array<uint32_t, 3> src = {0xab123456u, 0x00000000u, 0xffffffffu};
	std::array<uint32_t, 3> dst = {};

	CopyBgrx32(src.data(), dst.data(), 3);

	EXPECT_EQ(dst[0], 0xab123456u);
	EXPECT_EQ(dst[1], 0x00000000u);
	EXPECT_EQ(dst[2], 0xffffffffu);
}

TEST(FormatConvert, ExpandBgrx32RowCopiesWhenNotDoubling)
{
	const std::array<uint32_t, 3> src = {0x00112233u, 0x00445566u, 0x00778899u};
	std::array<uint32_t, 3> dst = {};

	ExpandBgrx32Row(src.data(), dst.data(), 3, false);

	EXPECT_EQ(dst, src);
}

TEST(FormatConvert, ExpandBgrx32RowDoublesEachPixel)
{
	const std::array<uint32_t, 3> src = {0x00112233u, 0x00445566u, 0x00778899u};
	std::array<uint32_t, 6> dst = {};

	ExpandBgrx32Row(src.data(), dst.data(), 3, true);

	const std::array<uint32_t, 6> expected = {0x00112233u,
	                                          0x00112233u,
	                                          0x00445566u,
	                                          0x00445566u,
	                                          0x00778899u,
	                                          0x00778899u};
	EXPECT_EQ(dst, expected);
}

TEST(FormatConvert, OddPixelCountsConvertEveryPixel)
{
	// The legacy scaler processed pixels in groups; the expanders must
	// handle any count exactly
	constexpr auto NumPixels = 7;

	std::array<uint32_t, 256> lut = {};
	lut[9]                        = 0xff102030u;

	const std::vector<uint8_t> src8(NumPixels, 9);
	const std::vector<uint16_t> src16(NumPixels, 0x7fff);
	const std::vector<uint8_t> src24(NumPixels * 3, 0xff);
	const std::vector<uint32_t> src32(NumPixels, 0x00c0ffeeu);

	std::vector<uint32_t> dst(NumPixels, 0xdeadbeefu);

	ExpandIndexed8ToBgrx32(src8.data(), dst.data(), NumPixels, lut.data());
	EXPECT_EQ(dst, std::vector<uint32_t>(NumPixels, 0xff102030u));

	ExpandRgb555ToBgrx32(src16.data(), dst.data(), NumPixels);
	EXPECT_EQ(dst, std::vector<uint32_t>(NumPixels, 0x00ffffffu));

	// 0x7fff in 565 is r=01111b g=111111b b=11111b -> r expands to 0x7b
	ExpandRgb565ToBgrx32(src16.data(), dst.data(), NumPixels);
	EXPECT_EQ(dst, std::vector<uint32_t>(NumPixels, 0x007bffffu));

	ExpandBgr24ToBgrx32(src24.data(), dst.data(), NumPixels);
	EXPECT_EQ(dst, std::vector<uint32_t>(NumPixels, 0x00ffffffu));

	CopyBgrx32(src32.data(), dst.data(), NumPixels);
	EXPECT_EQ(dst, std::vector<uint32_t>(NumPixels, 0x00c0ffeeu));
}

} // namespace
