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

#include "../src/hardware/serialport/fifo.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(Fifo, QueryEmptyQueue)
{
	Fifo f(3);

	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.isUsed());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.numFreeSlots(), 3);
	EXPECT_EQ(f.numQueued(), 0);
}

// Should match QueryEmptyQueue
TEST(Fifo, QueryClearedQueue1)
{
	Fifo f(3);
	f.clear();
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.isUsed());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.numFreeSlots(), 3);
	EXPECT_EQ(f.numQueued(), 0);
}

// Should match QueryEmptyQueue
TEST(Fifo, QueryClearedQueue2)
{
	Fifo f(3);
	f.push(1);
	f.clear();
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.isUsed());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.numFreeSlots(), 3);
	EXPECT_EQ(f.numQueued(), 0);
}

TEST(Fifo, QueryPartiallyFilledQueue)
{
	Fifo f(3);
	f.push(1);
	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.isUsed());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.numFreeSlots(), 2);
	EXPECT_EQ(f.numQueued(), 1);
}

TEST(Fifo, QueryFullyFilledQueue)
{
	Fifo f(3);

	f.push(1);
	f.push(2);
	f.push(3);

	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.isUsed());
	EXPECT_TRUE(f.isFull());
	EXPECT_EQ(f.numFreeSlots(), 0);
	EXPECT_EQ(f.numQueued(), 3);
}


TEST(Fifo, GetFromEmptyQueue)
{
	Fifo f(3);
	EXPECT_EQ(f.back(), 0);
	EXPECT_EQ(f.front(), 0);
	EXPECT_EQ(f.pop(), 0);

	f.push(1);
	(void) f.pop(); // queue is empty
	EXPECT_TRUE(f.isEmpty());
	EXPECT_EQ(f.back(), 0);
	EXPECT_EQ(f.front(), 0);
	EXPECT_EQ(f.pop(), 0);

	f.push(1);
	f.push(2);
	f.push(3);
	f.clear(); // queue is empty
	EXPECT_TRUE(f.isEmpty());
	EXPECT_EQ(f.back(), 0);
	EXPECT_EQ(f.front(), 0);
	EXPECT_EQ(f.pop(), 0);
}

TEST(Fifo, GetFromPartiallyFilledQueue)
{
	Fifo f(10);

	f.push(1);
	EXPECT_EQ(f.back(), 1);
	EXPECT_EQ(f.front(), 1);

	f.push(2);
	EXPECT_EQ(f.back(), 2);
	EXPECT_EQ(f.front(), 1);	

	f.push(3);
	EXPECT_EQ(f.back(), 3);
	EXPECT_EQ(f.front(), 1);	

	EXPECT_EQ(f.pop(), 1);
	EXPECT_EQ(f.pop(), 2);
	EXPECT_EQ(f.pop(), 3);

	// queue is now empty
	EXPECT_TRUE(f.isEmpty());
	EXPECT_EQ(f.back(), 0);
	EXPECT_EQ(f.front(), 0);
	EXPECT_EQ(f.pop(), 0);
}


TEST(Fifo, GetFromFullyFilledQueue)
{
	Fifo f(3);

	f.push(1);
	f.push(2);
	f.push(3);

	EXPECT_EQ(f.back(), 3);
	EXPECT_EQ(f.front(), 1);	

	EXPECT_EQ(f.pop(), 1);
	EXPECT_EQ(f.pop(), 2);
	EXPECT_EQ(f.pop(), 3);

	// queue is now empty
	EXPECT_TRUE(f.isEmpty());
	EXPECT_EQ(f.back(), 0);
	EXPECT_EQ(f.front(), 0);
	EXPECT_EQ(f.pop(), 0);
}

TEST(Fifo, GetFromOverflowedQueue1)
{
	Fifo f(3);

	f.push(1);
	f.push(2);
	f.push(3);
	f.push(4); // overflows on 4th value

	EXPECT_EQ(f.back(), 3);
	EXPECT_EQ(f.front(), 1);	

	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.isUsed());
	EXPECT_TRUE(f.isFull());
	EXPECT_EQ(f.numFreeSlots(), 0);
	EXPECT_EQ(f.numQueued(), 3);

	EXPECT_EQ(f.pop(), 1);
	EXPECT_EQ(f.pop(), 2);
	EXPECT_EQ(f.pop(), 3);

	// queue is now empty
	EXPECT_TRUE(f.isEmpty());
	EXPECT_EQ(f.back(), 0);
	EXPECT_EQ(f.front(), 0);
	EXPECT_EQ(f.pop(), 0);
}


TEST(Fifo, GetFromOverflowedQueue2)
{
	Fifo f(3);

	// overflow, should only drop the 4th
	const uint8_t four_vals[4] = {1, 2, 3, 4};
	f.pushMany(four_vals, 4);

	EXPECT_EQ(f.back(), 3);
	EXPECT_EQ(f.front(), 1);	

	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.isUsed());
	EXPECT_TRUE(f.isFull());
	EXPECT_EQ(f.numFreeSlots(), 0);
	EXPECT_EQ(f.numQueued(), 3);

	EXPECT_EQ(f.pop(), 1);
	EXPECT_EQ(f.pop(), 2);
	EXPECT_EQ(f.pop(), 3);

	// queue is now empty
	EXPECT_TRUE(f.isEmpty());
	EXPECT_EQ(f.back(), 0);
	EXPECT_EQ(f.front(), 0);
	EXPECT_EQ(f.pop(), 0);
}

TEST(Fifo, SetQueueSize1)
{
	Fifo f(0);
	EXPECT_EQ(f.numFreeSlots(), 0);
	EXPECT_EQ(f.numQueued(), 0);

	f.setSize(1);
	EXPECT_EQ(f.numFreeSlots(), 1);
	EXPECT_EQ(f.numQueued(), 0);
}

TEST(Fifo, SetQueueSize2)
{
	Fifo f(0);
	// bound to 1024 slots
	f.setSize(10000);
	EXPECT_EQ(f.numFreeSlots(), 1024);
	EXPECT_EQ(f.numQueued(), 0);
}

} // namespace
