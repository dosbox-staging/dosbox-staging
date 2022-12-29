/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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
 *  A fixed-size thread-safe queue that blocks both the
 *  producer until space is available and the consumer until
 *  items are available.
 *
 *  For some background, this class was authored to replace
 *  the one that resulted from this discussion:
 *  https://github.com/cameron314/readerwriterqueue/issues/112
 *  because the MoodyCamel implementation:
 *   - Is roughly 5-fold larger (and more latent)
 *   - Consumes more CPU by spinning (instead of locking)
 *   - Lacks bulk queue/dequeue methods (request was rejected
 *     https://github.com/cameron314/readerwriterqueue/issues/130)
 */

#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

template <typename T>
class RWQueue {
private:
	std::deque<T> queue{}; // faster than: vector, queue, and list
	std::mutex mutex                  = {};
	std::condition_variable has_room  = {};
	std::condition_variable has_items = {};
	size_t capacity                   = 0;
	using difference_t = typename std::vector<T>::difference_type;

public:
	RWQueue()                                      = delete;
	RWQueue(const RWQueue<T>& other)               = delete;
	RWQueue<T>& operator=(const RWQueue<T>& other) = delete;

	RWQueue(size_t queue_capacity);
	void Resize(size_t queue_capacity);

	bool IsEmpty();
	size_t Size();
	size_t MaxCapacity() const;
	float GetPercentFull();

	// Discourage copying into the queue. Instead, use std::move into the
	// queue to explicitly invalidate the source object to avoid having
	// two source objects floating around.
	//
	// void Enqueue(const T& item);

	// items will be empty (moved-out) after call
	void Enqueue(T&& item);

	T Dequeue();

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
	// return (and can be reused).
	void BulkEnqueue(std::vector<T>& from_source, const size_t num_requested);

	// The target vector will be resized to accomodate, if needed.
	void BulkDequeue(std::vector<T>& into_target, const size_t num_requested);
};

#endif
