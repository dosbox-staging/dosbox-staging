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
#include "log_msg_knockout.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(CFifo, constructor_EmptyQueue)
{
	CFifo f(10);
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
	EXPECT_EQ(f.getb(), 0);
}

TEST(CFifo, clear_EmptyQueue)
{
	CFifo f(10);
	f.clear();
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
	EXPECT_EQ(f.getb(), 0);
}

TEST(CFifo, clear_ExistingQueue)
{
	CFifo f(10);
	f.addb(1);
	f.clear(); 

	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
	// EXPECT_EQ(f.getb(), 0); <--- should pass, but fails
	//
	// BUG
	//  - getb() will now return '1'
	//  - Inconsistent behavior versus prior inuse() == false state
	//  - Inconsistent behavior versus prior left() == 10 state
	//  - Inconsistent behavior versus prior clear()'ed state
}

TEST(CFifo, state_QueueExists)
{
	CFifo f(3);
	f.addb(1);
	EXPECT_TRUE(f.inuse());
	EXPECT_EQ(f.left(), 2);
}

TEST(CFifo, state_QueueFull)
{
	CFifo f(3);

	f.addb(1);
	f.addb(2);
	f.addb(3);

	EXPECT_TRUE(f.inuse());
	EXPECT_EQ(f.left(), 0);
}

TEST(CFifo, state_Empty)
{
	CFifo f(3);

	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 3);
}

TEST(CFifo, getb_QueueExists)
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

TEST(CFifo, getb_QueueFull)
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

TEST(CFifo, getb_Empty)
{
	CFifo f(10);
	EXPECT_EQ(f.getb(), 0);
	EXPECT_FALSE(f.inuse());
	EXPECT_EQ(f.left(), 10);
}

TEST(CFifo, addb_OverFlow)
{
	CFifo f(3);
	
	f.addb(1);
	f.addb(2);
	f.addb(3);
	f.addb(4); // overflows on 4th value and drops it
	EXPECT_EQ(f.getb(), 1);
	EXPECT_EQ(f.inuse(), 2);

	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.inuse(), 1);

	EXPECT_EQ(f.getb(), 3);
	EXPECT_EQ(f.inuse(), 0);
	EXPECT_EQ(f.left(), 3);

	// EXPECT_EQ(f.getb(), 0); <-- fails, it return '1'.
	// BUG:
	//  - Inconsistent result versus prior cleared states.
}

TEST(CFifo, adds_OverFlow)
{
	CFifo f(3);

	uint8_t four_vals[4] = {1, 2, 3, 4};
	f.adds(four_vals, 4);

	// Expected behavior:
	// - should match addb_OverFlow test above
	// - 1, 2, 3 should exist in the queue	
	// - 4th value should overflow and should be dropped
	
	/*

	// FAILS ALL OF THESE
	EXPECT_EQ(f.getb(), 1);
	EXPECT_EQ(f.inuse(), 2);

	EXPECT_EQ(f.getb(), 2);
	EXPECT_EQ(f.inuse(), 1);

	EXPECT_EQ(f.getb(), 3);
	EXPECT_EQ(f.inuse(), 0);

	EXPECT_EQ(f.getb(), 0);
	EXPECT_EQ(f.inuse(), 0);

	// BUG:
	// - Modem operations operated one byte at a time, therefore
	//   adds() should be equivalent to sequential addb()'s. 
	// - 'adds' drops the entire request, and return empty value

	*/
}

TEST(CFifo, setSize_Nominal)
{
	CFifo f(10);
	EXPECT_EQ(f.left(), 10);
}

} // namespace
