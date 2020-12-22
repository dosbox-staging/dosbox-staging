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
#include "log_msg_knockout.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(MyFifo, getTop_Empty)
{
	MyFifo f(3);
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getTop(), 0);
}


TEST(MyFifo, clear_Nominal)
{
	MyFifo f(10);
	f.addb(1);
	f.clear();

	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getTop(), 0);
	EXPECT_EQ(f.probeByte(), 0);
	EXPECT_EQ(f.probeByte(), 0);
	EXPECT_EQ(f.getb(), 0);
}

TEST(MyFifo, getTop_QueueExists)
{
	MyFifo f(3);

	f.addb(1);
	f.addb(2);
	f.addb(3);
	EXPECT_EQ(f.getTop(), 3);
	// '3' is the back of the queue and this passes

	// Drain the queue
	f.getb();
	f.getb();
	f.getb();

	// Confirm queue is empty:
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());	
	EXPECT_TRUE(f.isEmpty());

	// EXPECT_EQ(f.getTop(), 0); <-- should match EmptyQueue state
	// BUG
	//  - Inconsistent results. Queue is empty, but results now
	//    depending on prior states.

	f.addb(4);
	f.addb(5);
	// EXPECT_EQ(f.getTop(), 5); 
	// '5' is the back of the queue, but this fails.
	// BUG
	//  - Inconsistent behavior because getTop() will now return '2'
	//  - '2' in this case is at the front of the queue and the oldest
	//    item in the queue (which doesn't match prior behavior).
}

TEST(MyFifo, probeByte_QueueExists)
{
	MyFifo f(10);

	f.addb(1);
	EXPECT_EQ(f.probeByte(), 1);

	f.addb(2);

	f.addb(3);
	EXPECT_EQ(f.probeByte(), 1);
}

TEST(MyFifo, probeByte_Empty)
{
	MyFifo f(10);
	EXPECT_EQ(f.probeByte(), 0);
}

TEST(MyFifo, state_QueueExists)
{
	MyFifo f(3);
	f.addb(1);
	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 2);
	EXPECT_EQ(f.getUsage(), 1);
}

TEST(MyFifo, state_QueueFull)
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

TEST(MyFifo, state_Empty)
{
	MyFifo f(3);

	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 3);
	EXPECT_EQ(f.getUsage(), 0);
}

TEST(MyFifo, getb_QueueExists)
{
	MyFifo f(3);
	f.addb(1);
	const auto val = f.getb();

	EXPECT_EQ(val, 1);

	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 3);
	EXPECT_EQ(f.getUsage(), 0);
}

TEST(MyFifo, getb_QueueFull)
{
	MyFifo f(3);

	const uint8_t vals[3] = {1, 2, 3};
	f.addb(1);
	f.addb(2);
	f.addb(3);

	auto val = f.getb();
	EXPECT_EQ(val, 1);

	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 1);
	EXPECT_EQ(f.getUsage(), 2);

	val = f.getb();
	val = f.getb();
	EXPECT_EQ(val, 3);

	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 3);
	EXPECT_EQ(f.getUsage(), 0);

	val = f.getb();
	// EXPECT_EQ(val, 0); <-- fails, it return '1'.
	// BUG:
	//  - Inconsistent result versus prior cleared states.
}

TEST(MyFifo, getb_Empty)
{
	MyFifo f(10);
	EXPECT_EQ(f.getb(), 0);
	EXPECT_TRUE(f.isEmpty());
	EXPECT_FALSE(f.getUsage());
	EXPECT_FALSE(f.isFull());
	EXPECT_EQ(f.getFree(), 10);
	EXPECT_EQ(f.getUsage(), 0);
}

TEST(MyFifo, addb_OverFlow)
{
	MyFifo f(3);

	const uint8_t four_vals[4] = {1, 2, 3, 4};

	f.addb(1);
	f.addb(2);
	f.addb(3);
	f.addb(4);  // overflows on 4th value and drops it

	EXPECT_EQ(f.probeByte(), 1);
	EXPECT_FALSE(f.isEmpty());
	EXPECT_TRUE(f.isFull());
}

TEST(MyFifo, setSize_Nominal)
{
	MyFifo f(0);
	EXPECT_EQ(f.getFree(), 0);
	EXPECT_EQ(f.getUsage(), 0);

	f.setSize(1);
	EXPECT_EQ(f.getFree(), 1);
	EXPECT_EQ(f.getUsage(), 0);
}

TEST(MyFifo, setSize_TooMany)
{
	MyFifo f(0);
	f.setSize(16);
	EXPECT_EQ(f.getFree(), 16);
	EXPECT_EQ(f.getUsage(), 0);
}

} // namespace
