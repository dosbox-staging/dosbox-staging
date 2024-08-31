/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_RWQUEUE_H
#define DOSBOX_RWQUEUE_H

#include "dosbox.h"

/*  RW (Read/Write) Queue
 *  ---------------------
 *  A fixed-size thread-safe queue that blocks both the producer until space is
 *  available, and the consumer until items are available.
 *
 *  For optimal performance inside the rwqueue, blocking is accomplished by
 *  putting the thread into the waiting state, then waking it up via notify when
 *  the required conditions are met.
 *
 *  Producer and consumer thread(s) are expected to simply call the enqueue and
 *  dequeue methods directly without any thread state management.
 */

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

#ifdef HAVE_MEMORY_RESOURCE
	#include <memory_resource>
#endif

template <typename T>
class RWQueue {
private:
#ifdef HAVE_MEMORY_RESOURCE
	std::pmr::unsynchronized_pool_resource pool = {};

	// The pooled deque recycles objects from the pool. It only allocates
	// when it needs to grow beyond its prior peak size.
	std::pmr::deque<T> queue = {};
#else
	std::deque<T> queue = {};
#endif
	std::mutex mutex                  = {};
	std::condition_variable has_room  = {};
	std::condition_variable has_items = {};
	size_t capacity                   = 0;
	bool is_running                   = true;
	using difference_t = typename std::vector<T>::difference_type;

public:
	RWQueue()                                      = delete;
	RWQueue(const RWQueue<T>& other)               = delete;
	RWQueue<T>& operator=(const RWQueue<T>& other) = delete;

	RWQueue(size_t queue_capacity);
	void Resize(size_t queue_capacity);

	// non-blocking call
	bool IsEmpty();

	// non-blocking call
	bool IsFull();

	// non-blocking call
	bool IsRunning();

	// non-blocking call
	size_t Size();

	// non-blocking call
	void Start();

	// non-blocking call
	void Stop();

	// non-blocking call
	void Clear();

	// non-blocking call
	size_t MaxCapacity();

	// non-blocking call
	float GetPercentFull();

	// Discourage copying into the queue. Instead, use std::move into the
	// queue to explicitly invalidate the source object to avoid having
	// two source objects floating around.
	//
	// void Enqueue(const T& item);

	// Items will be empty (moved-out) after call.
	// The method potentially blocks until the queue has enough capacity to
	// queue a single item.

	// If queuing has stopped prior to enqueing, then this will immediately
	//  return false and the item will not be queued.
	bool Enqueue(T&& item);

	// Returns false and does nothing if the queue is at capacity or the queue is not running
	// Otherwise the item gets moved into the queue and it returns true
	bool NonblockingEnqueue(T&& item);

	// The method potentially blocks until there is at least a single item
	// in the queue to dequeue.

	// If queuing has stopped, this will continue to return item(s) until
	// none remain in the queue, at which point it returns empty results
	// as indicated by the <optional> wrapper evaluating as "false".
	std::optional<T> Dequeue();

	// Bulk operations move multiple items from/to the given vector, which
	// signficantly reduces the number of mutex lock state changes. It also
	// uses references-to-vectors, such that they can be reused for the
	// entire lifetime of the application, avoiding costly repeated memory
	// reallocation.

	// The number of requested items can exceed the capacity of the queue
	// (the operation will be done in chunks, provided pressure on the other
	// side is relieved).

	// Items are std::move'd out of the source vector and also erased so the
	// source vector is returned in a defined state. The method potentially
	// blocks until there is enough capacity in the queue for the source items.

	// If the queue is stopped then the function unblocks and returns the
	// quantity enqueued (which can be less than the number requested).
	size_t BulkEnqueue(std::vector<T>& from_source, const size_t num_requested);

	// Bulk enqueue all of the source vector's items
	size_t BulkEnqueue(std::vector<T>& from_source);

	// Does nothing if queue is at capacity or queue is not running.
	// Otherwise, enqueues as many elements as possible until the queue is
	// at capacity. Moved elements will be erased from the source vector.
	// Elements not enqueued will be left in the source vector.
	// Returns the number of elements enqueued.
	size_t NonblockingBulkEnqueue(std::vector<T>& from_source, const size_t num_requested);

	// Bulk enqueue all of the source vector's items
	size_t NonblockingBulkEnqueue(std::vector<T>& from_source);

	// Deque's the requested number of items into the given container
	// in-bulk, and returns the quantity dequeued. This potentially blocks
	// until the requested number of items have been dequeued. Normally all
	// of the requested items are dequeued, however:
	//
	// - If queuing was stopped prior to bulk dequeueing then this
	//   immediately returns with a value of 0 and no items dequeued.
	//
	// - If queuing was stopped in the middle of bulk dequeueing then this
	//   immediately returns with a value indicating the subset dequeued.

	// The rwqueue takes care of sizing the target vector to accomodate the
	// number requested. On return, the vector's size matches the number
	// dequeued.
	size_t BulkDequeue(std::vector<T>& into_target, const size_t num_requested);

	// The caller is responsible for sizing the target's array to accomodate
	// the number requested.
	size_t BulkDequeue(T* const into_target, const size_t num_requested);
};

#endif
