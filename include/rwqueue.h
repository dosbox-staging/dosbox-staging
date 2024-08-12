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

#include <atomic>
#include <condition_variable>
#include <deque>

#ifdef HAVE_MEMORY_RESOURCE
	#include <memory_resource>
#endif

#include <mutex>
#include <optional>
#include <vector>

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
	std::atomic<bool> is_running      = true;
	using difference_t = typename std::vector<T>::difference_type;

public:
	RWQueue()                                      = delete;
	RWQueue(const RWQueue<T>& other)               = delete;
	RWQueue<T>& operator=(const RWQueue<T>& other) = delete;

	RWQueue(size_t queue_capacity);
	void Resize(size_t queue_capacity);

	bool IsEmpty();

	// non-blocking call
	bool IsRunning() const;

	// non-blocking call
	size_t Size();

	// non-blocking call
	void Start();

	// non-blocking call
	void Stop();

	// non-blocking call
	void Clear();

	// non-blocking call
	size_t MaxCapacity() const;

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

	// Items are std::move'd out of the source vector into the queue. This
	// function clear()s the vector such that it's in a defined state on
	// return (and can be reused). The method potentially blocks until there
	// is enough capacity in the queue for the new items.

	// If queuing has stopped prior to bulk enqueing, then this will
	// immediately return false and no items will be queued.

	// If queuing stops in the middle of enqueing prior to completion, then
	// this will immediately return false. The items queued /before/
	// stopping will be available in the queue however the items that came
	// after stopping will not be queued.
	bool BulkEnqueue(std::vector<T>& from_source, const size_t num_requested);

	// Does nothing if queue is at capacity or queue is not running.
	// Otherwise, enqueues as many elements as possible until the queue is at capacity.
	// Moved elements will be erased from the source vector.
	// Elements not enqueued will be left in the source vector.
	// Returns the number of elements enqueued.
	size_t NonblockingBulkEnqueue(std::vector<T>& from_source, const size_t num_requested);

	// The target vector will be resized to accomodate, if needed. The
	// method potentially blocks until the requested number of items have
	// been dequeued.

	// If queuing has stopped:
	//  - Returns true when  one or more item(s) have been dequeued.
	//  - Returns false when no items can be dequeued.
	//
	// The vector is always sized to match the number of items returned.
	bool BulkDequeue(std::vector<T>& into_target, const size_t num_requested);
};

#endif
