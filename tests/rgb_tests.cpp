// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/bgrx8888.h"
#include "utils/rgb.h"
#include "utils/rgb565.h"
#include "utils/rgb888.h"

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

TEST(rgb, type_sizes)
{
	auto as_bits = [](const size_t num_bytes) { return num_bytes * 8; };

	EXPECT_EQ(as_bits(sizeof(Rgb565)), 5 + 6 + 5);
	EXPECT_EQ(as_bits(sizeof(Rgb888)), 8 * 3);
	EXPECT_EQ(as_bits(sizeof(Bgrx8888)), 8 * 4);
}

TEST(rgb, rgb565_pixel_components)
{
	//                                  |  r5 |  g6  |  b5 |
	constexpr uint16_t rgb_as_uint16 = 0b00001'000011'00111;
	constexpr uint8_t r8             = 0b00001'000;
	constexpr uint8_t g8             = /* */ 0b000011'00;
	constexpr uint8_t b8             = /*        */ 0b00111'000;

	const Rgb565 rgb_object_from_uint16(rgb_as_uint16);
	const Rgb565 rgb_object_from_components(r8, g8, b8);

	EXPECT_EQ(rgb_object_from_uint16.pixel, rgb_object_from_components.pixel);
}

TEST(rgb, rgb888_byte_order)
{
	constexpr uint8_t r8 = 0b1000'0011;
	constexpr uint8_t g8 = 0b1000'0111;
	constexpr uint8_t b8 = 0b1000'1111;

	// Create a sequential array of objects
	constexpr Rgb888 rgb_array[3] = {
	        {r8, g8, b8},
	        {r8, g8, b8},
	        {r8, g8, b8},
	};

	// Treat the array as if it were video memory, i.e., a sequence of bytes.
	const auto byte_array = reinterpret_cast<const uint8_t*>(rgb_array);

	// If Rgb888 works properly, the colour values will always be in the
	// same place regardless of the hosts's endianness.
	//
	constexpr auto first_r8_offset  = sizeof(Rgb888) * 0 + 0;
	constexpr auto second_g8_offset = sizeof(Rgb888) * 1 + 1;
	constexpr auto third_b8_offset  = sizeof(Rgb888) * 2 + 2;

	EXPECT_EQ(byte_array[first_r8_offset], r8);
	EXPECT_EQ(byte_array[second_g8_offset], g8);
	EXPECT_EQ(byte_array[third_b8_offset], b8);
}

TEST(rgb, bgrx8888_byte_array)
{
	constexpr uint8_t r = 0b1000'0011;
	constexpr uint8_t g = 0b1000'0111;
	constexpr uint8_t b = 0b1000'1111;

	const Bgrx8888 bgrx_array[3] = {
	        {r, g, b},
	        {r, g, b},
	        {r, g, b},
	};

	const auto byte_array = reinterpret_cast<const uint8_t*>(bgrx_array);

	constexpr auto first_blue_offset   = sizeof(Bgrx8888) * 0 + 0;
	constexpr auto second_green_offset = sizeof(Bgrx8888) * 1 + 1;
	constexpr auto third_red_offset    = sizeof(Bgrx8888) * 2 + 2;

	EXPECT_EQ(byte_array[first_blue_offset], b);
	EXPECT_EQ(byte_array[second_green_offset], g);
	EXPECT_EQ(byte_array[third_red_offset], r);
}

TEST(rgb, bgrx8888_object)
{
	constexpr uint8_t r = 0b1000'0011;
	constexpr uint8_t g = 0b1000'0111;
	constexpr uint8_t b = 0b1000'1111;

	const Bgrx8888 bgrx_object(r, g, b);

	const uint32_t bgrx_as_uint32 = bgrx_object;
	constexpr auto array_size     = sizeof(bgrx_as_uint32);

	uint8_t byte_array[array_size] = {};
	memcpy(byte_array, &bgrx_as_uint32, array_size);

	EXPECT_EQ(byte_array[0], b);
	EXPECT_EQ(byte_array[1], g);
	EXPECT_EQ(byte_array[2], r);
}
