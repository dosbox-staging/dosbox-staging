// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/private/mapper.h"

#include <utility>

#include <gtest/gtest.h>

namespace {

using AxisPair = std::pair<int, int>;

TEST(CircleToSquare, center)
{
	EXPECT_EQ(CircleToSquare(0, 0), AxisPair(0, 0));
}

TEST(CircleToSquare, straight_directions_unchanged)
{
	EXPECT_EQ(CircleToSquare(32767, 0), AxisPair(32767, 0));
	EXPECT_EQ(CircleToSquare(-32768, 0), AxisPair(-32768, 0));
	EXPECT_EQ(CircleToSquare(0, 32767), AxisPair(0, 32767));
	EXPECT_EQ(CircleToSquare(0, -32768), AxisPair(0, -32768));
	EXPECT_EQ(CircleToSquare(10000, 0), AxisPair(10000, 0));
}

TEST(CircleToSquare, full_diagonal_reaches_corner)
{
	// 32767 * cos 45 = 23170
	EXPECT_EQ(CircleToSquare(23170, 23170), AxisPair(32767, 32767));
	EXPECT_EQ(CircleToSquare(-23170, 23170), AxisPair(-32767, 32767));
	EXPECT_EQ(CircleToSquare(23170, -23170), AxisPair(32767, -32767));
	EXPECT_EQ(CircleToSquare(-23170, -23170), AxisPair(-32767, -32767));
}

TEST(CircleToSquare, half_diagonal_stays_half)
{
	EXPECT_EQ(CircleToSquare(11585, 11585), AxisPair(16384, 16384));
}

TEST(CircleToSquare, keeps_direction)
{
	// Scaled by hypot(2, 1) / 2 = 1.118
	EXPECT_EQ(CircleToSquare(20000, 10000), AxisPair(22361, 11180));
	EXPECT_EQ(CircleToSquare(-10000, 20000), AxisPair(-11180, 22361));
}

TEST(CircleToSquare, clamps_beyond_the_circle)
{
	EXPECT_EQ(CircleToSquare(32767, 32767), AxisPair(32767, 32767));
	EXPECT_EQ(CircleToSquare(-32768, -32768), AxisPair(-32768, -32768));
	EXPECT_EQ(CircleToSquare(32767, 25000), AxisPair(32767, 31446));
}

} // namespace
