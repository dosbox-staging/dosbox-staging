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

#include "dosbox.h"

#include <atomic>
#include <vector>
#include <string>

#include "mixer.h"

/*
Zero-Latency Soft Limiter
-------------------------
Given an input vector of floats, the Soft Limiter scales sequences that
exceed the bounds of a signed 16-bit signal.

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
   perform seamless scaling both on the front and back-end of the signal

 - Permits a pre-scaling factor be applied to the input samples
   before peak detection and scaling (see UpdateLevels).

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

Instantiate the Soft Limiter object with the name of the channel that's
being operated on, for example:
  SoftLimiter limiter("channel name");

You can then repeatedly call:
  limiter.Process(in, num_frames, out);

Where:
 - 'in' is a std::vector<float>
 - 'num_frames' is some number of frames from zero up the maximum
   previously passed into the Soft Limiter's constructor
 - 'out' is a a std::vector<int16_t> of samples (num_frames * 2).

The limiter will either copy or soft-limit num_frames into the out vector.

The PrintStats function will indicate the peak amplitude as a percent
of the allow maximum detected during the entire processing period. It
will also provide mixer-level suggestions if the peak samples was
significantly below the maximum or if a significant percent of the overall
sequence required limiting.
*/

class SoftLimiter {
public:
	using in_iterator_t = typename std::vector<float>::const_iterator;

	// Prevent default object construction, copy, and assignment
	SoftLimiter() = delete;
	SoftLimiter(const SoftLimiter &) = delete;
	SoftLimiter &operator=(const SoftLimiter &) = delete;

	SoftLimiter(const std::string &name);

	void Process(const std::vector<float> &in,
	             uint16_t req_frames,
	             std::vector<int16_t> &out) noexcept;
	const AudioFrame &GetPeaks() const noexcept { return global_peaks; }
	void PrintStats() const;
	void Reset() noexcept;
	void UpdateLevels(const AudioFrame &desired_levels, float desired_multiplier);

private:
	using out_iterator_t = typename std::vector<int16_t>::iterator;

	void FindPeaksAndZeroCrosses(const std::vector<float> &in,
	                             in_iterator_t &precross_peak_pos_left,
	                             in_iterator_t &precross_peak_pos_right,
	                             in_iterator_t &zero_cross_left,
	                             in_iterator_t &zero_cross_right,
	                             uint16_t samples) noexcept;

	void LinearScale(in_iterator_t in_pos,
	                 in_iterator_t in_end,
	                 out_iterator_t out_pos,
	                 float scalar) const noexcept;

	void PolyFit(in_iterator_t in_pos,
	             in_iterator_t in_end,
	             out_iterator_t out_pos,
	             float prescalar,
	             float poly_a,
	             float poly_b) const noexcept;

	void Release() noexcept;

	void SaveTailFrame(uint16_t req_frames,
	                   const std::vector<int16_t> &out) noexcept;

	template <int8_t channel>
	void ScaleOrCopy(const std::vector<float> &in,
	                 uint16_t samples,
	                 float prescalar,
	                 in_iterator_t precross_peak_pos,
	                 in_iterator_t zero_cross_pos,
	                 float global_peak,
	                 float tail,
	                 std::vector<int16_t> &out);

	// Mutable members
	std::string channel_name = {};
	std::atomic<AudioFrame> prescale = {};
	AudioFrame global_peaks = {0, 0};
	AudioFrame tail_frame = {0, 0};
	float range_multiplier = 1.0f;
	int limited_tally = 0;
	int non_limited_tally = 0;
};

#endif
