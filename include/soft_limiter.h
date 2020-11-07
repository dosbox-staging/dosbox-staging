/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  Kevin R Croft <krcroft@gmail.com>
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

#ifndef DOSBOX_SOFT_LIMITER_H
#define DOSBOX_SOFT_LIMITER_H

/*
Zero-Latency Soft Limiter
-------------------------
Given an input array of floats, the Soft Limiter scales sequences that
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
  const auto out_array = limiter.Apply(in_array, num_frames);

Where:
 - in_array is a std::array<float, 48>
 - num_frames is some number of frames from zero up to 48.
 - out_array is a const reference to std::array<int16_t, 48>

The limiter will either copy or scale-limit num_frames into the out_array.

The PrintStats function will make mixer suggestions if the in_array samples were
either significantly under or over the allowed bounds for the entire playback
duration.
*/

#include "dosbox.h"
#include "logging.h"
#include "mixer.h"
#include "support.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <cmath>
#include <string>

template <size_t array_frames>
class SoftLimiter {
public:
	SoftLimiter(const std::string &name, const AudioFrame &scale);

	// Prevent default object construction, copy, and assignment
	SoftLimiter() = delete;
	SoftLimiter(const SoftLimiter<array_frames> &) = delete;
	SoftLimiter<array_frames> &operator=(const SoftLimiter<array_frames> &) = delete;

	constexpr static size_t array_samples = array_frames * 2; // 2 for stereo
	using in_array_t = std::array<float, array_samples>;
	using out_array_t = std::array<int16_t, array_samples>;

	const out_array_t &Apply(const in_array_t &in, uint16_t frames) noexcept;
	const AudioFrame &GetPeaks() const noexcept { return global_peaks; }
	void PrintStats() const;
	void Reset() noexcept;

private:
	using in_array_iterator_t = typename std::array<float, array_samples>::const_iterator;
	using out_array_iterator_t = typename std::array<int16_t, array_samples>::iterator;
	using out_limits = std::numeric_limits<int16_t>;

	void FindPeaksAndZeroCrosses(const in_array_t &stream, uint16_t frames) noexcept;

	void FindPeakAndCross(const in_array_iterator_t in_end,
	                      const in_array_iterator_t pos,
	                      in_array_iterator_t &prev_pos,
	                      const float prescalar,
	                      float &local_peak,
	                      in_array_iterator_t &precross_peak_pos,
	                      in_array_iterator_t &zero_cross_pos,
	                      float &global_peak) noexcept;

	void LinearScale(in_array_iterator_t in_pos,
	                 const in_array_iterator_t in_end,
	                 out_array_iterator_t out_pos,
	                 const float scalar) noexcept;

	void PolyFit(in_array_iterator_t in_pos,
	             const in_array_iterator_t in_end,
	             out_array_iterator_t out_pos,
	             const float prescalar,
	             const float poly_a,
	             const float poly_b) noexcept;

	void Release() noexcept;

	void SaveTailFrame(const uint16_t frames) noexcept;

	template <size_t channel>
	void ScaleOrCopy(const in_array_t &in,
	                 const size_t samples,
	                 const float prescalar,
	                 const in_array_iterator_t precross_peak_pos,
	                 const in_array_iterator_t zero_cross_pos,
	                 const float global_peak,
	                 const float tail);

	// Const members
	constexpr static size_t left = 0;
	constexpr static size_t right = 1;
	constexpr static float bounds = static_cast<float>(out_limits::max() - 1);

	// Mutable members
	out_array_t out{};
	std::string channel_name = {};
	const AudioFrame &prescale; // values inside struct are mutable
	in_array_iterator_t zero_cross_left = {};
	in_array_iterator_t zero_cross_right = {};
	in_array_iterator_t precross_peak_pos_left = {};
	in_array_iterator_t precross_peak_pos_right = {};
	AudioFrame global_peaks = {0, 0};
	AudioFrame tail_frame = {0, 0};
	int limited_ms = 0;
	int non_limited_ms = 0;
};

template <size_t array_frames>
SoftLimiter<array_frames>::SoftLimiter(const std::string &name, const AudioFrame &scale)
        : channel_name(name),
          prescale(scale)
{
	static_assert(array_frames > 0, "need some quantity of frames");
	static_assert(array_frames < 16385, "consider using a smaller sequence");
}

// Applies the Soft Limiter to the given input sequence and returns the results
// as a reference to a std::array of 16-bit ints the same length as the input.
template <size_t array_frames>
const typename SoftLimiter<array_frames>::out_array_t &SoftLimiter<array_frames>::Apply(
        const in_array_t &in, const uint16_t frames) noexcept
{
	// Ensure the buffers are large enough to handle the request
	const uint16_t samples = frames * 2; // left and right channels
	assert(samples <= in.size());

	FindPeaksAndZeroCrosses(in, samples);

	// Given the local peaks found in each side channel, scale or copy the
	// input array into the output array
	ScaleOrCopy<left>(in, samples, prescale.left, precross_peak_pos_left,
	                  zero_cross_left, global_peaks.left, tail_frame.left);

	ScaleOrCopy<right>(in, samples, prescale.right, precross_peak_pos_right,
	                   zero_cross_right, global_peaks.right, tail_frame.right);

	SaveTailFrame(frames);
	Release();
	return out;
}

