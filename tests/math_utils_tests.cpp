// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/math_utils.h"

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

TEST(are_almost_equal_relative, QuakeValues)
{
	// Numbers taken from Quake startup
	EXPECT_TRUE(are_almost_equal_relative(239.999999999999972, 240.0));
	EXPECT_TRUE(are_almost_equal_relative(23.999999999999996, 24.0));
	EXPECT_TRUE(are_almost_equal_relative(7.999999999999999, 8.0));
}

TEST(clamp_to_int8, signed_negatives)
{
	EXPECT_EQ(clamp_to_int8(INT16_MIN), INT8_MIN);
	EXPECT_EQ(clamp_to_int8(INT8_MIN - 0), INT8_MIN);
	EXPECT_EQ(clamp_to_int8(INT8_MIN + 1), INT8_MIN + 1);
}

TEST(clamp_to_int16, signed_negatives)
{
	EXPECT_EQ(clamp_to_int16(INT32_MIN), INT16_MIN);
	EXPECT_EQ(clamp_to_int16(INT16_MIN - 0), INT16_MIN);
	EXPECT_EQ(clamp_to_int16(INT16_MIN + 1), INT16_MIN + 1);
}

TEST(clamp_to_int32, signed_negatives)
{
	EXPECT_EQ(clamp_to_int32(INT64_MIN), INT32_MIN);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(INT32_MIN - 0)), INT32_MIN);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(INT32_MIN + 1)), INT32_MIN + 1);
}

TEST(clamp_to_int8, signed_positives)
{
	EXPECT_EQ(clamp_to_int8(INT16_MAX), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(INT8_MAX - 0), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(INT16_MAX - 1), INT8_MAX);
}

TEST(clamp_to_int16, signed_positives)
{
	EXPECT_EQ(clamp_to_int16(INT32_MAX), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(INT16_MAX - 0), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(INT32_MAX - 1), INT16_MAX);
}

TEST(clamp_to_int32, signed_positives)
{
	EXPECT_EQ(clamp_to_int32(INT64_MAX), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(INT64_MAX - static_cast<int64_t>(0)), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(INT64_MAX - static_cast<int64_t>(1)), INT32_MAX);
}

