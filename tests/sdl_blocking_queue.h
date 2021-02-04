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

#include <algorithm>
#include <deque>
#include <mutex>
#include <SDL_thread.h>

template <typename T>
class blocking_queue {
private:
	std::deque<T> queue{}; // fastest vs vector, queue, and list
	SDL_mutex *mutex = nullptr;
	SDL_cond *not_full = nullptr;
	SDL_cond *not_empty = nullptr;
	size_t capacity = 0;

public:
	blocking_queue(const blocking_queue<T> &other) = delete;
	blocking_queue<T> &operator=(const blocking_queue<T> &other) = delete;

	blocking_queue(size_t queue_capacity) : capacity(queue_capacity)
	{
		mutex = SDL_CreateMutex();
		not_full = SDL_CreateCond();
		not_empty = SDL_CreateCond();
	}

	~blocking_queue()
	{
		SDL_DestroyMutex(mutex);
		mutex = nullptr;

		SDL_DestroyCond(not_full);
		not_full = nullptr;

		SDL_DestroyCond(not_empty);
		not_empty = nullptr;
	}

	size_t size()
	{
		SDL_LockMutex(mutex);

		const size_t q_size = queue.size();

		SDL_UnlockMutex(mutex);
		return q_size;
	}

	bool empty()
	{
		SDL_LockMutex(mutex);

		const bool is_empty = queue.empty();

		SDL_UnlockMutex(mutex);
		return is_empty;
	}

	void push(const T &elem)
	{
		SDL_LockMutex(mutex);

		// wait while the queue is full
		while (queue.size() >= capacity) {
			SDL_CondWait(not_full, mutex);
		}
		queue.push_back(elem);

		SDL_UnlockMutex(mutex);
		SDL_CondSignal(not_empty);
	}

	void pop()
	{
		SDL_LockMutex(mutex);

		// wait while the queue is empty
		while (queue.size() == 0) {
			SDL_CondWait(not_empty, mutex);
		}
		queue.pop_front();

		SDL_UnlockMutex(mutex);
		SDL_CondSignal(not_full);
	}

	const T &front()
	{
		SDL_LockMutex(mutex);

		// wait while the queue is empty
		while (queue.size() == 0) {
			SDL_CondWait(not_empty, mutex);
		}
		const T &item = queue.front();

		SDL_UnlockMutex(mutex);
		return item;
	}
};