// Helper function to evaluate the existing peaks and prior values.
// Saves new local and global peaks, and the input-array iterator of any new
// peaks before the first zero-crossing, along with the first zero-crossing
// position.
template <size_t array_frames>
void SoftLimiter<array_frames>::FindPeakAndCross(const in_array_iterator_t in_end,
                                                 const in_array_iterator_t pos,
                                                 in_array_iterator_t &prev_pos,
                                                 const float prescalar,
                                                 float &local_peak,
                                                 in_array_iterator_t &precross_peak_pos,
                                                 in_array_iterator_t &zero_cross_pos,
                                                 float &global_peak) noexcept
{
	const auto val = fabsf(*pos) * prescalar;
	if (val > bounds && val > local_peak) {
		local_peak = val;
		if (zero_cross_pos == in_end) {
			precross_peak_pos = pos;
		}
	}
	if (val > global_peak) {
		global_peak = val;
	}
	// Detect and save the first zero-crossing position (if any)
	if (zero_cross_pos == in_end && prev_pos != in_end &&
	    std::signbit(*prev_pos) != std::signbit(*pos)) {
		zero_cross_pos = pos;
	}
	prev_pos = pos;
}

// Sequentially scans the input channels to find new peaks, their positions, and
// the first zero crossings (saved in member variables).
template <size_t array_frames>
void SoftLimiter<array_frames>::FindPeaksAndZeroCrosses(const in_array_t &in,
                                                        const uint16_t samples) noexcept
{
	auto pos = in.begin();
	const auto pos_end = in.begin() + samples;

	precross_peak_pos_left = in.end();
	precross_peak_pos_right = in.end();
	zero_cross_left = in.end();
	zero_cross_right = in.end();
	in_array_iterator_t prev_pos_left = in.end();
	in_array_iterator_t prev_pos_right = in.end();
	AudioFrame local_peaks = global_peaks;

	while (pos != pos_end) {
		FindPeakAndCross(in.end(), pos++, prev_pos_left, prescale.left,
		                 local_peaks.left, precross_peak_pos_left,
		                 zero_cross_left, global_peaks.left);

		FindPeakAndCross(in.end(), pos++, prev_pos_right, prescale.right,
		                 local_peaks.right, precross_peak_pos_right,
		                 zero_cross_right, global_peaks.right);
	}
}

// Scale or copy the given channel's samples into the output array
template <size_t array_frames>
template <size_t channel>
void SoftLimiter<array_frames>::ScaleOrCopy(const in_array_t &in,
                                            const size_t samples,
                                            const float prescalar,
                                            const in_array_iterator_t precross_peak_pos,
                                            const in_array_iterator_t zero_cross_pos,
                                            const float global_peak,
                                            const float tail)
{
	assert(samples >= 2); // need at least one frame
	auto in_start = in.begin() + channel;
	const auto in_end = in.begin() + channel + samples;
	auto out_start = out.begin() + channel;

	// We have a new peak, so ...
	if (precross_peak_pos != in.end()) {
		const auto tail_abs = fabsf(tail);
		const auto prepeak_scalar = (bounds - tail_abs) /
		                            (prescalar * fabsf(*precross_peak_pos) -
		                             tail_abs);
		// fit the frontside of the waveform to the tail up to the peak.
		PolyFit(in_start, precross_peak_pos, out_start, prescalar,
		        prepeak_scalar, tail);

		// Then scale the backend of the waveform from its peak ...
		out_start = out.begin() + (precross_peak_pos - in.begin());
		const auto postpeak_scalar = bounds / fabsf(*precross_peak_pos);
		// down to the zero-crossing ...
		if (zero_cross_pos != in.end()) {
			LinearScale(precross_peak_pos, zero_cross_pos,
			            out_start, postpeak_scalar);

			// and from the zero-crossing to the end of the sequence.
			out_start = out.begin() + (zero_cross_pos - in.begin());
			const auto postcross_scalar = prescalar * bounds / global_peak;
			LinearScale(zero_cross_pos, in_end, out_start,
			            postcross_scalar);
		}
		// down to the end of the sequence.
		else {
			LinearScale(precross_peak_pos, in_end, out_start,
			            postpeak_scalar);
		}
		limited_ms++;

		// We have an existing peak ...
	} else if (global_peak > bounds) {
		// so scale the entire sequence a a ratio of the peak.
		const auto current_scalar = prescalar * bounds / global_peak;
		LinearScale(in_start, in_end, out_start, current_scalar);
		limited_ms++;

		// The current sequence is fully inbounds ...
	} else {
		// so simply prescale the entire sequence.
		LinearScale(in_start, in_end, out_start, prescalar);
		++non_limited_ms;
	}
}