TEST(clamp_to_int8, signed_literals)
{
	EXPECT_EQ(clamp_to_int8(-1'000), INT8_MIN);
	EXPECT_EQ(clamp_to_int8(-100), -100);
	EXPECT_EQ(clamp_to_int8(-1), -1);
	EXPECT_EQ(clamp_to_int8(0), 0);
	EXPECT_EQ(clamp_to_int8(1), 1);
	EXPECT_EQ(clamp_to_int8(100), 100);
	EXPECT_EQ(clamp_to_int8(1'000), INT8_MAX);
}

TEST(clamp_to_int16, signed_literals)
{
	EXPECT_EQ(clamp_to_int16(-100'000), INT16_MIN);
	EXPECT_EQ(clamp_to_int16(-10'000), -10000);
	EXPECT_EQ(clamp_to_int16(-1'000), -1000);
	EXPECT_EQ(clamp_to_int16(-10), -10);
	EXPECT_EQ(clamp_to_int16(0), 0);
	EXPECT_EQ(clamp_to_int16(10), 10);
	EXPECT_EQ(clamp_to_int16(1'000), 1000);
	EXPECT_EQ(clamp_to_int16(10'000), 10000);
	EXPECT_EQ(clamp_to_int16(100'000), INT16_MAX);
}

TEST(clamp_to_int32, signed_literals)
{
	EXPECT_EQ(clamp_to_int32(-10'000'000'000), INT32_MIN);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(-1'000'000'000)),
	          -1'000'000'000);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(-1'000'000)), -1'000'000);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(-100)), -100);
	EXPECT_EQ(clamp_to_int32(0u), 0);
	EXPECT_EQ(clamp_to_int32(100u), 100);
	EXPECT_EQ(clamp_to_int32(1'000'000u), 1'000'000);
	EXPECT_EQ(clamp_to_int32(1'000'000'000u), 1'000'000'000);
	EXPECT_EQ(clamp_to_int32(10'000'000'000u), INT32_MAX);
}

#ifndef UINT8_MIN
constexpr uint8_t UINT8_MIN = {0};
#endif

#ifndef UINT16_MIN
constexpr uint16_t UINT16_MIN = {0};
#endif

#ifndef UINT32_MIN
constexpr uint32_t UINT32_MIN = {0};
#endif

#ifndef UINT64_MIN
constexpr uint64_t UINT64_MIN = {0};
#endif

TEST(clamp_to_int8, unsigned_minimums)
{
	EXPECT_EQ(clamp_to_int8(UINT16_MIN), 0);
	EXPECT_EQ(clamp_to_int8(UINT8_MIN - 0u), 0);
	EXPECT_EQ(clamp_to_int8(UINT8_MIN + 1u), 1);
}

TEST(clamp_to_int16, unsigned_minimums)
{
	EXPECT_EQ(clamp_to_int16(UINT32_MIN), 0);
	EXPECT_EQ(clamp_to_int16(UINT16_MIN - 0u), 0);
	EXPECT_EQ(clamp_to_int16(UINT16_MIN + 1u), 1);
}

TEST(clamp_to_int32, unsigned_minimums)
{
	EXPECT_EQ(clamp_to_int32(UINT64_MIN), 0);
	EXPECT_EQ(clamp_to_int32(UINT32_MIN - 0u), 0);
	EXPECT_EQ(clamp_to_int32(UINT32_MIN + 1u), 1);
}

TEST(clamp_to_int8, unsigned_maximums)
{
	EXPECT_EQ(clamp_to_int8(UINT16_MAX), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(UINT8_MAX - 0u), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(INT16_MAX - 1u), INT8_MAX);
}

TEST(clamp_to_int16, unsigned_maximums)
{
	EXPECT_EQ(clamp_to_int16(UINT32_MAX), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(UINT16_MAX - 0u), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(UINT32_MAX - 1u), INT16_MAX);
}

TEST(clamp_to_int32, unsigned_maximums)
{
	EXPECT_EQ(clamp_to_int32(UINT64_MAX), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(UINT32_MAX - 0u), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(UINT64_MAX - 1u), INT32_MAX);
}

TEST(clamp_to_int8, unsigned_literals)
{
	EXPECT_EQ(clamp_to_int8(0u), 0);
	EXPECT_EQ(clamp_to_int8(1u), 1);
	EXPECT_EQ(clamp_to_int8(100u), 100);
	EXPECT_EQ(clamp_to_int8(1'000u), INT8_MAX);
}

TEST(clamp_to_int16, unsigned_literals)
{
	EXPECT_EQ(clamp_to_int16(0u), 0);
	EXPECT_EQ(clamp_to_int16(10u), 10);
	EXPECT_EQ(clamp_to_int16(1'000u), 1000);
	EXPECT_EQ(clamp_to_int16(10'000u), 10'000);
	EXPECT_EQ(clamp_to_int16(100'000u), INT16_MAX);
}

TEST(clamp_to_int32, unsigned_literals)
{
	EXPECT_EQ(clamp_to_int32(0u), 0);
	EXPECT_EQ(clamp_to_int32(100u), 100);
	EXPECT_EQ(clamp_to_int32(1'000'000u), 1'000'000);
	EXPECT_EQ(clamp_to_int32(1'000'000'000u), 1'000'000'000);
	EXPECT_EQ(clamp_to_int32(10'000'000'000u), INT32_MAX);
}

TEST(ascii_to_bcd, test_string)
{
	std::vector<uint8_t> bcd = ascii_to_bcd("12345");
	ASSERT_EQ(bcd.size(), 3);
	EXPECT_EQ(bcd[0], (1 << 4) | 2);
	EXPECT_EQ(bcd[1], (3 << 4) | 4);
	EXPECT_EQ(bcd[2], 5 << 4);
}

TEST(round_to_multiple_of, zero)
{
	EXPECT_EQ(round_to_multiple_of(1, 0), 1);
	EXPECT_EQ(round_to_multiple_of(16, 0), 16);
}

TEST(round_to_multiple_of, positive)
{
	EXPECT_EQ(round_to_multiple_of(1, 1), 1);
	EXPECT_EQ(round_to_multiple_of(8, 48), 48);
	EXPECT_EQ(round_to_multiple_of(8, 49), 56);
	EXPECT_EQ(round_to_multiple_of(8, 65), 72);
	EXPECT_EQ(round_to_multiple_of(11, 7), 11);
	EXPECT_EQ(round_to_multiple_of(11, 12), 22);
	EXPECT_EQ(round_to_multiple_of(11, 11), 11);
}

TEST(round_to_multiple_of, negative)
{
	EXPECT_EQ(round_to_multiple_of(1, -1), -1);
	EXPECT_EQ(round_to_multiple_of(8, -48), -48);
	EXPECT_EQ(round_to_multiple_of(8, -49), -56);
	EXPECT_EQ(round_to_multiple_of(8, -65), -72);
	EXPECT_EQ(round_to_multiple_of(11, -7), -11);
	EXPECT_EQ(round_to_multiple_of(11, -12), -22);
	EXPECT_EQ(round_to_multiple_of(11, -11), -11);
}

} // namespace
