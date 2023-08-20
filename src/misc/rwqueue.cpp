/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2023  The DOSBox Staging Team
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

#include "../capture/image/image_saver.h"

#include <cassert>

template <typename T>
RWQueue<T>::RWQueue(size_t queue_capacity)
{
	Resize(queue_capacity);
}

template <typename T>
void RWQueue<T>::Resize(size_t queue_capacity)
{
	std::lock_guard<std::mutex> lock(mutex);
	capacity = queue_capacity;
	assert(capacity > 0);
}

template <typename T>
size_t RWQueue<T>::Size()
{
	std::lock_guard<std::mutex> lock(mutex);
	return queue.size();
}

template <typename T>
void RWQueue<T>::Stop()
{
	if (!is_running) {
		return;
	}
	mutex.lock();
	is_running = false;
	mutex.unlock();

	// notify the conditions
	has_items.notify_all();
	has_room.notify_all();
}

template <typename T>
size_t RWQueue<T>::MaxCapacity() const
{
	return capacity;
}

template <typename T>
float RWQueue<T>::GetPercentFull()
{
	const auto cur_level = static_cast<float>(Size());
	const auto max_level = static_cast<float>(capacity);
	return (100.0f * cur_level) / max_level;
}

template <typename T>
bool RWQueue<T>::IsEmpty()
{
	std::lock_guard<std::mutex> lock(mutex);
	return queue.empty();
}

template <typename T>
bool RWQueue<T>::IsRunning() const
{
	return is_running;
}

template <typename T>
bool RWQueue<T>::Enqueue(T&& item)
{
	// wait until we're stopped or the queue has room to accept the item
	std::unique_lock<std::mutex> lock(mutex);
	has_room.wait(lock,
	              [this] { return !is_running || queue.size() < capacity; });

	// add it, and notify the next waiting thread that we've got an item
	if (is_running) {
		queue.emplace(queue.end(), std::move(item));
	}
	// If we stopped while enqueing, then anything that was enqueued prior
	// to being stopped is safely in the queue.

	lock.unlock();
	has_items.notify_one();
	return is_running;
}

// In both bulk methods, the best case scenario is if the queue can absorb or
// fill the entire request in one pass.

// The worst-case is if the queue is full (when the user wants to enqueue) or is
// empty (when the user wants to dequeue). In this case, the calculations at a
// minimum need to request at least one element to keep blocking until we have
// room for just one item to avoid spinning with a zero count (which burns CPU).

template <typename T>
bool RWQueue<T>::BulkEnqueue(std::vector<T>& from_source, const size_t num_requested)
{
	constexpr size_t min_items = 1;
	assert(num_requested >= min_items);
	assert(num_requested <= from_source.size());

	auto source_start  = from_source.begin();
	auto num_remaining = num_requested;

	while (num_remaining > 0) {
		std::unique_lock<std::mutex> lock(mutex);

		const auto free_capacity = static_cast<size_t>(capacity -
		                                               queue.size());

		const auto num_items = std::max(min_items,
		                                std::min(num_remaining,
		                                         free_capacity));

		// wait until we're stopped or the queue has enough room for the
		// items
		has_room.wait(lock, [&] {
			return !is_running || capacity - queue.size() >= num_items;
		});

		if (is_running) {
			const auto source_end = source_start +
			                        static_cast<difference_t>(num_items);
			queue.insert(queue.end(),
			             std::move_iterator(source_start),
			             std::move_iterator(source_end));
			source_start = source_end;
			num_remaining -= num_items;
		} else {
			// If we stopped while bulk enqueing, then stop here.
			// Anything that was enqueued prior to being stopped is
			// safely in the queue.
			num_remaining = 0;
		}

		// notify the next waiting thread that we have an item
		lock.unlock();
		has_items.notify_one();
	}
	from_source.clear();
	return is_running;
}

template <typename T>
std::optional<T> RWQueue<T>::Dequeue()
{
	// wait until we're stopped or the queue has an item
	std::unique_lock<std::mutex> lock(mutex);
	has_items.wait(lock, [this] { return !is_running || !queue.empty(); });

	auto optional_item = std::optional<T>();

	// Even if the queue has stopped, we need to drain the (previously)
	// queued items before we're done.
	if (is_running || !queue.empty()) {
		optional_item = std::move(queue.front());
		queue.pop_front();
	}
	lock.unlock();

	// notify the first waiting thread that the queue has room
	has_room.notify_one();
	return optional_item;
}

template <typename T>
bool RWQueue<T>::BulkDequeue(std::vector<T>& into_target, const size_t num_requested)
{
	constexpr size_t min_items = 1;
	assert(num_requested >= min_items);

	if (into_target.size() != num_requested) {
		into_target.resize(num_requested);
	}

	auto target_start  = into_target.begin();
	auto num_remaining = num_requested;

	while (num_remaining > 0) {
		std::unique_lock<std::mutex> lock(mutex);

		const auto num_items = std::max(min_items,
		                                std::min(num_remaining,
		                                         queue.size()));

		// wait until we're stopped or the queue has enough items
		has_items.wait(lock, [&] {
			return !is_running || queue.size() >= num_items;
		});

		// Even if the queue has stopped, we need to drain the
		// (previously) queued items before we're done.
		if (is_running || !queue.empty()) {
			const auto queue_end = queue.begin() +
			                       static_cast<difference_t>(num_items);

			std::move(queue.begin(), queue_end, target_start);
			queue.erase(queue.begin(), queue_end);

			target_start += static_cast<difference_t>(num_items);
			num_remaining -= num_items;

		} else {
			// If we stopped while dequeing, cap off the target
			// vector based on the subset that were dequeued.
			assert(num_remaining <= num_requested);
			into_target.resize(num_requested - num_remaining);
			num_remaining = 0;
		}

		// notify the first waiting thread that the queue now has room
		lock.unlock();
		has_room.notify_one();
	}
	return !into_target.empty();
}

// Explicit template instantiations
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#include <vector>
// Unit tests
template class RWQueue<int>;
template class RWQueue<std::vector<int16_t>>;

// FluidSynth and MT-32
#include "audio_frame.h"
template class RWQueue<AudioFrame>;

#include "midi.h"
template class RWQueue<MidiWork>;

#include "render.h"
template class RWQueue<SaveImageTask>;