// Apply the polynomial coefficients to the sequence
template <size_t array_frames>
void SoftLimiter<array_frames>::PolyFit(in_array_iterator_t in_pos,
                                        const in_array_iterator_t in_end,
                                        out_array_iterator_t out_pos,
                                        const float prescalar,
                                        const float poly_a,
                                        const float poly_b) noexcept
{
	while (in_pos != in_end) {
		const auto fitted = poly_a * (*in_pos * prescalar - poly_b) + poly_b;
		assert(fabsf(fitted) <= out_limits::max());
		*out_pos = static_cast<int16_t>(fitted);
		out_pos += 2;
		in_pos += 2;
	}
}

// Apply the scalar to the sequence
template <size_t array_frames>
void SoftLimiter<array_frames>::LinearScale(in_array_iterator_t in_pos,
                                            const in_array_iterator_t in_end,
                                            out_array_iterator_t out_pos,
                                            const float scalar) noexcept
{
	while (in_pos != in_end) {
		const auto scaled = (*in_pos) * scalar;
		assert(fabsf(scaled) <= out_limits::max());
		*out_pos = static_cast<int16_t>(scaled);
		out_pos += 2;
		in_pos += 2;
	}
}

template <size_t array_frames>
void SoftLimiter<array_frames>::SaveTailFrame(const uint16_t frames) noexcept
{
	const size_t i = (frames - 1) * 2;
	tail_frame.left = static_cast<float>(out[i]);
	tail_frame.right = static_cast<float>(out[i + 1]);
}

// If either channel was out of bounds, then decrement their peak
template <size_t array_frames>
void SoftLimiter<array_frames>::Release() noexcept
{
	// Decrement the peak(s) one step
	constexpr float delta_db = 0.002709201f; // 0.0235 dB increments
	constexpr float release_amplitude = bounds * delta_db;
	if (global_peaks.left > bounds)
		global_peaks.left -= release_amplitude;
	if (global_peaks.right > bounds)
		global_peaks.right -= release_amplitude;
}

// Print helpful statistics about the signal thus-far
template <size_t array_frames>
void SoftLimiter<array_frames>::PrintStats() const
{
	// Only print information if we have more than 30 seconds of data
	constexpr auto ms_per_minute = 1000 * 60;
	const auto ms_total = static_cast<double>(limited_ms) + non_limited_ms;
	const auto minutes_total = ms_total / ms_per_minute;
	if (minutes_total < 0.5)
		return;

	// Only print information if there was at least some amplitude
	const auto peak_sample = std::max(global_peaks.left, global_peaks.right);
	constexpr auto two_percent_of_max = 0.02f * bounds;
	if (peak_sample < two_percent_of_max)
		return;

	// Inform the user what percent of the dynamic-range was used
	auto peak_ratio = peak_sample / bounds;
	peak_ratio = std::min(peak_ratio, 1.0f);
	LOG_MSG("%s: Peak amplitude reached %.0f%% of max",
	        channel_name.c_str(), 100 * static_cast<double>(peak_ratio));

	// Inform when the stream fell short of using the full dynamic-range
	const auto scale = std::max(prescale.left, prescale.right);
	constexpr auto well_below_3db = 0.6f;
	if (peak_ratio < well_below_3db) {
		const auto suggested_mix_val = 100 * scale / peak_ratio;
		LOG_MSG("%s: If it should be louder, use: mixer %s %.0f",
		        channel_name.c_str(), channel_name.c_str(),
		        static_cast<double>(suggested_mix_val));
	}
	// Inform if more than 20% of the stream required limiting
	const auto time_ratio = limited_ms / (ms_total + 1); // one ms avoids div-by-0
	if (time_ratio > 0.2) {
		const auto minutes_limited = static_cast<double>(limited_ms) / ms_per_minute;
		const auto suggested_mix_val = 100 * (1 - time_ratio) *
		                               static_cast<double>(scale);
		LOG_MSG("%s: %.1f%% or %.2f of %.2f minutes needed limiting, consider: mixer %s %.0f",
		        channel_name.c_str(), 100 * time_ratio, minutes_limited,
		        minutes_total, channel_name.c_str(), suggested_mix_val);
	}
}

// A paused audio source should Reset() the limiter so it starts with
// fresh peaks and a zero-tail if/when the stream is restarted.
template <size_t array_frames>
void SoftLimiter<array_frames>::Reset() noexcept
{
	// if the current peaks are over the upper bounds, then we simply save
	// the upper bound because we want retain information about the peak
	// amplitude when printing statistics.

	constexpr auto upper = bounds; // (workaround to prevent link-errors)
	global_peaks.left = std::min(global_peaks.left, upper);
	global_peaks.right = std::min(global_peaks.right, upper);
	tail_frame = {0, 0};
}

#endif
