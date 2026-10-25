// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/rwqueue.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <tuple>
#include <vector>

namespace {

constexpr auto iterations = 10000;

TEST(RWQueue, TrivialSerial)
{
	RWQueue<int> q(65);
	for (int iteration = 0; iteration != 128;
	     ++iteration) { // check there's no problem with mismatch
		            // between nominal and allocated capacity
		EXPECT_EQ(q.MaxCapacity(), 65);
		EXPECT_EQ(q.Size(), 0);
		EXPECT_TRUE(q.IsEmpty());
		q.Enqueue(0);
		EXPECT_EQ(q.MaxCapacity(), 65);
		EXPECT_EQ(q.Size(), 1);
		EXPECT_FALSE(q.IsEmpty());
		for (int i = 1; i != 65; ++i)
			q.Enqueue(std::move(i));
		EXPECT_EQ(q.Size(), 65);
		EXPECT_FALSE(q.IsEmpty());

		// Basic dequeue
		auto item = q.Dequeue();
		EXPECT_EQ(*item, 0);
		for (int i = 1; i != 65; ++i) {
			item = q.Dequeue();
			EXPECT_EQ(*item, i);
		}
		EXPECT_EQ(*item, 64);
		EXPECT_TRUE(q.IsEmpty());
	}
}

TEST(RWQueue, TrivialZeroCapacity)
{
	// Zero capacity
	EXPECT_DEBUG_DEATH({ RWQueue<int> q(0); }, "");
}

void rw_consume_trivial(RWQueue<int> *q, const size_t *max_depth)
{
	for (int i = 0; i != iterations; ++i) {
		EXPECT_TRUE(q->Size() <= *max_depth);
		const auto item = q->Dequeue();
		EXPECT_EQ(*item, i);
	}
}

/* Copying is disabled
void rw_produce_copy_trivial(RWQueue<int> *q, const size_t *max_depth)
{
	for (int i = 0; i != iterations; ++i) {
		q->Enqueue(i);
		EXPECT_TRUE(q->Size() <= *max_depth);
	}
}
 */

void rw_produce_move_trivial(RWQueue<int> *q, const size_t *max_depth)
{
	for (int i = 0; i != iterations; ++i) {
		q->Enqueue(std::move(i));
		EXPECT_TRUE(q->Size() <= *max_depth);
	}
}

/* Copying is disabled
TEST(RWQueue, TrivialCopyAsync)
{
	const size_t max_depth = 8;
	RWQueue<int> q(max_depth);

	std::thread writer(rw_produce_copy_trivial, &q, &max_depth);
	std::thread reader(rw_consume_trivial, &q, &max_depth);

	writer.join();
	reader.join();

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(q.Size(), 0);
}
 */

TEST(RWQueue, TrivialMoveAsync)
{
	const size_t max_depth = 8;
	RWQueue<int> q(max_depth);

	std::thread writer(rw_produce_move_trivial, &q, &max_depth);
	std::thread reader(rw_consume_trivial, &q, &max_depth);

	writer.join();
	reader.join();

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(q.Size(), 0);
}

void bulk_enqueue(RWQueue<int>& q, const size_t total_to_enqueue,
                  const size_t num_per_bulk_enqueue)
{
	// Make the index and values match, for easy testing
	auto i               = 0;

	assert(total_to_enqueue >= num_per_bulk_enqueue);
	auto remaining_items = total_to_enqueue;
	auto num_to_enqueue  = num_per_bulk_enqueue;

	std::vector<int> items = {};

	while (remaining_items > 0) {
		items.push_back(i);
		--remaining_items;

		if (items.size() == num_to_enqueue) {
			q.BulkEnqueue(items, num_to_enqueue);
			EXPECT_TRUE(items.empty());

			num_to_enqueue = std::min(remaining_items,
			                          num_per_bulk_enqueue);
		}
		++i;
	}
}

void bulk_dequeue(RWQueue<int>& q, const size_t total_to_dequeue,
                  const size_t num_per_bulk_dequeue)
{
	auto expected_front_val = 0;

	assert(total_to_dequeue >= num_per_bulk_dequeue);
	auto remaining_items = total_to_dequeue;
	auto num_to_dequeue  = num_per_bulk_dequeue;

	std::vector<int> items = {};

	while (remaining_items > 0) {
		num_to_dequeue = std::min(remaining_items, num_per_bulk_dequeue);

		q.BulkDequeue(items, num_to_dequeue);
		remaining_items -= num_to_dequeue;

		EXPECT_EQ(items.size(), num_to_dequeue);

		EXPECT_EQ(static_cast<int>(items.front()), expected_front_val);

		const auto expected_back_val = expected_front_val +
		                               (static_cast<int>(num_to_dequeue) - 1);
		EXPECT_EQ(static_cast<int>(items.back()), expected_back_val);

		expected_front_val = expected_back_val + 1;
	}
}

void run_bulk_async_test(const size_t queue_capacity,
                         const size_t num_per_bulk_enqueue,
                         const size_t num_per_bulk_dequeue, size_t total_to_queue)
{
	assert(total_to_queue >= num_per_bulk_enqueue);
	assert(total_to_queue >= num_per_bulk_dequeue);

	RWQueue<int> q(queue_capacity);

	std::thread writer(bulk_enqueue, std::ref(q), total_to_queue, num_per_bulk_enqueue);
	std::thread reader(bulk_dequeue, std::ref(q), total_to_queue, num_per_bulk_dequeue);

	writer.join();
	reader.join();

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(static_cast<int>(q.Size()), 0);
}

using bulk_params_t = typename std::tuple<size_t, size_t, size_t, size_t>;

TEST(RWQueue, AsyncBulkIOSingles)
{
	for (const auto& [queue_capacity,
	                  num_per_bulk_enqueue,
	                  num_per_bulk_dequeue,
	                  total_to_queue] : {

	             // queue matches total
	             bulk_params_t{1, 1, 1, 1},
	             bulk_params_t{50, 1, 1, 50},

	             // queue is smaller than total
	             bulk_params_t{1, 1, 1, 50},
	             bulk_params_t{50, 1, 1, 242},

	             // queue exceeds total
	             bulk_params_t{50, 1, 1, 1},
	             bulk_params_t{242, 1, 1, 50},

	     }) {
		run_bulk_async_test(queue_capacity,
		                    num_per_bulk_enqueue,
		                    num_per_bulk_dequeue,
		                    total_to_queue);
	}
}

TEST(RWQueue, AsyncBulkIOEqualSizes)
{
	for (const auto& [queue_capacity,
	                  num_per_bulk_enqueue,
	                  num_per_bulk_dequeue,
	                  total_to_queue] : {

	             bulk_params_t{50, 10, 10, 10},
	             bulk_params_t{10, 50, 50, 50},
	             bulk_params_t{10, 10, 10, 50},

	     }) {
		run_bulk_async_test(queue_capacity,
		                    num_per_bulk_enqueue,
		                    num_per_bulk_dequeue,
		                    total_to_queue);
	}
}

TEST(RWQueue, AsyncBulkIODequeueLargerThanEnqueue)
{
	for (const auto& [queue_capacity,
	                  num_per_bulk_enqueue,
	                  num_per_bulk_dequeue,
	                  total_to_queue] : {

	             bulk_params_t{50, 1, 2, 10},
	             bulk_params_t{10, 2, 5, 50},
	             bulk_params_t{10, 3, 10, 50},

	     }) {
		run_bulk_async_test(queue_capacity,
		                    num_per_bulk_enqueue,
		                    num_per_bulk_dequeue,
		                    total_to_queue);
	}
}

TEST(RWQueue, AsyncBulkIOSEnqueueLargerThanDequeue)
{
	for (const auto& [queue_capacity,
	                  num_per_bulk_enqueue,
	                  num_per_bulk_dequeue,
	                  total_to_queue] : {

	             bulk_params_t{50, 2, 1, 10},
	             bulk_params_t{10, 5, 2, 50},
	             bulk_params_t{10, 10, 3, 50},

	     }) {
		run_bulk_async_test(queue_capacity,
		                    num_per_bulk_enqueue,
		                    num_per_bulk_dequeue,
		                    total_to_queue);
	}
}

TEST(RWQueue, AsyncBulkIOSOverSized)
{
	for (const auto& [queue_capacity,
	                  num_per_bulk_enqueue,
	                  num_per_bulk_dequeue,
	                  total_to_queue] : {

	             bulk_params_t{1, 20, 1, 20},
	             bulk_params_t{7, 50, 2, 50},
	             bulk_params_t{3, 100, 3, 100},

	             bulk_params_t{1, 20, 1, 130},
	             bulk_params_t{7, 50, 2, 57},
	             bulk_params_t{3, 100, 3, 340},

	             bulk_params_t{1, 2, 100, 100},
	             bulk_params_t{9, 5, 20, 20},
	             bulk_params_t{4, 10, 30, 30},

	             bulk_params_t{1, 2, 100, 130},
	             bulk_params_t{9, 5, 20, 53},
	             bulk_params_t{4, 10, 30, 97},

	     }) {
		run_bulk_async_test(queue_capacity,
		                    num_per_bulk_enqueue,
		                    num_per_bulk_dequeue,
		                    total_to_queue);
	}
}

using container_t = std::vector<int16_t>;

TEST(RWQueue,ContainerSerial)
{
	RWQueue<container_t> q(65);
	for (int iteration = 0; iteration != 128;
	     ++iteration) { // check there's no problem with mismatch
		            // between nominal and allocated capacity
		EXPECT_EQ(q.MaxCapacity(), 65);
		EXPECT_EQ(q.Size(), 0);
		EXPECT_TRUE(q.IsEmpty());

		container_t v_outer(iteration + 1);
		v_outer[iteration] = static_cast<int16_t>(iteration);
		q.Enqueue(std::move(v_outer));
		EXPECT_EQ(v_outer.size(), 0); // check move

		EXPECT_EQ(q.MaxCapacity(), 65);
		EXPECT_EQ(q.Size(), 1);
		EXPECT_FALSE(q.IsEmpty());

		for (int i = 1; i != 65; ++i) {
			container_t v_inner(i + 1);
			v_inner[i] = static_cast<int16_t>(i);
			q.Enqueue(std::move(v_inner));
			EXPECT_EQ(v_inner.size(), 0); // check move
		}
		EXPECT_EQ(q.Size(), 65);
		EXPECT_FALSE(q.IsEmpty());

		// Basic dequeue
		v_outer = q.Dequeue().value();
		EXPECT_EQ(v_outer[0], 0);
		EXPECT_EQ(v_outer.size(), iteration + 1);

		for (int i = 1; i != 65; ++i) {
			v_outer = q.Dequeue().value();
			EXPECT_EQ(v_outer[i], i);
			EXPECT_EQ(v_outer.size(), i + 1);
		}
		EXPECT_EQ(v_outer[64], 64);
		EXPECT_EQ(v_outer.size(), 65);
		EXPECT_TRUE(q.IsEmpty());
	}
}

TEST(RWQueue,ContainerZeroCapacity)
{
	// Zero capacity
	EXPECT_DEBUG_DEATH({ RWQueue<container_t> q(0); }, "");
}

void rw_consume_container(RWQueue<container_t> *q, const size_t *max_depth)
{
	container_t v;
	for (int i = 0; i != iterations; ++i) {
		EXPECT_TRUE(q->Size() <= *max_depth);
		v = q->Dequeue().value();
		EXPECT_EQ(v[i], i);
		EXPECT_EQ(v.size(), i + 1);
	}
}

/* Copying is disabled
void rw_produce_copy_container(RWQueue<container_t> *q, const size_t *max_depth)
{
	for (int i = 0; i != iterations; ++i) {
		container_t v(i + 1);
		v[i] = i;
		q->Enqueue(v);
		EXPECT_EQ(v.size(), i + 1); // check copy
		EXPECT_TRUE(q->Size() <= *max_depth);
	}
}
 */

void rw_produce_move_container(RWQueue<container_t> *q, const size_t *max_depth)
{
	for (int i = 0; i != iterations; ++i) {
		container_t v(i + 1);
		v[i] = static_cast<int16_t>(i);
		q->Enqueue(std::move(v));
		EXPECT_EQ(v.size(), 0); // check move
		EXPECT_TRUE(q->Size() <= *max_depth);
	}
}

/* Copying is disabled
TEST(RWQueue,ContainerCopyAsync)
{
	const size_t max_depth = 8;
	RWQueue<container_t> q(max_depth);

	std::thread writer(rw_produce_copy_container, &q, &max_depth);
	std::thread reader(rw_consume_container, &q, &max_depth);

	writer.join();
	reader.join();

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(q.Size(), 0);
}
 */

TEST(RWQueue,ContainerMoveAsync)
{
	const size_t max_depth = 8;
	RWQueue<container_t> q(max_depth);

	std::thread writer(rw_produce_move_container, &q, &max_depth);
	std::thread reader(rw_consume_container, &q, &max_depth);

	writer.join();
	reader.join();

	// Make sure we've consumed all produced items and the queue is empty
	EXPECT_EQ(q.Size(), 0);
}

TEST(RWQueue, StopImmediately)
{
	RWQueue<int> q(65);

	q.Stop();
	EXPECT_FALSE(q.IsRunning());

	EXPECT_FALSE(q.Enqueue(1));      // shouldn't block
	EXPECT_TRUE(q.IsEmpty());
	const auto value = q.Dequeue();  // shouldn't block
	EXPECT_FALSE(value.has_value()); // once stopped, no long has a value
}

TEST(RWQueue, StopMidway)
{
	RWQueue<int> q(2);

	q.Enqueue(1);
	EXPECT_EQ(q.Size(), 1);
	EXPECT_FALSE(q.IsEmpty());
	EXPECT_TRUE(q.IsRunning());

	q.Stop();

	// Enqueuing fails after being stopped
	EXPECT_FALSE(q.IsRunning());
	const auto enqueue_result = q.Enqueue(2);
	EXPECT_FALSE(enqueue_result);

	// We still have one item in the queue, so we're not stopped yet
	auto value = q.Dequeue();
	EXPECT_EQ(*value, 1);

	// once stopped and out of items, dequeuing has stopped
	value = q.Dequeue();
	EXPECT_FALSE(value.has_value());
}

TEST(RWQueue, StopBulkImmediately)
{
	RWQueue<int> q(3);

	q.Stop();

	EXPECT_FALSE(q.IsRunning());

	std::vector<int> items = {1, 2, 3};
	const auto num_items = items.size();

	// Bulk enqueuing fails after being stopped
	const auto bulk_enqueue_result = q.BulkEnqueue(items, num_items);
	EXPECT_FALSE(bulk_enqueue_result);
	EXPECT_TRUE(q.IsEmpty());

	// Bulk dequeing fails after being stopped and without any items to deqeue
	const auto num_dequeued = q.BulkDequeue(items, num_items);
	EXPECT_FALSE(num_dequeued);
	EXPECT_TRUE(items.empty());
}

TEST(RWQueue, StopBulkMidway)
{
	RWQueue<int> q(8);

	// Bulk enque a couple before stopping
	std::vector<int> items = {1, 2, 3, 4, 5};

	auto bulk_enqueue_result = q.BulkEnqueue(items);
	EXPECT_TRUE(bulk_enqueue_result);
	EXPECT_TRUE(q.IsRunning());
	EXPECT_EQ(q.Size(), 5);

	q.Stop();

	// Bulking enqueuing fails after being stopped
	items = {6, 7};
	bulk_enqueue_result = q.BulkEnqueue(items);
	EXPECT_FALSE(bulk_enqueue_result);
	EXPECT_FALSE(q.IsRunning());
	EXPECT_EQ(q.Size(), 5);

	// But we still have a handful of items queued

	// Bulk dequeue the first couple
	auto num_items = 2u;
	auto num_dequeued = q.BulkDequeue(items, num_items);
	EXPECT_EQ(num_dequeued, num_items);
	EXPECT_EQ(q.Size(), 3);
	std::vector<int> expected_items = {1, 2};
	EXPECT_EQ(items, expected_items);

	// Dequeue the middle value
	auto value = q.Dequeue();
	EXPECT_EQ(q.Size(), 2);
	EXPECT_EQ(*value, 3);

	// Bulk dequeue the last couple, but over-request
	num_items = 3u;
	num_dequeued = q.BulkDequeue(items, num_items);
	EXPECT_TRUE(num_dequeued < num_items);
	EXPECT_EQ(num_dequeued, 2u);
	EXPECT_TRUE(q.IsEmpty());
	EXPECT_EQ(q.Size(), 0);
	expected_items = {4, 5};
	EXPECT_EQ(items, expected_items);

	// At this point, we should be out of items, but let's try bulk
	// dequeuing anyway
	num_items = 10u;
	num_dequeued = q.BulkDequeue(items, num_items);
	EXPECT_EQ(num_dequeued, 0);
	EXPECT_TRUE(items.empty());

	// At this point, we should be out of items, but let's try single
	// dequeuing anyway
	value = q.Dequeue();
	EXPECT_FALSE(value.has_value());
	EXPECT_TRUE(items.empty());
}

TEST(RWQueue, BulkDequeueZeroRequestFreshVector)
{
	RWQueue<int> q(8);

	// Never allocated, so data() is typically nullptr
	std::vector<int> target = {};

	EXPECT_EQ(q.BulkDequeue(target, 0), 0u);
	EXPECT_TRUE(target.empty());
}

TEST(RWQueue, BulkDequeueZeroRequestNullPointer)
{
	RWQueue<int> q(8);
	EXPECT_EQ(q.BulkDequeue(static_cast<int*>(nullptr), 0), 0u);
}

TEST(RWQueue, BulkDequeueZeroRequestLeavesQueueIntact)
{
	RWQueue<int> q(8);

	std::vector<int> items = {1, 2, 3};
	q.BulkEnqueue(items);

	int untouched = 0;
	EXPECT_EQ(q.BulkDequeue(&untouched, 0), 0u);
	EXPECT_EQ(untouched, 0);
	EXPECT_EQ(q.Size(), 3u);

	std::vector<int> out = {};
	EXPECT_EQ(q.BulkDequeue(out, 3), 3u);
	EXPECT_EQ(out, (std::vector<int>{1, 2, 3}));
}

TEST(RWQueue, NonblockingEnqueueFull)
{
	RWQueue<int> q(2);

	EXPECT_TRUE(q.NonblockingEnqueue(1));
	EXPECT_TRUE(q.NonblockingEnqueue(2));
	EXPECT_TRUE(q.IsFull());

	// Full: returns false instead of blocking
	EXPECT_FALSE(q.NonblockingEnqueue(3));
	EXPECT_EQ(q.Size(), 2u);
}

TEST(RWQueue, NonblockingEnqueueStopped)
{
	RWQueue<int> q(2);
	q.Stop();
	EXPECT_FALSE(q.NonblockingEnqueue(1));
	EXPECT_TRUE(q.IsEmpty());
}

TEST(RWQueue, NonblockingBulkEnqueuePartial)
{
	RWQueue<int> q(3);

	std::vector<int> items = {1, 2, 3, 4, 5};
	EXPECT_EQ(q.NonblockingBulkEnqueue(items), 3u);
	EXPECT_TRUE(q.IsFull());

	// Only the accepted items are removed from the source
	EXPECT_EQ(items, (std::vector<int>{4, 5}));

	// Queue is full: nothing taken, source untouched
	EXPECT_EQ(q.NonblockingBulkEnqueue(items), 0u);
	EXPECT_EQ(items, (std::vector<int>{4, 5}));

	std::vector<int> out = {};
	EXPECT_EQ(q.BulkDequeue(out, 3), 3u);
	EXPECT_EQ(out, (std::vector<int>{1, 2, 3}));
}

TEST(RWQueue, NonblockingBulkEnqueueStopped)
{
	RWQueue<int> q(3);
	q.Stop();

	std::vector<int> items = {1, 2};
	EXPECT_EQ(q.NonblockingBulkEnqueue(items), 0u);
	EXPECT_EQ(items.size(), 2u);
	EXPECT_TRUE(q.IsEmpty());
}

// There is no way to observe from the outside that a thread is parked inside
// a condition variable wait. Once the worker signals that it's about to make
// the blocking call, give it a moment to reach the wait.
void wait_for_worker_to_block(const std::atomic<bool>& worker_has_started)
{
	while (!worker_has_started) {
		std::this_thread::yield();
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(RWQueue, StopWakesBlockedDequeue)
{
	RWQueue<int> q(2);
	std::optional<int> result = 42;

	std::atomic<bool> has_started  = false;
	std::atomic<bool> has_returned = false;

	std::thread reader([&] {
		has_started  = true;
		result       = q.Dequeue();
		has_returned = true;
	});

	wait_for_worker_to_block(has_started);

	// Still blocked: nothing has been enqueued and the queue is running
	EXPECT_FALSE(has_returned);

	q.Stop();
	reader.join();

	EXPECT_FALSE(result.has_value());
}

TEST(RWQueue, StopWakesBlockedEnqueue)
{
	RWQueue<int> q(1);
	q.Enqueue(1);

	auto result = true;

	std::atomic<bool> has_started  = false;
	std::atomic<bool> has_returned = false;

	std::thread writer([&] {
		has_started  = true;
		result       = q.Enqueue(2);
		has_returned = true;
	});

	wait_for_worker_to_block(has_started);

	// Still blocked: nothing has been dequeued and the queue is running
	EXPECT_FALSE(has_returned);

	q.Stop();
	writer.join();

	EXPECT_FALSE(result);
	EXPECT_EQ(q.Size(), 1u);
}

TEST(RWQueue, StopIsIdempotentAndRestartable)
{
	RWQueue<int> q(2);

	q.Stop();
	q.Stop();
	EXPECT_FALSE(q.IsRunning());

	q.Start();
	EXPECT_TRUE(q.IsRunning());
	EXPECT_TRUE(q.Enqueue(1));
	EXPECT_EQ(*q.Dequeue(), 1);
}

TEST(RWQueue, ClearEmptiesQueue)
{
	RWQueue<int> q(4);
	q.Enqueue(1);
	q.Enqueue(2);
	EXPECT_EQ(q.Size(), 2u);

	q.Clear();
	EXPECT_TRUE(q.IsEmpty());
	EXPECT_EQ(q.MaxCapacity(), 4u);

	// Still usable afterwards
	EXPECT_TRUE(q.Enqueue(3));
	EXPECT_EQ(*q.Dequeue(), 3);
}

TEST(RWQueue, IsFullTracksCapacity)
{
	RWQueue<int> q(2);
	EXPECT_FALSE(q.IsFull());
	q.Enqueue(1);
	EXPECT_FALSE(q.IsFull());
	q.Enqueue(2);
	EXPECT_TRUE(q.IsFull());
	q.Dequeue();
	EXPECT_FALSE(q.IsFull());
}

TEST(RWQueue, GetPercentFull)
{
	RWQueue<int> q(4);
	EXPECT_FLOAT_EQ(q.GetPercentFull(), 0.0f);
	q.Enqueue(1);
	EXPECT_FLOAT_EQ(q.GetPercentFull(), 25.0f);
	q.Enqueue(2);
	q.Enqueue(3);
	q.Enqueue(4);
	EXPECT_FLOAT_EQ(q.GetPercentFull(), 100.0f);
}

TEST(RWQueue, ResizeChangesCapacity)
{
	RWQueue<int> q(4);
	q.Resize(10);
	EXPECT_EQ(q.MaxCapacity(), 10u);
}

TEST(RWQueue, ResizeZeroCapacity)
{
	RWQueue<int> q(4);
	EXPECT_DEBUG_DEATH({ q.Resize(0); }, "");
}

} // namespace
