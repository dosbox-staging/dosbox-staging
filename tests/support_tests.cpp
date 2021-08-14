/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

TEST(Support_split_delim, NoBoundingDelims)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split("a /b /c/d /e/f/", ' '), expected);
	EXPECT_EQ(split("abc", 'x'), std::vector<std::string>{"abc"});
}

TEST(Support_split_delim, DelimAtStartNotEnd)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(":a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split(" a /b /c/d /e/f/", ' '), expected);
}

TEST(Support_split_delim, DelimAtEndNotStart)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split("a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split("a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Support_split_delim, DelimsAtBoth)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split(":a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split(" a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Support_split_delim, MultiInternalDelims)
{
	const std::vector<std::string> expected(
	        {"a", "/b", "", "/c/d", "", "", "/e/f/"});
	EXPECT_EQ(split("a:/b::/c/d:::/e/f/", ':'), expected);
	EXPECT_EQ(split("a /b  /c/d   /e/f/", ' '), expected);
}

TEST(Support_split_delim, MultiBoundingDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "/c/d", "/e/f/", "", "", ""});
	EXPECT_EQ(split("::a:/b:/c/d:/e/f/:::", ':'), expected);
	EXPECT_EQ(split("  a /b /c/d /e/f/   ", ' '), expected);
}

TEST(Support_split_delim, MixedDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "", "/c/d", "/e/f/"});
	EXPECT_EQ(split("::a:/b::/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split("  a /b  /c/d /e/f/", ' '), expected);
}

TEST(Support_split_delim, Empty)
{
	const std::vector<std::string> empty;
	const std::vector<std::string> two({"", ""});
	const std::vector<std::string> three({"", "", ""});

	EXPECT_EQ(split("", ':'), empty);
	EXPECT_EQ(split(":", ':'), two);
	EXPECT_EQ(split("::", ':'), three);
	EXPECT_EQ(split("", ' '), empty);
	EXPECT_EQ(split(" ", ' '), two);
	EXPECT_EQ(split("  ", ' '), three);
}
 
TEST(Support_split, NoBoundingWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b /c/d /e/f/"), expected);
	EXPECT_EQ(split("abc"), std::vector<std::string>{"abc"});
}
TEST(Support_split, WhitespaceAtStartNotEnd)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(" a /b /c/d /e/f/"), expected);
}

TEST(Support_split, WhitespaceAtEndNotStart)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b /c/d /e/f/ "), expected);
}

TEST(Support_split, WhitespaceAtBoth)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(" a /b /c/d /e/f/ "), expected);
}

TEST(Support_split, MultiInternalWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b  /c/d   /e/f/"), expected);
}

TEST(Support_split, MultiBoundingWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("  a /b /c/d /e/f/   "), expected);
}

TEST(Support_split, MixedWhitespace)
{
	const std::vector<std::string> expected({"a", "b", "c"});
	EXPECT_EQ(split("\t\na\f\vb\rc"), expected);
	EXPECT_EQ(split("a\tb\f\vc"), expected);
	EXPECT_EQ(split(" a \n \v \r b \f \r c "), expected);
}

TEST(Support_split, Empty)
{
	const std::vector<std::string> empty;
	EXPECT_EQ(split(""), empty);
	EXPECT_EQ(split(" "), empty);
	EXPECT_EQ(split("   "), empty);
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

TEST(Support_left_shift_signed, Positive)
{
	// shifting zero ...
	int8_t var_8bit = 0;
	int16_t var_16bit = 0;
	int32_t var_32bit = 0;
	// by zero
	EXPECT_EQ(left_shift_signed(var_8bit, 0), 0);
	EXPECT_EQ(left_shift_signed(var_16bit, 0), 0);
	EXPECT_EQ(left_shift_signed(var_32bit, 0), 0);

	// shifting one ...
	var_8bit = 1;
	var_16bit = 1;
	var_32bit = 1;

	// by four
	EXPECT_EQ(left_shift_signed(var_8bit, 4), 16);
	EXPECT_EQ(left_shift_signed(var_16bit, 4), 16);
	EXPECT_EQ(left_shift_signed(var_32bit, 4), 16);

	// by max signed bits
	EXPECT_EQ(left_shift_signed(var_8bit, 6), 64);
	EXPECT_EQ(left_shift_signed(var_16bit, 14), 16384);
	EXPECT_EQ(left_shift_signed(var_32bit, 30), 1073741824);

	// max shiftable value before overflow
	var_8bit = INT8_MAX / 2;
	var_16bit = INT16_MAX / 2;
	var_32bit = INT32_MAX / 2;

	EXPECT_EQ(left_shift_signed(var_8bit, 1), INT8_MAX - 1);
	EXPECT_EQ(left_shift_signed(var_16bit, 1), INT16_MAX - 1);
	EXPECT_EQ(left_shift_signed(var_32bit, 1), INT32_MAX - 1);
}

TEST(Support_left_shift_signed, PositiveOverflow)
{
	int8_t var_8bit = INT8_MAX;
	int16_t var_16bit = INT16_MAX;
	int32_t var_32bit = INT32_MAX;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 1); }, "");

	var_8bit = 1;
	var_16bit = 1;
	var_32bit = 1;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 7); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 15); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 31); }, "");
}

TEST(Support_left_shift_signed, Negative)
{
	// shifting negative one ...
	int8_t var_8bit = -1;
	int16_t var_16bit = -1;
	int32_t var_32bit = -1;

	// by four
	EXPECT_EQ(left_shift_signed(var_8bit, 4), -16);
	EXPECT_EQ(left_shift_signed(var_16bit, 4), -16);
	EXPECT_EQ(left_shift_signed(var_32bit, 4), -16);

	// by max signed bits
	EXPECT_EQ(left_shift_signed(var_8bit, 7), INT8_MIN);
	EXPECT_EQ(left_shift_signed(var_16bit, 15), INT16_MIN);
	EXPECT_EQ(left_shift_signed(var_32bit, 31), INT32_MIN);

	// max shiftable value before overflow
	var_8bit = INT8_MIN / 2;
	var_16bit = INT16_MIN / 2;
	var_32bit = INT32_MIN / 2;

	EXPECT_EQ(left_shift_signed(var_8bit, 1), INT8_MIN);
	EXPECT_EQ(left_shift_signed(var_16bit, 1), INT16_MIN);
	EXPECT_EQ(left_shift_signed(var_32bit, 1), INT32_MIN);
}

TEST(Support_left_shift_signed, NegativeOverflow)
{
	int8_t var_8bit = INT8_MIN;
	int16_t var_16bit = INT16_MIN;
	int32_t var_32bit = INT32_MIN;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 1); }, "");

	var_8bit = -1;
	var_16bit = -1;
	var_32bit = -1;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 8); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 16); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 32); }, "");

	// Shift a negative number of bits
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, -1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, -100); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, -10000); }, "");
}

} // namespace
