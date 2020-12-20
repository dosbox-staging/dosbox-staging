/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  The dosbox-staging team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"
#include "fifo.h"
#include "logging.h"

#include <cassert>

Fifo::Fifo(const size_t n) : q()
{
	setSize(n);
}

uint8_t Fifo::back() const noexcept
{
	return numQueued() ? q.back() : 0;
}

void Fifo::clear() noexcept
{
	q = std::queue<uint8_t>();
	overflow_tally = 0;
}

bool Fifo::isEmpty() const noexcept
{
	return q.empty();
}

bool Fifo::isFull() const noexcept
{
	return numQueued() >= slots;
}

bool Fifo::isUsed() const noexcept
{
	return !q.empty();
}

uint8_t Fifo::front() const noexcept
{
	return isEmpty() ? 0 : q.front();
}

size_t Fifo::numFreeSlots() const noexcept
{
	return slots - numQueued();
}

size_t Fifo::numQueued() const noexcept
{
	return q.size();
}

uint8_t Fifo::pop() noexcept
{
	if (isEmpty())
		return 0;
	const auto val = q.front();
	q.pop();
	return val;
}

bool Fifo::push(const uint8_t val) noexcept
{
	if (numQueued() < slots) {
		q.push(val);
		return true;
	}

	// Check and report overflow
	++overflow_tally;
	if (overflow_tally < 1000)
		LOG_MSG("FIFO: Overflow adding to the queue");
	return false;
}

void Fifo::pushMany(const uint8_t *str, size_t len) noexcept
{
	// Check and report overflow
	if (len > numFreeSlots()) {
		++overflow_tally;
		if (overflow_tally < 1000) {
			LOG_MSG("FIFO: Overflow adding %lu bytes to the queue", len);
		}
		// but accept what we can because FIFOs operated one byte at a time
		len = numFreeSlots();
	}
	size_t i = 0;
	while (len--)
		q.push(str[i++]);
	assert(numQueued() <= slots);
	return;
}

void Fifo::setSize(const size_t n) noexcept
{
	constexpr uint16_t slot_limit = 1024;
	if (n <= slot_limit) {
		slots = n;
	} else {
		LOG_MSG("FIFO: Limiting request for %lu-byte FIFO to %u bytes",
		        n, slot_limit);
		slots = slot_limit;
	}
	clear();
}
