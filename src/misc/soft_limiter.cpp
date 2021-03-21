/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2020-2021  Kevin R. Croft <krcroft@gmail.com>
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

#include "soft_limiter.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "support.h"

constexpr static auto relaxed = std::memory_order_relaxed;

constexpr static float bounds = static_cast<float>(INT16_MAX - 1);

SoftLimiter::SoftLimiter(const std::string &name)
        : channel_name(name)
{
	UpdateLevels({1, 1}, 1); // default to unity (ie: no) scaling
	limited_tally = 0;
	non_limited_tally = 0;
}

void SoftLimiter::UpdateLevels(const AudioFrame &desired_levels,
                               const float desired_multiplier)
{
	range_multiplier = desired_multiplier;
	prescale = {desired_levels.left * desired_multiplier,
	            desired_levels.right * desired_multiplier};
}

//  Limit the input array and returned as integer array
void SoftLimiter::Process(const std::vector<float> &in,
                          const uint16_t frames,
                          std::vector<int16_t> &out) noexcept
{
	// Make sure chunk sizes aren't too big or latent
	assertm(frames > 0, "need some quantity of frames");
	assertm(frames <= 16384, "consider using smaller sequence chunks");

	// Make sure the incoming vector has at least the requested frames
	const uint16_t samples = frames * 2; // left and right channels
	assert(in.size() >= samples);
	assert(out.size() >= samples);

	auto precross_peak_pos_left = in.end();
	auto precross_peak_pos_right = in.end();
	auto zero_cross_left = in.end();
	auto zero_cross_right = in.end();

	FindPeaksAndZeroCrosses(in, precross_peak_pos_left, precross_peak_pos_right,
	                        zero_cross_left, zero_cross_right, samples);

	// Given the local peaks found in each side channel, scale or copy the
	// input array into the output array
	constexpr int8_t left = 0;
	ScaleOrCopy<left>(in, samples, prescale.load(relaxed).left,
	                  precross_peak_pos_left, zero_cross_left,
	                  global_peaks.left, tail_frame.left, out);

	constexpr int8_t right = 1;
	ScaleOrCopy<right>(in, samples, prescale.load(relaxed).right,
	                   precross_peak_pos_right, zero_cross_right,
	                   global_peaks.right, tail_frame.right, out);

	SaveTailFrame(frames, out);
	Release();
}

// Helper function to evaluate the existing peaks and prior values.
// Saves new local and global peaks, and the input-array iterator of any new
// peaks before the first zero-crossing, along with the first zero-crossing
// position.
void FindPeakAndCross(const SoftLimiter::in_iterator_t in_end,
                      const SoftLimiter::in_iterator_t pos,
                      SoftLimiter::in_iterator_t &prev_pos,
                      const float prescalar,
                      float &local_peak,
                      SoftLimiter::in_iterator_t &precross_peak_pos,
                      SoftLimiter::in_iterator_t &zero_cross_pos,
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
void SoftLimiter::FindPeaksAndZeroCrosses(const std::vector<float> &in,
                                          in_iterator_t &precross_peak_pos_left,
                                          in_iterator_t &precross_peak_pos_right,
                                          in_iterator_t &zero_cross_left,
                                          in_iterator_t &zero_cross_right,
                                          const uint16_t samples) noexcept
{
	auto pos = in.begin();
	const auto pos_end = in.begin() + samples;

	auto prev_pos_left = in.end();
	auto prev_pos_right = in.end();
	AudioFrame local_peaks = global_peaks;

	while (pos != pos_end) {
		FindPeakAndCross(in.end(), pos++, prev_pos_left,
		                 prescale.load(relaxed).left, local_peaks.left,
		                 precross_peak_pos_left, zero_cross_left,
		                 global_peaks.left);

		FindPeakAndCross(in.end(), pos++, prev_pos_right,
		                 prescale.load(relaxed).right,
		                 local_peaks.right, precross_peak_pos_right,
		                 zero_cross_right, global_peaks.right);
	}
}

// Scale or copy the given channel's samples into the output array
template <int8_t channel>
void SoftLimiter::ScaleOrCopy(const std::vector<float> &in,
                              const uint16_t samples,
                              const float prescalar,
                              const in_iterator_t precross_peak_pos,
                              const in_iterator_t zero_cross_pos,
                              const float global_peak,
                              const float tail,
                              std::vector<int16_t> &out)
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
		limited_tally++;

		// We have an existing peak ...
	} else if (global_peak > bounds) {
		// so scale the entire sequence a a ratio of the peak.
		const auto current_scalar = prescalar * bounds / global_peak;
		LinearScale(in_start, in_end, out_start, current_scalar);
		limited_tally++;

		// The current sequence is fully inbounds ...
	} else {
		// so simply prescale the entire sequence.
		LinearScale(in_start, in_end, out_start, prescalar);
		++non_limited_tally;
	}
}

