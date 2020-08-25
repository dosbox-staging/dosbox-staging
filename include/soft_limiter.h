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

#ifndef DOSBOX_SOFT_LIMITER_H
#define DOSBOX_SOFT_LIMITER_H

/*
Soft Limiter
------------
Given an input array and output array where the input can support wider values
than the output, the limiter will detect when the input contains one or more
values that would exceed the output's type bounds.

When detected, it scale-down the entire input set such that they fit within the
output bounds. By scaling the entire series of values, it ensure relative
differences are retained without distorion. (This is known as Soft Limiting,
which is superior to Hard Limiting that truncates or clips values and causes
distortion).

This scale-down-to-fit effect continues to be applied to subsequent input sets,
each time with less effect (provided new peaks aren't detected), until the
scale-down is complete - this period is known as 'Release'.

The release duration is a function of how much we needed to scale down in the
first place.  The larger the scale-down, the longer the release (typically
ranging from 10's of milliseconds to low-hundreds for > 2x overages).

Use:

The SoftLimiter reads and writes arrays of the same length, which is
defined during object creation as a template size. For example:

  SoftLimiter<48> limiter;

You can then repeatedly call:
  limiter.Apply(in_samples, out_samples);

Where in_samples is a std::array<float, 48> and out_samples is a 
std::array<int16_t, 48>. The limiter will either copy or limit the in_samples
into the out_samples array.

The PrintStats function will make mixer suggestions if the in_samples
were (at most) 40% under the allowed bounds, in which case the recommendation
will describe how to scale up the channel amplitude. 

On the other hand, if the limiter found itself scaling down more than 10% of
the inbound stream, then it will in turn recommend how to scale down the
channel. 
*/

#include "dosbox.h"
#include "logging.h"
#include "mixer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <cmath>
#include <string>

template <size_t array_frames>
class SoftLimiter {
public:
	SoftLimiter() = delete;
	SoftLimiter(const std::string &name, const AudioFrame &scale);

	using in_array_t = std::array<float, array_frames * 2>; // 2 for stereo
	using out_array_t = std::array<int16_t, array_frames * 2>; // 2 for stereo

	void Apply(const in_array_t &in, out_array_t &out, uint16_t frames) noexcept;
	const AudioFrame &GetPeaks() const noexcept { return peak; }
	void PrintStats() const;
	void Reset() noexcept;

private:
	// Amplitude level constants
	using out_limits = std::numeric_limits<int16_t>;
	constexpr static float upper_bound = static_cast<float>(out_limits::max());
	constexpr static float lower_bound = static_cast<float>(out_limits::min());

	bool FindPeaks(const in_array_t &stream, uint16_t frames) noexcept;
	void Release() noexcept;

	std::string channel_name = {};
	AudioFrame limit_scale = {1, 1}; // real-time limit applied to stream
	AudioFrame peak = {1, 1};        // holds real-time peak amplitudes
	const AudioFrame &prescale;      // scale before operating on the stream
	int limited_ms = 0;              // milliseconds that needed limiting
	int non_limited_ms = 0; // milliseconds that didn't need limiting
};

template <size_t array_frames>
SoftLimiter<array_frames>::SoftLimiter(const std::string &name, const AudioFrame &scale)
        : channel_name(name),
          prescale(scale)
{
	static_assert(array_frames > 0, "need some quantity of frames");
	static_assert(array_frames < 16384, "too many frames adds audible latency");
}

template <size_t array_frames>
bool SoftLimiter<array_frames>::FindPeaks(const in_array_t &stream,
                                          const uint16_t samples) noexcept
{
	auto val = stream.begin();
	const auto val_end = stream.begin() + samples;
	AudioFrame local_peak{};
	while (val < val_end) {
		// Left channel
		local_peak.left = std::max(local_peak.left, fabsf(*val++));
		local_peak.right = std::max(local_peak.right, fabsf(*val++));
	}
	peak.left = std::max(peak.left, prescale.left * local_peak.left);
	peak.right = std::max(peak.right, prescale.right * local_peak.right);

	const bool limiting_needed = (peak.left > upper_bound ||
	                              peak.right > upper_bound);

	// Calculate the percent we need to scale down each channel. In cases
	// where one channel is less than the upper-bound, its time_ratio is limited
	// to 1.0 to retain the original level.
	if (limiting_needed)
		limit_scale = {std::min(1.0f, upper_bound / peak.left),
		               std::min(1.0f, upper_bound / peak.right)};

	return limiting_needed;
	// LOG_MSG("%s peak left = %f", channel_name.c_str(),
	// static_cast<double>(peak.left));
}

