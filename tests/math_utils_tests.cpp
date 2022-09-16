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

#include "math_utils.h"

#include <cstdint>
#include <gtest/gtest.h>

namespace {

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

TEST(iroundf, valid)
{
	EXPECT_EQ(iroundf(0.0f), 0);

	EXPECT_EQ(iroundf(0.00000000001f), 0);
	EXPECT_EQ(iroundf(-0.00000000001f), 0);

	EXPECT_EQ(iroundf(0.5f), 1);
	EXPECT_EQ(iroundf(-0.5f), -1);

	EXPECT_EQ(iroundf(0.50001f), 1);
	EXPECT_EQ(iroundf(-0.50001f), -1);

	EXPECT_EQ(iroundf(0.499999f), 0);
	EXPECT_EQ(iroundf(-0.499999f), 0);

	assert(iroundf(1000000.4f) == 1000000);
	assert(iroundf(-1000000.4f) == -1000000);

	assert(iroundf(1000000.5f) == 1000001);
	assert(iroundf(-1000000.5f) == -1000001);
}

TEST(iroundf, invalid)
{
	EXPECT_DEBUG_DEATH({ iroundf(80000000000.0f); }, "");
	EXPECT_DEBUG_DEATH({ iroundf(-80000000000.0f); }, "");
}

} // namespace