// Apply the polynomial coefficients to the sequence
void SoftLimiter::PolyFit(in_iterator_t in_pos,
                          const in_iterator_t in_end,
                          out_iterator_t out_pos,
                          const float prescalar,
                          const float poly_a,
                          const float poly_b) const noexcept
{
	while (in_pos != in_end) {
		const auto fitted = poly_a * (*in_pos * prescalar - poly_b) + poly_b;
		assert(fabsf(fitted) < INT16_MAX);
		*out_pos = static_cast<int16_t>(fitted);
		out_pos += 2;
		in_pos += 2;
	}
}

// Apply the scalar to the sequence
void SoftLimiter::LinearScale(in_iterator_t in_pos,
                              const in_iterator_t in_end,
                              out_iterator_t out_pos,
                              const float scalar) const noexcept
{
	while (in_pos != in_end) {
		const auto scaled = (*in_pos) * scalar;
		assert(fabsf(scaled) < INT16_MAX);
		*out_pos = static_cast<int16_t>(scaled);
		out_pos += 2;
		in_pos += 2;
	}
}

void SoftLimiter::SaveTailFrame(const uint16_t frames,
                                const std::vector<int16_t> &out) noexcept
{
	const size_t i = (frames - 1) * 2;
	tail_frame.left = static_cast<float>(out[i]);
	tail_frame.right = static_cast<float>(out[i + 1]);
}

// If either channel was out of bounds, then decrement their peak
void SoftLimiter::Release() noexcept
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
void SoftLimiter::PrintStats() const
{
	const auto peak_sample = std::max(global_peaks.left, global_peaks.right);
	const auto peak_ratio = std::min(peak_sample / bounds, 1.0f);

	// Only print information if the channel reached 2% of full amplitude
	if (peak_ratio < 0.02f)
		return;

	// Inform the user what percent of the dynamic-range was reached
	LOG_MSG("%s: Peak amplitude reached %.0f%% of max",
	        channel_name.c_str(), 100 * static_cast<double>(peak_ratio));

	// Inform when the stream fell short of using the full dynamic-range
	const auto scale = std::max(prescale.load().left, prescale.load().right) /
	                   range_multiplier;
	constexpr auto well_below_3db = 0.6f;
	if (peak_ratio < well_below_3db) {
		const auto suggested_mix_val = 100 * scale / peak_ratio;
		LOG_MSG("%s: If it should be louder, use: mixer %s %.0f",
		        channel_name.c_str(), channel_name.c_str(),
		        static_cast<double>(suggested_mix_val));
	}

	// Inform if more than 20% of the stream required limiting
	const auto total_tally = limited_tally + non_limited_tally;
	const auto limited_ratio = limited_tally / (total_tally + 1.0); // +1 avoid div-by-0
	if (limited_ratio > 0.2) {
		const auto suggested_mix_pct = 100 * (1 - limited_ratio) *
		                               static_cast<double>(scale);
		LOG_MSG("%s: %.1f%% of the audio needed limiting, consider: mixer %s %.0f",
		        channel_name.c_str(), 100 * limited_ratio,
		        channel_name.c_str(), suggested_mix_pct);
	}
}

// A paused audio source should Reset() the limiter so it starts with
// fresh peaks and a zero-tail if/when the stream is restarted.
void SoftLimiter::Reset() noexcept
{
	// if the current peaks are over the upper bounds, then we simply save
	// the upper bound because we want retain information about the peak
	// amplitude when printing statistics.

	constexpr auto upper = bounds; // (workaround to prevent link-errors)
	global_peaks.left = std::min(global_peaks.left, upper);
	global_peaks.right = std::min(global_peaks.right, upper);
	tail_frame = {0, 0};
}
