/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

#include "support.h"

#include <cstdint>
#include <gtest/gtest.h>

#include <string>
#include <sys/types.h>

namespace {

TEST(DriveIndex, DriveA)
{
	EXPECT_EQ(0, drive_index('a'));
	EXPECT_EQ(0, drive_index('A'));
}

TEST(DriveIndex, DriveZ)
{
	EXPECT_EQ(25, drive_index('z'));
	EXPECT_EQ(25, drive_index('Z'));
}

TEST(Support_next_int, Signed)
{
	// 8-bit types should upgrade to int16_t
	EXPECT_EQ(typeid(next_int_t<int8_t>), typeid(int16_t));
	EXPECT_EQ(typeid(next_int_t<uint8_t>), typeid(int16_t));

	ASSERT_TRUE(sizeof(char) == 1);
	EXPECT_EQ(typeid(next_int_t<char>), typeid(int16_t));
	EXPECT_EQ(typeid(next_int_t<unsigned char>), typeid(int16_t));

	// 16-bit types should upgrade to int32_t
	EXPECT_EQ(typeid(next_int_t<int16_t>), typeid(int32_t));
	EXPECT_EQ(typeid(next_int_t<uint16_t>), typeid(int32_t));

	ASSERT_TRUE(sizeof(short) == 2);
	EXPECT_EQ(typeid(next_int_t<short>), typeid(int32_t));
	EXPECT_EQ(typeid(next_int_t<unsigned short>), typeid(int32_t));

	// 32-bit types should upgrade to int64_t
	EXPECT_EQ(typeid(next_int_t<int32_t>), typeid(int64_t));
	EXPECT_EQ(typeid(next_int_t<uint32_t>), typeid(int64_t));

	ASSERT_TRUE(sizeof(int) == 4);
	EXPECT_EQ(typeid(next_int_t<int>), typeid(int64_t));
	EXPECT_EQ(typeid(next_int_t<unsigned int>), typeid(int64_t));

	// 64-bit types should remain at parity with int64_t
	EXPECT_EQ(typeid(next_int_t<int64_t>), typeid(int64_t));
	EXPECT_EQ(typeid(next_int_t<uint64_t>), typeid(int64_t));

	ASSERT_TRUE(sizeof(long int) >= 4);
	EXPECT_EQ(typeid(next_int_t<long int>), typeid(int64_t));
	EXPECT_EQ(typeid(next_int_t<unsigned long int>), typeid(int64_t));
}

TEST(Support_next_int, SignedInvalid)
{
	// 8-bit types should not upgrade to int8_t, int32_t, or int64_t
	EXPECT_NE(typeid(next_int_t<int8_t>), typeid(int8_t));
	EXPECT_NE(typeid(next_int_t<int8_t>), typeid(int32_t));
	EXPECT_NE(typeid(next_int_t<int8_t>), typeid(int64_t));

	// 16-bit types should not upgrade to int8_t, int16_t, or int64_t
	EXPECT_NE(typeid(next_int_t<int16_t>), typeid(int8_t));
	EXPECT_NE(typeid(next_int_t<int16_t>), typeid(int16_t));
	EXPECT_NE(typeid(next_int_t<int16_t>), typeid(int64_t));

	// 32-bit types should not upgrade to int8_t, int16_t, or int32_t
	EXPECT_NE(typeid(next_int_t<int32_t>), typeid(int8_t));
	EXPECT_NE(typeid(next_int_t<int32_t>), typeid(int16_t));
	EXPECT_NE(typeid(next_int_t<int32_t>), typeid(int32_t));

	// 64-bit types should not upgrade to int8_t, int16_t, or int32_t
	EXPECT_NE(typeid(next_int_t<int64_t>), typeid(int8_t));
	EXPECT_NE(typeid(next_int_t<int64_t>), typeid(int16_t));
	EXPECT_NE(typeid(next_int_t<int64_t>), typeid(int32_t));
}

TEST(Support_next_uint, Unsigned)
{
	// 8-bit types should upgrade to uint16_t
	EXPECT_EQ(typeid(next_uint_t<int8_t>), typeid(uint16_t));
	EXPECT_EQ(typeid(next_uint_t<uint8_t>), typeid(uint16_t));

	ASSERT_TRUE(sizeof(char) == 1);
	EXPECT_EQ(typeid(next_uint_t<char>), typeid(uint16_t));
	EXPECT_EQ(typeid(next_uint_t<unsigned char>), typeid(uint16_t));

	// 16-bit types should upgrade to uint32_t
	EXPECT_EQ(typeid(next_uint_t<int16_t>), typeid(uint32_t));
	EXPECT_EQ(typeid(next_uint_t<uint16_t>), typeid(uint32_t));

	ASSERT_TRUE(sizeof(short) == 2);
	EXPECT_EQ(typeid(next_uint_t<short>), typeid(uint32_t));
	EXPECT_EQ(typeid(next_uint_t<unsigned short>), typeid(uint32_t));

	// 32-bit types should upgrade to uint64_t
	EXPECT_EQ(typeid(next_uint_t<int32_t>), typeid(uint64_t));
	EXPECT_EQ(typeid(next_uint_t<uint32_t>), typeid(uint64_t));

	ASSERT_TRUE(sizeof(int) == 4);
	EXPECT_EQ(typeid(next_uint_t<int>), typeid(uint64_t));
	EXPECT_EQ(typeid(next_uint_t<unsigned int>), typeid(uint64_t));

	// 64-bit types should remain at parity with uint64_t
	EXPECT_EQ(typeid(next_uint_t<int64_t>), typeid(uint64_t));
	EXPECT_EQ(typeid(next_uint_t<uint64_t>), typeid(uint64_t));

	ASSERT_TRUE(sizeof(long int) >= 4);
	EXPECT_EQ(typeid(next_uint_t<long int>), typeid(uint64_t));
	EXPECT_EQ(typeid(next_uint_t<unsigned long int>), typeid(uint64_t));
}

TEST(Support_next_uint, UnsignedInvalid)
{
	// 8-bit types should not upgrade to uint8_t, uint32_t, or uint64_t
	EXPECT_NE(typeid(next_uint_t<uint8_t>), typeid(uint8_t));
	EXPECT_NE(typeid(next_uint_t<uint8_t>), typeid(uint32_t));
	EXPECT_NE(typeid(next_uint_t<uint8_t>), typeid(uint64_t));

	// 16-bit types should not upgrade to uint8_t, uint16_t, or uint64_t
	EXPECT_NE(typeid(next_uint_t<uint16_t>), typeid(uint8_t));
	EXPECT_NE(typeid(next_uint_t<uint16_t>), typeid(uint16_t));
	EXPECT_NE(typeid(next_uint_t<uint16_t>), typeid(uint64_t));

	// 32-bit types should not upgrade to uint8_t, uint16_t, or uint32_t
	EXPECT_NE(typeid(next_uint_t<uint32_t>), typeid(uint8_t));
	EXPECT_NE(typeid(next_uint_t<uint32_t>), typeid(uint16_t));
	EXPECT_NE(typeid(next_uint_t<uint32_t>), typeid(uint32_t));

	// 64-bit types should not upgrade to uint8_t, uint16_t, or uint32_t
	EXPECT_NE(typeid(next_uint_t<uint64_t>), typeid(uint8_t));
	EXPECT_NE(typeid(next_uint_t<uint64_t>), typeid(uint16_t));
	EXPECT_NE(typeid(next_uint_t<uint64_t>), typeid(uint32_t));
}

template <typename T>
void test_randomizer(const T min_value, const T max_value)
{
	// Ensure the range is valid
	ASSERT_LT(min_value, max_value);

	// Calculate one quarter of the range to roughly test the distribution
	// of the generated values
	const auto quarter_range = (max_value - min_value) / 4;
	ASSERT_GT(quarter_range, 0);

	// Calculate a value roughly 25% greater than min
	const auto near_min = min_value + quarter_range;
	ASSERT_GT(near_min, min_value);

	// Calculate a value oughly 25% less than max
	const auto near_max = max_value - quarter_range;
	ASSERT_LT(near_max, max_value);

	// State trackers of what we've found so far
	bool found_near_middle = false;
	bool found_near_min = false;
	bool found_near_max = false;

	// Create a random value generator
	const auto generate_random_value = create_randomizer<T>(min_value, max_value);

	constexpr auto max_iterations = 1000;

	// Start generating and testing values
	for (auto i = 0; i < max_iterations; ++i) {

		const auto v = generate_random_value();
		EXPECT_GE(v, min_value);
		EXPECT_LE(v, max_value);

		const auto values_span_range = found_near_middle && found_near_min && found_near_max;
		if (values_span_range) {
			break;
		}
		else if (v > near_min && v < near_max) {
			found_near_middle = true;
		}
		else if (v < near_min) {
			found_near_min = true;
		} else if (v > near_max) {
			found_near_max = true;
		}
	}
	// We can only pass when values have been found near the middle, min, and max before we pass the test
	ASSERT_TRUE(found_near_middle);
	ASSERT_TRUE(found_near_min);
	ASSERT_TRUE(found_near_max);
}

TEST(create_randomizer, RangeOfLetters)
{
	// Ensure we're dealing with the standard ASCII character values
	ASSERT_EQ('A', 65);
	ASSERT_EQ('z', 122);

	test_randomizer<int16_t>('A', 'z');
}

TEST(create_randomizer, RangeOfFloats)
{
	// positive range
	test_randomizer(1000.0f, 2000.0f);

	// negative range
	test_randomizer(-2000.0f, -1000.0f);

	// postitive and negative range
	test_randomizer(-32000.0f, 32000.0f);

	// positive percent-as-ratio
	test_randomizer(0.0f, 1.0f);

	// negative percent-as-ratio
	test_randomizer(-1.0f, 0.0f);

	// postitive and negative percent-as-ratio
	test_randomizer(-1.0f, 1.0f);
}

} // namespace
