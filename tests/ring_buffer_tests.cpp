// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdio>

#include "util/ring_buffer.h"

#include <gtest/gtest.h>

constexpr auto BufSize = 16;

RingBuffer<int, BufSize> buf = {};

void init_buf(int num_items = BufSize)
{
	auto it = buf.begin();
	for (auto i = 0; i < num_items; ++i) {
		*it++ = i;
	}
}

TEST(RingBuffer, read)
{
	init_buf();

	auto it = buf.begin();
	for (auto i = 0; i < BufSize; ++i) {
		EXPECT_EQ(*it, i);
		++it;
	}
}

TEST(RingBuffer, read_wraparound)
{
	init_buf();

	// read with wraparound 5 times
	auto it = buf.begin();
	for (auto i = 0; i < BufSize * 5; ++i) {
		EXPECT_EQ(*it, i % BufSize);
		++it;
	}
}

TEST(RingBuffer, write_wraparound)
{
	init_buf(BufSize + 3);

	auto it = buf.begin();
	EXPECT_EQ(*it++, BufSize);
	EXPECT_EQ(*it++, BufSize + 1);
	EXPECT_EQ(*it++, BufSize + 2);

	for (auto i = 3; i < BufSize; ++i) {
		EXPECT_EQ(*it, i);
		++it;
	}
}

TEST(RingBuffer, iterator_add)
{
	init_buf();

	auto it = buf.begin();
	EXPECT_EQ(*(it + 5), 5);

	it += 4;
	EXPECT_EQ(*it, 4);
	++it;
	EXPECT_EQ(*it, 5);
}

TEST(RingBuffer, iterator_add_wraparound)
{
	init_buf();

	auto it = buf.begin();
	EXPECT_EQ(*(it + 5), 5);
	EXPECT_EQ(*(it + 5 + BufSize - 1), 4);
	EXPECT_EQ(*(it + 5 + BufSize), 5);

	it += 5;
	EXPECT_EQ(*it, 5);
	it += BufSize - 1;
	EXPECT_EQ(*it, 4);
	++it;
	EXPECT_EQ(*it, 5);

	it = buf.begin() + BufSize - 1;
	EXPECT_EQ(*it, BufSize - 1);
	++it;
	EXPECT_EQ(*it, 0);
}

TEST(RingBuffer, iterator_sub)
{
	init_buf();

	auto it = buf.begin() + 5;
	EXPECT_EQ(*it, 5);

	EXPECT_EQ(*(it - 3), 2);
	it -= 2;
	EXPECT_EQ(*it, 3);
	--it;
	EXPECT_EQ(*it, 2);
}

TEST(RingBuffer, iterator_sub_wraparound)
{
	init_buf();

	auto it = buf.begin();
	EXPECT_EQ(*it, 0);

	EXPECT_EQ(*(it - 1), BufSize - 1);
	EXPECT_EQ(*(it - 2), BufSize - 2);

	--it;
	EXPECT_EQ(*it, BufSize - 1);

	it = buf.begin();
	it -= 5;
	EXPECT_EQ(*it, BufSize - 5);
}

TEST(RingBuffer, iterator_equality)
{
	init_buf();

	auto it = buf.begin();
	EXPECT_TRUE(it == buf.begin());
	EXPECT_FALSE(it == (buf.begin() + 1));

	++it;
	EXPECT_FALSE(it == buf.begin());
	EXPECT_TRUE((it - 1) == buf.begin());
}

TEST(RingBuffer, iterator_inequality)
{
	init_buf();

	auto it = buf.begin();
	EXPECT_FALSE(it != buf.begin());
	EXPECT_TRUE(it != (buf.begin() + 1));

	++it;
	EXPECT_TRUE(it != buf.begin());
	EXPECT_FALSE((it - 1) != buf.begin());
}
