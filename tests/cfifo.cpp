/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  The dosbox-staging team
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

#include "../src/hardware/serialport/softmodem.h"

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include <string>

namespace {

TEST(CFifo, QueryEmptyQueue)
{
	CFifo f(10);
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
	EXPECT_EQ(f.getb(), 0);
}

// Should match QueryEmptyQueue
TEST(CFifo, QueryClearedQueue1)
{
	CFifo f(10);
	f.clear();
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
	EXPECT_EQ(f.getb(), 0);
}

// Should match QueryEmptyQueue
TEST(CFifo, QueryClearedQueue2)
{
	CFifo f(10);
	f.addb(1);
	f.clear();
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
	EXPECT_EQ(f.getb(), 0);
}

TEST(CFifo, QueryPartiallyFilledQueue)
{
	CFifo f(3);
	f.addb(1);
	EXPECT_TRUE(f.inuse());
	EXPECT_EQ(f.left(), 2);
}

TEST(CFifo, QueryFullyFilledQueue)
{
	CFifo f(3);

	f.addb(1);
	f.addb(2);
	f.addb(3);

	EXPECT_TRUE(f.inuse());
	EXPECT_EQ(f.left(), 0);
}

TEST(CFifo, GetFromEmptyQueue)
{
	CFifo f(10);
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
	EXPECT_EQ(f.getb(), 0);
}

TEST(CFifo, GetFromPartiallyFilledQueue)
{
	CFifo f(3);
	f.addb(1);
	const auto val = f.getb();

	EXPECT_EQ(val, 1);
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 3);

	f.addb(2);
	EXPECT_TRUE(f.inuse());
	EXPECT_EQ(f.left(), 2);

	f.addb(3);
	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.getb(), 3);
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 3);
}

TEST(CFifo, GetFromFullyFilledQueue)
{
	CFifo f(3);

	uint8_t vals[3] = {1, 2, 3};
	f.adds(vals, 3);

	auto val = f.getb();
	EXPECT_EQ(val, 1);

	EXPECT_TRUE(f.inuse());
	EXPECT_EQ(f.left(), 1);

	val = f.getb();
	val = f.getb();

	EXPECT_EQ(val, 3);

	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 3);
}

TEST(CFifo, GetFromOverflowedQueue1)
{
	CFifo f(3);

	f.addb(1);
	f.addb(2);
	f.addb(3);
	f.addb(4); // overflows on 4th value

	EXPECT_EQ(f.getb(), 1);
	EXPECT_EQ(f.inuse(), 2);

	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.inuse(), 1);

	EXPECT_EQ(f.getb(), 3);
	EXPECT_EQ(f.inuse(), 0);
	EXPECT_EQ(f.left(), 3);

	// Queue is now empty. Should match QueryEmptyQueue
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 3);
	EXPECT_EQ(f.getb(), 0); // fails
}

// Serial port FIFOs operate one byte at a time, therefore expected behavior
// should match GetFromOverflowedQueue1
TEST(CFifo, GetFromOverflowedQueue2)
{
	CFifo f(3);

	uint8_t four_vals[4] = {1, 2, 3, 4};
	f.adds(four_vals, 4);

	// FAILS ALL OF THESE
	EXPECT_EQ(f.getb(), 1);
	EXPECT_EQ(f.inuse(), 2);

	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.inuse(), 1);

	EXPECT_EQ(f.getb(), 3);
	EXPECT_EQ(f.inuse(), 0);

	// Queue is now empty. Behavior should match the QueryEmptyQueue
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 3);
	EXPECT_EQ(f.getb(), 0);
}

TEST(CFifo, SetQueueSize)
{
	CFifo f(10);
	EXPECT_EQ(f.left(), 10);
}

} // namespace
