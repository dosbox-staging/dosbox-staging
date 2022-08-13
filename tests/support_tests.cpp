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

} // namespace
