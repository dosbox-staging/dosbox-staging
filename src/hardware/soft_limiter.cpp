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

#include <cmath>

#include "support.h"

SoftLimiter::SoftLimiter(const std::string &name,
                         const AudioFrame &scale,
                         const uint16_t max_frames)
        : channel_name(name),
          prescale(scale),
          max_samples(max_frames * 2)
{}

// Applies the Soft Limiter to the given input sequence and returns the results
// as a reference to a std::vector of 16-bit ints the same length as the input.
typename SoftLimiter::out_t SoftLimiter::Apply(const in_t &in,
                                               const uint16_t frames) noexcept
{
	// Make sure chunk sizes aren' too big or latent
	assertm(frames > 0, "need some quantity of frames");
	assertm(frames <= 16384, "consider using smaller sequence chunks");

	// Make sure the incoming vector has at least the requested frames
	const uint16_t samples = frames * 2; // left and right channels
	assert(in.size() >= samples);

	FindPeaksAndZeroCrosses(in, samples);

	// Size our temporary output vector using the max_samples, which
	// is typically assigned from constexpr's at compile-time. This followed
	// by logically sizing the vector downward (if needed) based on runtime
	// content.
	out_t out;
	out.reserve(max_samples);
	assert(samples <= max_samples);
	out.resize(samples);

	// Given the local peaks found in each side channel, scale or copy the
	// input array into the output array
	ScaleOrCopy<left>(in, samples, prescale.left, precross_peak_pos_left,
	                  zero_cross_left, global_peaks.left, tail_frame.left, out);

	ScaleOrCopy<right>(in, samples, prescale.right, precross_peak_pos_right,
	                   zero_cross_right, global_peaks.right,
	                   tail_frame.right, out);

	SaveTailFrame(frames, out);
	Release();
	return out;
}

// Helper function to evaluate the existing peaks and prior values.
// Saves new local and global peaks, and the input-array iterator of any new
// peaks before the first zero-crossing, along with the first zero-crossing
// position.
void SoftLimiter::FindPeakAndCross(const in_iterator_t in_end,
                                   const in_iterator_t pos,
                                   in_iterator_t &prev_pos,
                                   const float prescalar,
                                   float &local_peak,
                                   in_iterator_t &precross_peak_pos,
                                   in_iterator_t &zero_cross_pos,
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
void SoftLimiter::FindPeaksAndZeroCrosses(const in_t &in, const uint16_t samples) noexcept
{
	auto pos = in.begin();
	const auto pos_end = in.begin() + samples;

	precross_peak_pos_left = in.end();
	precross_peak_pos_right = in.end();
	zero_cross_left = in.end();
	zero_cross_right = in.end();
	in_iterator_t prev_pos_left = in.end();
	in_iterator_t prev_pos_right = in.end();
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
template <size_t channel>
void SoftLimiter::ScaleOrCopy(const in_t &in,
                              const size_t samples,
                              const float prescalar,
                              const in_iterator_t precross_peak_pos,
                              const in_iterator_t zero_cross_pos,
                              const float global_peak,
                              const float tail,
                              out_t &out)
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
void SoftLimiter::PolyFit(in_iterator_t in_pos,
                          const in_iterator_t in_end,
                          out_iterator_t out_pos,
                          const float prescalar,
                          const float poly_a,
                          const float poly_b) const noexcept
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
void SoftLimiter::LinearScale(in_iterator_t in_pos,
                              const in_iterator_t in_end,
                              out_iterator_t out_pos,
                              const float scalar) const noexcept
{
	while (in_pos != in_end) {
		const auto scaled = (*in_pos) * scalar;
		assert(fabsf(scaled) <= out_limits::max());
		*out_pos = static_cast<int16_t>(scaled);
		out_pos += 2;
		in_pos += 2;
	}
}

void SoftLimiter::SaveTailFrame(const uint16_t frames, const out_t &out) noexcept
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
