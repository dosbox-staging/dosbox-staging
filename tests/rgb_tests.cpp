/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "rgb.h"
#include "rgb888.h"

#include <gtest/gtest.h>

constexpr uint8_t rgb5_to_8_reference(const uint8_t c)
{
	return check_cast<uint8_t>((c * 255 + 15) / 31);
}

constexpr uint8_t rgb6_to_8_reference(const uint8_t c)
{
	return check_cast<uint8_t>((c * 255 + 31) / 63);
}

constexpr auto AbsError = 0.000001f;

TEST(rgb, Rgb888_FromRgb444)
{
	EXPECT_EQ(Rgb888::FromRgb444(0x0, 0x1, 0x2), Rgb888(0x00, 0x11, 0x22));
	EXPECT_EQ(Rgb888::FromRgb444(0x8, 0xe, 0xf), Rgb888(0x88, 0xee, 0xff));
}

TEST(rgb, rgb5_to_8)
{
	for (auto c = 0; c <= Rgb5Max; ++c) {
		EXPECT_EQ(rgb5_to_8(c), rgb5_to_8_reference(c));
	}
}

TEST(rgb, rgb6_to_8)
{
	for (auto c = 0; c <= Rgb6Max; ++c) {
		EXPECT_EQ(rgb6_to_8(c), rgb6_to_8_reference(c));
	}
}

TEST(rgb, rgb8_to_5)
{
	for (auto c = 0; c <= Rgb5Max; ++c) {
		const auto c8 = rgb5_to_8_reference(c);
		EXPECT_EQ(rgb8_to_5(c8), c);
	}
}

TEST(rgb, rgb8_to_6)
{
	for (auto c = 0; c <= Rgb6Max; ++c) {
		const auto c8 = rgb6_to_8_reference(c);
		EXPECT_EQ(rgb8_to_6(c8), c);
	}
}

TEST(rgb, rgb5_to_8_lut)
{
	for (auto c = 0; c <= Rgb5Max; ++c) {
		EXPECT_EQ(rgb5_to_8_lut(c), rgb5_to_8_reference(c));
	}
}

TEST(rgb, rgb6_to_8_lut)
{
	for (auto c = 0; c <= Rgb6Max; ++c) {
		EXPECT_EQ(rgb6_to_8_lut(c), rgb6_to_8_reference(c));
	}
}

TEST(rgb, srgb_to_linear)
{
	EXPECT_NEAR(srgb_to_linear(0.0f), 0.0000000f, AbsError);
	EXPECT_NEAR(srgb_to_linear(0.2f), 0.0331048f, AbsError);
	EXPECT_NEAR(srgb_to_linear(0.8f), 0.6038270f, AbsError);
	EXPECT_NEAR(srgb_to_linear(1.0f), 1.0000000f, AbsError);
}

TEST(rgb, linear_to_srgb)
{
	EXPECT_NEAR(linear_to_srgb(0.0000000f), 0.0f, AbsError);
	EXPECT_NEAR(linear_to_srgb(0.0331048f), 0.2f, AbsError);
	EXPECT_NEAR(linear_to_srgb(0.6038270f), 0.8f, AbsError);
	EXPECT_NEAR(linear_to_srgb(1.0000000f), 1.0f, AbsError);
}

TEST(rgb, srgb_linear_roundtrip)
{
	constexpr auto NumIter = 1000;
	for (auto i = 0; i < NumIter; ++i) {
		const float srgb1 = i * (1.0f / NumIter);
		const auto lin    = srgb_to_linear(srgb1);
		const auto srgb2  = linear_to_srgb(lin);
		EXPECT_NEAR(srgb1, srgb2, AbsError);
	}
}

TEST(rgb, srgb8_to_linear_lut)
{
	for (auto c = 0; c <= Rgb8Max; ++c) {
		const auto expected = srgb_to_linear(c * (1.0f / Rgb8Max));
		EXPECT_NEAR(srgb8_to_linear_lut(c), expected, AbsError);
	}
}

TEST(rgb, linear_to_srgb8_lut)
{
	// This is good enough accuracy with a (16 * 1024) element LUT
	constexpr auto NumIter = 500;
	for (auto i = 0; i < NumIter; ++i) {
		const float lin     = i * (1.0f / NumIter);
		const auto expected = static_cast<std::uint8_t>(
		        std::round(linear_to_srgb(lin) * Rgb8Max));
		EXPECT_EQ(linear_to_srgb8_lut(lin), expected);
	}
}
