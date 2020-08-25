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
differences are retained without distorion. (This is known as 'Soft Limiting',
which is superior to Hard Limiting, which truncates values and causes
distortion).

This scale-down-to-fit effect continues to be applied to subsequent input sets,
each time with less effect (provided new peaks aren't detected), until the
scale-down is complete - this period is known as 'Release'.

The release duration is a function of how much we needed to scale down in the
first place.  The larger the scale-down, the longer the effect (typically
ranging from 10's of milliseconds to low-hundreds for > 2x overages).

Use:

The SoftLimiter is a template class where:
 - in_t is inbound sample type, which should be decimal-type (float/double/..)
 - out_t is the outbound sample type, typically a fixed-width integer (int16_t)
 - Static assert check that a float-type is used for in_t, and that
   in_t has a wider-value range than out_t.
 - array_frames is a fixed number of frames in both arrays.

SoftLimiter<float, int16_t, 48> limiter;

You can then repeatedly call:
  limiter.Apply(inarray, outarray);

which will either copy or limit the inarray values into outarray.

The PrintStats function will make mixer suggestions if the inarray
was 40% under the allowed bounds (recommending it be scaled up)
or if the limiter was needed on more than 10% of the stream, in which
case a reduction is recommended.  The PrintStats takes in "pre-scalars",
which might have already been used on the stream, and takes these into
account in its recommendations.
*/

#include "logging.h"
#include "mixer.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>

// Short-hand helpers
#define SL_TEMPLATE_DEF                                                        \
	template <typename in_t, typename out_t, size_t array_frames>
#define SL_TEMPLATE_INST SoftLimiter<in_t, out_t, array_frames>

SL_TEMPLATE_DEF
class SoftLimiter {
public:
	SoftLimiter() = delete;
	SoftLimiter(const std::string &name, const AudioFrame<in_t> &scale);

	using in_array_t = std::array<in_t, array_frames * 2>; // 2 for stereo
	using out_array_t = std::array<out_t, array_frames * 2>; // 2 for stereo

	void Apply(in_array_t &in, out_array_t &out, uint16_t frames) noexcept;
	const AudioFrame<in_t> &GetPeaks() const noexcept { return peak; }
	void PrintStats(const AudioFrame<in_t> &external_scale) const;
	void Reset() noexcept;

private:
	// Amplitude level constants
	using out_limits = std::numeric_limits<out_t>;
	constexpr static in_t upper_bound = static_cast<in_t>(out_limits::max());
	constexpr static in_t lower_bound = static_cast<in_t>(out_limits::min());

	constexpr void CheckTypes() const;
	bool PrescaleAndPeak(in_array_t &stream, uint16_t frames) noexcept;

	std::string channel_name = {};
	AudioFrame<in_t> limit_scale = {1, 1}; // real-time limit applied to stream
	AudioFrame<in_t> peak = {1, 1};   // tracks real-time peak amplitude
	const AudioFrame<in_t> &prescale; // scale before operating on the stream
	int limited_ms = 0;               // milliseconds that needed limiting
	int non_limited_ms = 0; // milliseconds that didn't need limiting
};

// A simple templatized absolute-value helper function
template <typename in_t>
in_t absolute_value(in_t val) noexcept
{
	return (val > 0 ? val : val * -1);
}

SL_TEMPLATE_DEF
SL_TEMPLATE_INST::SoftLimiter(const std::string &name, const AudioFrame<in_t> &scale)
        : channel_name(name),
          prescale(scale)
{
	CheckTypes();
}

SL_TEMPLATE_DEF
constexpr void SL_TEMPLATE_INST::CheckTypes() const
{
	static_assert(std::numeric_limits<in_t>::max() >
	                      std::numeric_limits<out_t>::max(),
	              "in_t needs to hold larger values than out_t");
	static_assert(!std::is_integral<in_t>::value,
	              "out_t needs to be non-integral type, like float or double");
}

SL_TEMPLATE_DEF
bool SL_TEMPLATE_INST::PrescaleAndPeak(in_array_t &stream, const uint16_t samples) noexcept
{
	auto val = stream.begin();
	const auto val_end = stream.begin() + samples;
	while (val < val_end) {
		// Left channel
		*val *= prescale.left;
		peak.left = std::max(peak.left, absolute_value(*val));
		++val;

		// Right channel
		*val *= prescale.right;
		peak.right = std::max(peak.right, absolute_value(*val));
		++val;
	}
	const bool limiting_needed = (peak.left > upper_bound ||
	                              peak.right > upper_bound);

	// Calculate the percent we need to scale down each channel. In cases
	// where one channel is less than the upper-bound, its time_ratio is limited
	// to 1.0 to retain the original level.
	if (limiting_needed)
		limit_scale = {std::min(in_t{1}, upper_bound / peak.left),
		               std::min(in_t{1}, upper_bound / peak.right)};

	return limiting_needed;
	// LOG_MSG("%s peak left = %f", channel_name.c_str(),
	// static_cast<double>(peak.left));
}

SL_TEMPLATE_DEF
void SL_TEMPLATE_INST::Apply(in_array_t &in, out_array_t &out, const uint16_t req_frames) noexcept
{
	// Ensure the buffers are large enough to handle the request
	const uint16_t samples = req_frames * 2; // left and right channels
	assert(samples <= in.size());

	// Prescale the signal and detect any new peaks
	const bool limiting_needed = PrescaleAndPeak(in, samples);

	// get our in and out iterators
	auto in_val = in.begin();
	const auto in_val_end = in.begin() + samples;
	auto out_val = out.begin();

	// If we don't need to limit the signal then simply copy it
	if (!limiting_needed) {
		while (in_val < in_val_end)
			*out_val++ = static_cast<out_t>(*in_val++);
		++non_limited_ms;
		return;
	}
	// Otherwise we need to limit the signal
	while (in_val < in_val_end) {
		*out_val++ = static_cast<out_t>(*in_val++ * limit_scale.left);
		*out_val++ = static_cast<out_t>(*in_val++ * limit_scale.right);
	}
	++limited_ms;

	// Decrement the peak(s) one step
	constexpr in_t delta_db = static_cast<in_t>(0.002709201); // 0.0235 dB
	                                                          // increments
	constexpr in_t release_amplitude = upper_bound * delta_db;
	if (peak.left > upper_bound)
		peak.left -= release_amplitude;
	if (peak.right > upper_bound)
		peak.right -= release_amplitude;
	// LOG_MSG("GUS: releasing peak_amplitude = %.2f | %.2f",
	//         static_cast<double>(peak.left),
	//         static_cast<double>(peak.right));
}

SL_TEMPLATE_DEF
void SL_TEMPLATE_INST::PrintStats(const AudioFrame<in_t> &external_scale) const
{
	constexpr auto ms_per_minute = 1000 * 60;
	const auto ms_total = static_cast<double>(limited_ms) + non_limited_ms;
	const auto minutes_total = ms_total / ms_per_minute;

	// Only print stats if we have more than 30 seconds of data
	if (minutes_total < 0.5)
		return;

	// Only print volume info the peak is at-least 5% of the max
	const auto peak_sample = std::max(peak.left, peak.right);
	constexpr auto five_percent_of_max = upper_bound / 20;
	if (peak_sample < five_percent_of_max)
		return;

	// It's expected and normal for multi-channel audio to periodically
	// accumulate beyond the max, which the soft-limiter gracefully handles.
	// More importantly, users typically care when their overall stream
	// never achieved the maximum levels.
	auto peak_ratio = peak_sample / upper_bound;
	peak_ratio = std::min(peak_ratio, static_cast<in_t>(1.0));
	LOG_MSG("%s: Peak amplitude reached %.0f%% of max",
	        channel_name.c_str(), 100 * static_cast<double>(peak_ratio));

	// Make a suggestion if the peak volume was well below 3 dB. Note that
	// we remove the effect of the external scale by dividing by it. This
	// avoids making a recommendation if the user delibertely wanted quiet
	// output (as an example).
	const auto scale = std::max(external_scale.left, external_scale.right);
	constexpr auto well_below_3db = static_cast<in_t>(0.6);
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

SL_TEMPLATE_DEF
void SL_TEMPLATE_INST::Reset() noexcept
{
	peak = {1, 1};
	limited_ms = 0;
	non_limited_ms = 0;
}

#undef SL_TEMPLATE_DEF
#undef SL_TEMPLATE_INST

#endif
