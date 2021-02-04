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

#include "rwqueue.h"

#include <cassert>

template <typename T>
RWQueue<T>::RWQueue(size_t queue_capacity) : capacity(queue_capacity)
{
	assert(capacity > 0);
}

template <typename T>
size_t RWQueue<T>::Size()
{
	std::lock_guard<std::mutex> lock(mutex);
	return queue.size();
}

template <typename T>
size_t RWQueue<T>::MaxCapacity() const
{
	return capacity;
}

template <typename T>
bool RWQueue<T>::IsEmpty()
{
	std::lock_guard<std::mutex> lock(mutex);
	return !queue.size();
}

template <typename T>
void RWQueue<T>::Enqueue(const T &item)
{
	// wait until the queue has room to accept the item
	std::unique_lock<std::mutex> lock(mutex);
	while (queue.size() >= capacity)
		has_room.wait(lock);

	// add it, and notify the next waiting thread that we've got an item
	queue.emplace(queue.end(), item);
	lock.unlock();
	has_items.notify_one();
}

template <typename T>
void RWQueue<T>::Enqueue(T &&item)
{
	// wait until the queue has room to accept the item
	std::unique_lock<std::mutex> lock(mutex);
	while (queue.size() >= capacity)
		has_room.wait(lock);

	// add it, and notify the next waiting thread that we've got an item
	queue.emplace(queue.end(), std::move(item));
	lock.unlock();
	has_items.notify_one();
}

template <typename T>
T RWQueue<T>::Dequeue()
{
	// wait until the queue has an item that we can get
	std::unique_lock<std::mutex> lock(mutex);
	while (!queue.size())
		has_items.wait(lock);

	// get it, and notify the first waiting thread that the queue has room
	T item = std::move(queue.front());
	queue.pop_front();
	lock.unlock();
	has_room.notify_one();
	return item;
}

// Explicit template instantiations
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#include <vector>
template class RWQueue<int>; // Unit tests
template class RWQueue<std::vector<int16_t>>; // MT-32 and FluidSynth