template <size_t array_frames>
void SoftLimiter<array_frames>::Apply(const in_array_t &in,
                                      out_array_t &out,
                                      const uint16_t req_frames) noexcept
{
	// Ensure the buffers are large enough to handle the request
	const uint16_t samples = req_frames * 2; // left and right channels
	assert(samples <= in.size());

	const bool limiting_needed = FindPeaks(in, samples);

	// get our in and out iterators
	auto in_val = in.begin();
	const auto in_val_end = in.begin() + samples;
	auto out_val = out.begin();

	// apply both the pre-scale and limit-scale to determine the final scale
	const AudioFrame scale{prescale.left * limit_scale.left,
	                       prescale.right * limit_scale.right};

	// Process the signal by pre-scaling and limit-scaling
	// Note: if limit-scaling isn't needed then its values are simply 1.0
	while (in_val < in_val_end) {
		*out_val++ = static_cast<int16_t>(*in_val++ * scale.left);
		*out_val++ = static_cast<int16_t>(*in_val++ * scale.right);
	}
	if (limiting_needed)
		Release();
	else
		non_limited_ms++;
}

template <size_t array_frames>
void SoftLimiter<array_frames>::Release() noexcept
{
	++limited_ms;
	// Decrement the peak(s) one step
	constexpr float delta_db = 0.002709201f; // 0.0235 dB increments
	constexpr float release_amplitude = upper_bound * delta_db;
	if (peak.left > upper_bound)
		peak.left -= release_amplitude;
	if (peak.right > upper_bound)
		peak.right -= release_amplitude;
	// LOG_MSG("GUS: releasing peak_amplitude = %.2f | %.2f",
	//         static_cast<double>(peak.left),
	//         static_cast<double>(peak.right));
}

template <size_t array_frames>
void SoftLimiter<array_frames>::PrintStats() const
{
	constexpr auto ms_per_minute = 1000 * 60;
	const auto ms_total = static_cast<double>(limited_ms) + non_limited_ms;
	const auto minutes_total = ms_total / ms_per_minute;

	// Only print stats if we have more than 30 seconds of data
	if (minutes_total < 0.5)
		return;

	// Only print levels if the peak is at-least 5% of the max
	const auto peak_sample = std::max(peak.left, peak.right);
	constexpr auto five_percent_of_max = upper_bound / 20;
	if (peak_sample < five_percent_of_max)
		return;

	// It's expected and normal for multi-channel audio to periodically
	// accumulate beyond the max, which the soft-limiter gracefully handles.
	// More importantly, users typically care when their overall stream
	// never achieved the maximum levels.
	auto peak_ratio = peak_sample / upper_bound;
	peak_ratio = std::min(peak_ratio, 1.0f);
	LOG_MSG("%s: Peak amplitude reached %.0f%% of max",
	        channel_name.c_str(), 100 * static_cast<double>(peak_ratio));

	// Make a suggestion if the peak amplitude was well below 3 dB. Note that
	// we remove the effect of the external scale by dividing by it. This
	// avoids making a recommendation if the user delibertely wanted quiet
	// output (as an example).
	const auto scale = std::max(prescale.left, prescale.right);
	constexpr auto well_below_3db = static_cast<float>(0.6);
	if (peak_ratio / scale < well_below_3db) {
		const auto suggested_mix_val = 100 * scale / peak_ratio;
		LOG_MSG("%s: If it should be louder, use: mixer %s %.0f",
		        channel_name.c_str(), channel_name.c_str(),
		        static_cast<double>(suggested_mix_val));
	}
	// Make a suggestion if more than 20% of the stream required limiting
	const auto time_ratio = limited_ms / (ms_total + 1); // one ms avoids div-by-0
	if (time_ratio > 0.2) {
		const auto minutes_limited = static_cast<double>(limited_ms) / ms_per_minute;
		const auto reduction_ratio = 1 - time_ratio / 2;
		const auto suggested_mix_val = 100 * reduction_ratio * static_cast<double>(scale);
		LOG_MSG("%s: %.1f%% or %.2f of %.2f minutes needed limiting, consider: mixer %s %.0f",
		        channel_name.c_str(), 100 * time_ratio, minutes_limited,
		        minutes_total, channel_name.c_str(), suggested_mix_val);
	}
}

template <size_t array_frames>
void SoftLimiter<array_frames>::Reset() noexcept
{
	peak = {1, 1};
	limited_ms = 0;
	non_limited_ms = 0;
}

#endif
