/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#include <cmath>

#include "mixer.h"
#include "compressor.h"

constexpr double log_to_db = 8.685889638065035;  // 20.0 / log(10.0)
constexpr double db_to_log = 0.1151292546497022; // log(10.0) / 20.0

std::array<double, 120> attack_times_ms = {};

constexpr void fill_attack_times_lut()
{
	for (size_t i = 0; i < attack_times_ms.size(); ++i) {
		const auto n = i + 1;
		attack_times_ms[i] = ((0.08924 / n) + (0.60755 / (n * n)) - 0.00006);
	}
}

Compressor::Compressor()
{
}

Compressor::~Compressor() {}

void Compressor::Configure(const uint16_t _sample_rate_hz,
                           const float threshold_db, const float _ratio,
                           const float release_time_ms, const float rms_window_ms)
{
	assert(_sample_rate_hz > 0);
	assert(_ratio > 0.0f);
	assert(release_time_ms > 0.0f);
	assert(rms_window_ms > 0.0f);

	sample_rate_hz = _sample_rate_hz;
	ratio          = _ratio;

	threshold_value = exp(threshold_db * db_to_log);

	release_coeff = exp(-millis_in_second / (release_time_ms * sample_rate_hz));
	rms_coeff     = exp(-millis_in_second / (rms_window_ms   * sample_rate_hz));

	Reset();
}

void Compressor::Reset()
{
	attack_time_ms  = 0.010;
	attack_coeff    = exp(-1.0 / (attack_time_ms * sample_rate_hz));
	comp_ratio      = 0.0;
	run_db          = 0.0;
	run_sum_squares = 0.0;
	over_db         = 0.0;
	run_max_db      = 0.0;
	max_over_db     = 0.0;
}

AudioFrame Compressor::Process(const AudioFrame &in)
{
	const double left  = in.left;
	const double right = in.right;

	const auto sum_squares = (left * left) + (right * right);
	run_sum_squares = sum_squares + rms_coeff * (run_sum_squares - sum_squares);
	const auto det = sqrt(fmax(0.0, run_sum_squares));

	over_db = 2.08136898 * log(det / threshold_value) * log_to_db;

	if (over_db > max_over_db) {
		max_over_db = over_db;
		const auto i = std::clamp(static_cast<unsigned long>(std::fabs(over_db)),
		                          0ul,
		                          attack_times_ms.size() - 1);
		attack_time_ms = attack_times_ms[i];
		attack_coeff   = exp(-1.0 / (attack_time_ms * sample_rate_hz));
	}

	over_db = fmax(0.0, over_db);

	if (over_db > run_db)
		run_db = over_db + attack_coeff * (run_db - over_db);
	else
		run_db = over_db + release_coeff * (run_db - over_db);

	over_db    = run_db;
	comp_ratio = 1.0 + (ratio - 1.0) * fmin(over_db, 6.0) / 6.0;

	const auto gain_reduction_db = -over_db * (comp_ratio - 1.0) / comp_ratio;
	const auto gain_reduction_factor = exp(gain_reduction_db * db_to_log);

	run_max_db  = max_over_db + release_coeff * (run_max_db - max_over_db);
	max_over_db = run_max_db;

	return {static_cast<float>(left * gain_reduction_factor),
	        static_cast<float>(right * gain_reduction_factor)};
}

