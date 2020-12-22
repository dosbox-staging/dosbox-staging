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

#include "serialport.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(MyFifo, QueryEmptyQueue)
{
	MyFifo f(3);
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getTop(), 0);
}

// Should match QueryEmptyQueue
TEST(MyFifo, QueryEmptyQueue1)
{
	MyFifo f(3);
	f.clear();
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getTop(), 0);
}

// Should match QueryEmptyQueue
TEST(MyFifo, QueryClearedQueue2)
{
	MyFifo f(10);
	f.addb(1);
	f.clear();

	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 3);
	EXPECT_EQ(f.getTop(), 0);
	EXPECT_EQ(f.probeByte(), 0);
	EXPECT_EQ(f.probeByte(), 0);
	EXPECT_EQ(f.getb(), 0);
}

TEST(MyFifo, QueryPartiallyFilledQueue)
{
	MyFifo f(3);
	f.addb(1);
	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 2);
	EXPECT_EQ(f.getUsage(), 1);
}

TEST(MyFifo, QueryFullyFilledQueue)
{
	MyFifo f(3);

	f.addb(1);
	f.addb(2);
	f.addb(3);

	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.getUsage());
	EXPECT_TRUE(f.isFull());
	EXPECT_EQ(f.getFree(), 0);
	EXPECT_EQ(f.getUsage(), 3);
}

TEST(MyFifo, GetFromEmptyQueue)
{
	MyFifo f(10);
	EXPECT_EQ(f.probeByte(), 0);
	EXPECT_EQ(f.getb(), 0);
}

TEST(MyFifo, GetFromPartiallyFilledQueue)
{
	MyFifo f(10);

	f.addb(1);
	EXPECT_EQ(f.probeByte(), 1);
	EXPECT_EQ(f.getb(), 1);

	f.addb(2);
	f.addb(3);
	EXPECT_EQ(f.probeByte(), 2);
	EXPECT_EQ(f.getb(), 2);

	EXPECT_EQ(f.probeByte(), 3);
	EXPECT_EQ(f.getb(), 3);
}

TEST(MyFifo, GetTopFromFullyFilledQueue)
{
	MyFifo f(3);

	f.addb(1);
	f.addb(2);
	f.addb(3);
}

TEST(MyFifo, GetFromFullyFilledQueue)
{
	MyFifo f(3);

	const uint8_t vals[3] = {1, 2, 3};
	f.addb(1);
	f.addb(2);
	f.addb(3);
	EXPECT_EQ(f.getTop(), 3);
	// '3' is the back of the queue and this passes

	// Drain the queue
	EXPECT_EQ(f.getb(), 1);
	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.getb(), 3);

	// 	// Should match QueryEmptyQueue
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getTop(), 0);

	f.addb(2);
	f.addb(3);
	// Should match first getTop above, as '3' is the back of the queue
	EXPECT_EQ(f.getTop(), 3);

	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.getb(), 3);

	// Should match QueryEmptyQueue
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 3);
	EXPECT_EQ(f.getUsage(), 0);
	EXPECT_EQ(f.getTop(), 0);
	// Inconsistent behaviour. Queue is empty, but results now depend on
	// prior states not on the actual reported state
}

TEST(MyFifo, GetFromOverflowedQueue1)
{
	MyFifo f(3);

	f.addb(1);
	f.addb(2);
	f.addb(3);
	f.addb(4); // overflows on 4th value

	EXPECT_EQ(f.getb(), 1);
	EXPECT_EQ(f.getUsage(), 2);
	EXPECT_EQ(f.getFree(), 0);

	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.getUsage(), 1);
	EXPECT_EQ(f.getFree(), 2);

	EXPECT_EQ(f.getb(), 3);
	EXPECT_EQ(f.getUsage(), 0);
	EXPECT_EQ(f.getFree(), 3);

	// Queue is now empty. Should match QueryEmptyQueue
	EXPECT_FALSE(f.getUsage());
	EXPECT_EQ(f.getFree(), 3);
	EXPECT_EQ(f.getb(), 0); // fails
}

TEST(MyFifo, SetQueueSize1)
{
	MyFifo f(0);
	EXPECT_EQ(f.getFree(), 0);
	EXPECT_EQ(f.getUsage(), 0);

	f.setSize(1);
	EXPECT_EQ(f.getFree(), 1);
	EXPECT_EQ(f.getUsage(), 0);
}

TEST(MyFifo, SetQueueSize2)
{
	MyFifo f(0);
	f.setSize(16);
	EXPECT_EQ(f.getFree(), 16);
	EXPECT_EQ(f.getUsage(), 0);
}

} // namespace
