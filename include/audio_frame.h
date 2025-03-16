/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_AUDIO_FRAME_H
#define DOSBOX_AUDIO_FRAME_H

#include <cassert>
#include <cstddef>

// A simple stereo audio frame
struct AudioFrame {
	float left  = 0.0f;
	float right = 0.0f;

	constexpr AudioFrame() = default;

	constexpr AudioFrame(const float l, const float r)
	        : left(l),
	          right(r)
	{}

	constexpr AudioFrame(const float m)
	        : left(m),
	          right(m)
	{}

	constexpr AudioFrame(const int16_t l, const int16_t r)
	        : left(l),
	          right(r)
	{}

	constexpr AudioFrame(const int16_t m)
	        : left(m),
	          right(m)
	{}

	// Indexing
	constexpr float& operator[](const size_t i) noexcept
	{
		assert(i < 2);
		return i == 0 ? left : right;
	}
	constexpr const float& operator[](const size_t i) const noexcept
	{
		assert(i < 2);
		return i == 0 ? left : right;
	}

	// Equality
	constexpr bool operator==(const AudioFrame& that) const
	{
		return (left == that.left && right == that.right);
	}

	// Accumulation
	constexpr AudioFrame& operator+=(const AudioFrame& that)
	{
		*this = *this + that;
		return *this;
	}

	constexpr AudioFrame operator+(const AudioFrame& that) const
	{
		return {left + that.left, right + that.right};
	}

	// Difference
	constexpr AudioFrame& operator-=(const AudioFrame& that)
	{
		*this = *this - that;
		return *this;
	}

	constexpr AudioFrame operator-(const AudioFrame& that) const
	{
		return {left - that.left, right - that.right};
	}

	// Gain
	constexpr AudioFrame& operator*=(const float gain)
	{
		*this = *this * gain;
		return *this;
	}

	constexpr AudioFrame operator*(const float gain) const
	{
		return {left * gain, right * gain};
	}

	constexpr AudioFrame& operator/=(const float atten)
	{
		*this = *this / atten;
		return *this;
	}

	constexpr AudioFrame operator/(const float atten) const
	{
		return {left / atten, right / atten};
	}

	constexpr AudioFrame& operator*=(const AudioFrame& gain)
	{
		*this = *this * gain;
		return *this;
	}

	constexpr AudioFrame operator*(const AudioFrame& gain) const
	{
		return {left * gain.left, right * gain.right};
	}

	constexpr AudioFrame& operator/=(const AudioFrame& atten)
	{
		*this = *this / atten;
		return *this;
	}

	constexpr AudioFrame operator/(const AudioFrame& atten) const
	{
		return {left * atten.left, right * atten.right};
	}
};

#endif
