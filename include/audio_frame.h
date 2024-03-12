/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

	constexpr AudioFrame(const int16_t l, const int16_t r)
	        : left(l),
	          right(r)
	{}

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

	constexpr bool operator==(const AudioFrame& that) const
	{
		return (left == that.left && right == that.right);
	}
};

#endif
