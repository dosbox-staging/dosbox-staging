// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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

	constexpr AudioFrame& operator*=(const AudioFrame& gain)
	{
		*this = *this * gain;
		return *this;
	}

	constexpr AudioFrame operator*(const AudioFrame& gain) const
	{
		return {left * gain.left, right * gain.right};
	}
};

#endif
