/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_RING_BUFFER_H
#define DOSBOX_RING_BUFFER_H

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

#include "math_utils.h"

// Simple ring buffer implementation that basically wraps `std:array` and adds
// iterators with "wrap-around" properties.
//
// Enforces power of two array sizes by default for efficiency reasons.
//
// Slightly adapted from:
// https://gist.github.com/jhurliman/58b9ee8f52053a0e3dbbb45aad718457
//
#define POWER_OF_TWO_ARRAY_SIZE 1

template <class T, size_t N>
class RingBuffer {

#if POWER_OF_TWO_ARRAY_SIZE
	static_assert(std::has_single_bit(N), "RingBuffer size must be power of two");

	static constexpr size_t IndexMask = (N - 1);
#endif

public:
	RingBuffer() = default;

	RingBuffer(const T initValue)
	{
		std::fill_n(data_.begin(), data_.size(), initValue);
	}

	T at(size_t n)
	{
		return data_.at(n);
	}

	std::array<T, N>& data() const
	{
		return data_;
	}

	size_t size()
	{
		return data_.size();
	}

	// TODO Add const_iterator support, then start using it in `mixer.cpp`
	class RingBufferIterator {
		RingBuffer* array;
		size_t index;

	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = T;
		using pointer = T*;
		using reference = T&;

		RingBufferIterator(RingBuffer& d, size_t idx) : array(&d), index(idx) {}

		// Decrement operator
		RingBufferIterator& operator--()
		{
			PrevIndex();
			return *this;
		}

		RingBufferIterator operator--(int)
		{
			RingBufferIterator tmp(*array, index);
			PrevIndex();
			return tmp;
		}

		// Increment operator
		RingBufferIterator& operator++()
		{
			NextIndex();
			return *this;
		}

		RingBufferIterator operator++(int)
		{
			RingBufferIterator tmp(*array, index);
			NextIndex();
			return tmp;
		}

		// Addition (positive offset) operator
		RingBufferIterator operator+(size_t off)
		{
			auto idx = (index + off);
#if POWER_OF_TWO_ARRAY_SIZE
			idx &= IndexMask;
#else
			idx %= array->size();
#endif
			return RingBufferIterator(*array, idx);
		}

		RingBufferIterator operator+=(size_t off)
		{
			*this = *this + off;
			return *this;
		}

		// Subtraction (negative offset) operator
		RingBufferIterator operator-(size_t off)
		{
			auto idx = index - off + array->size();
#if POWER_OF_TWO_ARRAY_SIZE
			idx &= IndexMask;
#else
			idx %= array->size();
#endif
			return RingBufferIterator(*array, idx);
		}

		RingBufferIterator operator-=(size_t off)
		{
			*this = *this - off;
			return *this;
		}

		// Dereference operator
		T& operator*() const
		{
			return (*array).data_[index];
		}

		// Equality operator
		bool operator==(const RingBufferIterator& other) const
		{
			return index == other.index;
		}

	private:
		void PrevIndex()
		{
#if POWER_OF_TWO_ARRAY_SIZE
			--index;
			index &= IndexMask;
#else
			if (index == 0) {
				index = array->size() - 1;
			} else {
				--index;
			}
#endif
		}

		void NextIndex()
		{
#if POWER_OF_TWO_ARRAY_SIZE
			++index;
			index &= IndexMask;
#else
			++index;
			if (index == array->size()) {
				index = 0;
			}
#endif
		}
	};

	RingBufferIterator begin()
	{
		return RingBufferIterator(*this, 0);
	}

	std::array<T, N> data_ = {};
};

#endif // DOSBOX_RING_BUFFER_H
