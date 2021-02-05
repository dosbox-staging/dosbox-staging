/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021       The DOSBox Staging Team
 *  Copyright (C) 2020-2021  Cameron Desrochers
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

// Unit tests for moodycamel::ReaderWriterCircularBuffer

#include "../src/libs/rwqueue/readerwritercircularbuffer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <thread>

namespace {
using namespace moodycamel;

TEST(ReaderWriterCircularBuffer, EnqueueDequeue)
{
	BlockingReaderWriterCircularBuffer<int> q(65);
	for (int iteration = 0; iteration != 128;
	     ++iteration) { // check there's no problem with mismatch
		            // between nominal and allocated capacity
		EXPECT_EQ(q.max_capacity(), 65);
		EXPECT_EQ(q.size_approx(), 0);
		EXPECT_TRUE(q.try_enqueue(0));
		EXPECT_EQ(q.max_capacity(), 65);
		EXPECT_EQ(q.size_approx(), 1);
		for (int i = 1; i != 65; ++i)
			q.wait_enqueue(i);
		EXPECT_EQ(q.size_approx(), 65);
		EXPECT_FALSE(q.try_enqueue(65));

		// Basic dequeue
		int item;
		EXPECT_TRUE(q.try_dequeue(item));
		EXPECT_EQ(item, 0);
		for (int i = 1; i != 65; ++i) {
			q.wait_dequeue(item);
			EXPECT_EQ(item, i);
		}
		EXPECT_FALSE(q.try_dequeue(item));
		EXPECT_FALSE(q.wait_dequeue_timed(item, 1));
		EXPECT_EQ(item, 64);
	}
}

TEST(ReaderWriterCircularBuffer, ZeroCapacity)
{
	// Zero capacity
	BlockingReaderWriterCircularBuffer<int> q(0);
	EXPECT_EQ(q.max_capacity(), 0);
	EXPECT_FALSE(q.try_enqueue(1));
	EXPECT_FALSE(q.wait_enqueue_timed(1, 0));
}

void consume(BlockingReaderWriterCircularBuffer<int> *q,
             const size_t *max_depth,
             weak_atomic<bool> *got_mismatch)
{
	int item;
	for (int i = 0; i != 1000000; ++i) {
		EXPECT_TRUE(q->size_approx() <= *max_depth);
		q->wait_dequeue(item);
		if (item != i && got_mismatch->load() == false) {
			*got_mismatch = true;
		}
	}
	// printf("exiting consumer thread on item %d\n", item);
}

void produce(BlockingReaderWriterCircularBuffer<int> *q, const size_t *max_depth)
{
	for (int i = 0; i != 1000000; ++i) {
		q->wait_enqueue(i);
		EXPECT_TRUE(q->size_approx() <= *max_depth);
	}
	// printf("exiting producer thread\n");
}

TEST(ReaderWriterCircularBuffer, BoundedAsyncProduceAndConsume)
{
	const size_t max_depth = 8;
	weak_atomic<bool> got_mismatch = false;

	BlockingReaderWriterCircularBuffer<int> q(max_depth);

	std::thread writer(produce, &q, &max_depth);
	std::thread reader(consume, &q, &max_depth, &got_mismatch);

	writer.join();
	reader.join();

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(q.size_approx(), 0);

	// Make sure there wasn't a single out-of-sequence item consumed
	EXPECT_FALSE(got_mismatch.load());

	// Confirm both threads report as terminated
	EXPECT_FALSE(reader.joinable());
	EXPECT_FALSE(writer.joinable());
}

} // namespace
