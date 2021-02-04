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

#include <condition_variable>
#include <deque>
#include <mutex>

template <typename T>
class RWQueue {
private:
	std::deque<T> queue{}; // faster than: vector, queue, and list
	std::mutex mutex = {};
	std::condition_variable has_room = {};
	std::condition_variable has_items = {};
	const size_t capacity = 0;

public:
	RWQueue() = delete;
	RWQueue(const RWQueue<T> &other) = delete;
	RWQueue<T> &operator=(const RWQueue<T> &other) = delete;

	RWQueue(size_t queue_capacity);

	bool IsEmpty();
	size_t Size();
	size_t MaxCapacity() const;

	void Enqueue(const T &item);
	void Enqueue(T &&item); // item will be empty (moved-out) after call
	T Dequeue();
};

#endif
