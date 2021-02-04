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
#include "sdl_blocking_queue.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <thread>

namespace {
using namespace moodycamel;

constexpr auto zero_duration = std::chrono::duration<double, std::micro>(0);
auto mq_duration = zero_duration;
auto bq_duration = zero_duration;

constexpr auto iterations = 5000000;

void bq_consume(blocking_queue<int> *q,
                const size_t *max_depth,
                weak_atomic<bool> *got_mismatch)
{
	int item;
	for (int i = 0; i != iterations; ++i) {
		EXPECT_TRUE(q->size() <= *max_depth);
		item = q->front();
		q->pop();
		if (item != i && got_mismatch->load() == false) {
			*got_mismatch = true;
		}
	}
}

void bq_produce(blocking_queue<int> *q, const size_t *max_depth)
{
	for (int i = 0; i != iterations; ++i) {
		q->push(i);
		EXPECT_TRUE(q->size() <= *max_depth);
	}
}

TEST(blocking_queue, BoundedAsyncProduceAndConsume)
{
	const size_t max_depth = 8;
	weak_atomic<bool> got_mismatch = false;

	blocking_queue<int> q(max_depth);

	const auto start = std::chrono::high_resolution_clock::now();

	std::thread writer(bq_produce, &q, &max_depth);
	std::thread reader(bq_consume, &q, &max_depth, &got_mismatch);

	writer.join();
	reader.join();

	const auto end = std::chrono::high_resolution_clock::now();
	bq_duration = end - start;

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(q.size(), 0);

	// Make sure there wasn't a single out-of-sequence item consumed
	EXPECT_FALSE(got_mismatch.load());

	// Confirm both threads report as terminated
	EXPECT_FALSE(reader.joinable());
	EXPECT_FALSE(writer.joinable());
}

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

void mq_consume(BlockingReaderWriterCircularBuffer<int> *q,
                const size_t *max_depth,
                weak_atomic<bool> *got_mismatch)
{
	int item;
	for (int i = 0; i != iterations; ++i) {
		EXPECT_TRUE(q->size_approx() <= *max_depth);
		q->wait_dequeue(item);
		if (item != i && got_mismatch->load() == false) {
			*got_mismatch = true;
		}
	}
}

void mq_produce(BlockingReaderWriterCircularBuffer<int> *q, const size_t *max_depth)
{
	for (int i = 0; i != iterations; ++i) {
		q->wait_enqueue(i);
		EXPECT_TRUE(q->size_approx() <= *max_depth);
	}
}

TEST(ReaderWriterCircularBuffer, BoundedAsyncProduceAndConsume)
{
	const size_t max_depth = 8;
	weak_atomic<bool> got_mismatch = false;

	BlockingReaderWriterCircularBuffer<int> q(max_depth);

	const auto start = std::chrono::high_resolution_clock::now();

	std::thread writer(mq_produce, &q, &max_depth);
	std::thread reader(mq_consume, &q, &max_depth, &got_mismatch);

	writer.join();
	reader.join();

	const auto end = std::chrono::high_resolution_clock::now();
	mq_duration = end - start;

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(q.size_approx(), 0);

	// Make sure there wasn't a single out-of-sequence item consumed
	EXPECT_FALSE(got_mismatch.load());

	// Confirm both threads report as terminated
	EXPECT_FALSE(reader.joinable());
	EXPECT_FALSE(writer.joinable());
}

// For debug-builds, we expect the Moody queue to be twice as fast as our
// baseline queue. When optiimized, MQ can be 8x or faster on x86-64 and
// Aarch64.
TEST(CompareQueues, Durations)
{
	// Only compare results when both are populated
	if (mq_duration == zero_duration || bq_duration == zero_duration)
		return;

	EXPECT_TRUE(2 * mq_duration < bq_duration);
}

} // namespace
