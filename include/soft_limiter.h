/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  Kevin R. Croft <krcroft@gmail.com>
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_SOFT_LIMITER_H
#define DOSBOX_SOFT_LIMITER_H

#include <algorithm>
#include <cinttypes>
#include <limits>
#include <vector>
#include <string>

#include "mixer.h"

/*
Zero-Latency Soft Limiter
-------------------------
Given an input vector of floats, the Soft Limiter scales sequences that
exceed the bounds of a standard 16-bit signal.

This scale-down effect continues to be applied to subsequent sequences,
each time with less effect (provided even greater peaks aren't detected),
until the scale-down is complete - this period is known as 'Release' and
prevents a discontinuous jump in subsequent waveforms after the initial
sequence is scaled down.

Likewise, when a new large peak is detected, a polynomial is used to join
the tail of the prior sequence with the head of the current sequence.

Unique features:
 - Left and right channels are independently handled

 - Zero-latency: it does not require a pre-buffer or aprior knowledge to
   perform seamless scaling both on the front-end and back-end of the signal

 - Permits a pre-scaling factor be applied to the 32-bit float source samples
   before detection and scaling (performed on-the-fly without pre-pass).

 - Informs the user if the source signal was significantly under the allowed
   bounds in which case it suggests a suitable scale-up factor, or if excessive
   scaling was required it suggests a suitable scale-down factor.

Regarding the release duration:

Because audio should only be adjusted in small amounts to prevent
discontinuities, the release duration is a function of the magnitude of the
scale-down factor. The larger the scale-down, the more release periods are
needed; typically ranging from 10's of milliseconds to low-hundreds for large >
2x overages.

Use:

Instantiate the Soft Limiter template object with the maximum number of frames
you plan to pass it. Construct it with the name of the channel that's being
operated on and prescalar frames (one frame = left and right samples) in a given
sequence of audio. For example:

  AudioFrame prescale = {1.0f, 1.0f};
  SoftLimiter<48> limiter("channel name", prescale);

You can then repeatedly call:
  auto &&out = limiter.Apply(in, num_frames);

Where:
 - in is a std::vector<float>
 - num_frames is some number of frames from zero up in.size()
 - out is a a std::vector<int16_t> of samples (num_frames * 2).

The limiter will either copy or scale-limit num_frames into the out vector.

The PrintStats function will make mixer-level suggestions if the 'in' samples
were either significantly under or over the allowed bounds for the entire
playback duration.
*/

class SoftLimiter {
public:
	// Prevent default object construction, copy, and assignment
	SoftLimiter() = delete;
	SoftLimiter(const SoftLimiter &) = delete;
	SoftLimiter &operator=(const SoftLimiter &) = delete;

	SoftLimiter(const std::string &name,
	            const AudioFrame &scale,
	            const uint16_t max_frames);

	using in_t = std::vector<float>;
	using out_t = std::vector<int16_t>;
	out_t Apply(const in_t &in, uint16_t req_frames) noexcept;
	const AudioFrame &GetPeaks() const noexcept { return global_peaks; }
	void PrintStats() const;
	void Reset() noexcept;

private:
	using in_iterator_t = typename std::vector<float>::const_iterator;
	using out_iterator_t = typename std::vector<int16_t>::iterator;
	using out_limits = std::numeric_limits<int16_t>;

	void FindPeaksAndZeroCrosses(const in_t &stream, uint16_t req_samples) noexcept;

	void FindPeakAndCross(const in_iterator_t in_end,
	                      const in_iterator_t pos,
	                      in_iterator_t &prev_pos,
	                      const float prescalar,
	                      float &local_peak,
	                      in_iterator_t &precross_peak_pos,
	                      in_iterator_t &zero_cross_pos,
	                      float &global_peak) noexcept;

	void LinearScale(in_iterator_t in_pos,
	                 const in_iterator_t in_end,
	                 out_iterator_t out_pos,
	                 const float scalar) const noexcept;

	void PolyFit(in_iterator_t in_pos,
	             const in_iterator_t in_end,
	             out_iterator_t out_pos,
	             const float prescalar,
	             const float poly_a,
	             const float poly_b) const noexcept;

	void Release() noexcept;

	void SaveTailFrame(const uint16_t req_frames, const out_t &out) noexcept;

	template <size_t channel>
	void ScaleOrCopy(const in_t &in,
	                 const size_t samples,
	                 const float prescalar,
	                 const in_iterator_t precross_peak_pos,
	                 const in_iterator_t zero_cross_pos,
	                 const float global_peak,
	                 const float tail,
	                 out_t &out);

	// Const members
	constexpr static size_t left = 0;
	constexpr static size_t right = 1;
	constexpr static float bounds = static_cast<float>(out_limits::max() - 1);

	// Mutable members
	std::string channel_name = {};
	const AudioFrame &prescale; // values inside struct are mutable
	in_iterator_t zero_cross_left = {};
	in_iterator_t zero_cross_right = {};
	in_iterator_t precross_peak_pos_left = {};
	in_iterator_t precross_peak_pos_right = {};
	AudioFrame global_peaks = {0, 0};
	AudioFrame tail_frame = {0, 0};
	int limited_ms = 0;
	int non_limited_ms = 0;
	const uint16_t max_samples = 0;
};

#endif
