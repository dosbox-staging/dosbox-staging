// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/compressor.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "audio/mixer.h"
#include "utils/checks.h"

CHECK_NARROWING();

constexpr auto log_to_db = 8.685889638065035f;  // 20.0 / log(10.0)
constexpr auto db_to_log = 0.1151292546497022f; // log(10.0) / 20.0

Compressor::Compressor() = default;

Compressor::~Compressor() = default;

void Compressor::Configure(const int _sample_rate_hz,
                           const float _0dbfs_sample_value, const float threshold_db,
                           const float _ratio, const float attack_time_ms,
                           const float release_time_ms, const float rms_window_ms)
{
	assert(_sample_rate_hz > 0);
	assert(_0dbfs_sample_value > 0.0f);
	assert(_ratio > 0.0f);
	assert(attack_time_ms > 0.0f);
	assert(release_time_ms > 0.0f);
	assert(rms_window_ms > 0.0f);

	sample_rate_hz = static_cast<float>(_sample_rate_hz);

	scale_in  = 1.0f / _0dbfs_sample_value;
	scale_out = _0dbfs_sample_value;

	threshold_value = std::exp(threshold_db * db_to_log);
	ratio           = _ratio;

	attack_coeff = std::exp(-1.0f / (attack_time_ms * sample_rate_hz));
	release_coeff = std::exp(-MillisInSecondF / (release_time_ms * sample_rate_hz));

	rms_coeff = std::exp(-MillisInSecondF / (rms_window_ms * sample_rate_hz));

	Reset();
}

void Compressor::Reset()
{
	comp_ratio      = 0.0f;
	run_db          = 0.0f;
	run_sum_squares = 0.0f;
	over_db         = 0.0f;
	run_max_db      = 0.0f;
	max_over_db     = 0.0f;
}

AudioFrame Compressor::Process(const AudioFrame in)
{
	const float left  = in.left * scale_in;
	const float right = in.right * scale_in;

	const auto sum_squares = (left * left) + (right * right);
	run_sum_squares = sum_squares + rms_coeff * (run_sum_squares - sum_squares);
	const auto det = std::sqrt(std::max(0.0f, run_sum_squares));

	over_db = 2.08136898f * std::log(det / threshold_value) * log_to_db;

	if (over_db > max_over_db) {
		max_over_db = over_db;
	}

	over_db = std::max(0.0f, over_db);

	run_db = over_db + (run_db - over_db) * (over_db > run_db ? attack_coeff
	                                                          : release_coeff);

	over_db = run_db;

	constexpr auto ratio_threshold_db = 6.0f;
	comp_ratio = 1.0f + ratio * std::min(over_db, ratio_threshold_db) /
	                            ratio_threshold_db;

	const auto gain_reduction_db = -over_db * (comp_ratio - 1.0f) / comp_ratio;
	const auto gain_reduction_factor = std::exp(gain_reduction_db * db_to_log);

	run_max_db  = max_over_db + release_coeff * (run_max_db - max_over_db);
	max_over_db = run_max_db;

	const auto gain_scalar = gain_reduction_factor * scale_out;

	return {left * gain_scalar, right * gain_scalar};
}

