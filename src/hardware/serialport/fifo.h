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

#ifndef DOSBOX_HARDWARE_SERIALPORT_FIFO_H
#define DOSBOX_HARDWARE_SERIALPORT_FIFO_H

#include <cstdint>
#include <queue>

class Fifo {
public:
	Fifo(const size_t n);
	uint8_t back() const noexcept;
	void clear() noexcept;
	uint8_t front() const noexcept;
	bool isEmpty() const noexcept;
	bool isFull() const noexcept;
	bool isUsed() const noexcept;
	size_t numFreeSlots() const noexcept;
	size_t numQueued() const noexcept;
	uint8_t pop() noexcept;
	bool push(const uint8_t val) noexcept;
	void pushMany(const uint8_t *str, size_t len) noexcept;
	void setSize(const size_t n) noexcept;

private:
	std::queue<uint8_t> q;
	size_t slots = 0;
	uint16_t overflow_tally = 0;
};

#endif

