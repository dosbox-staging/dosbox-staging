// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "noise_gate.h"

#include <cassert>
#include <cmath>

#include "audio_frame.h"
#include "checks.h"

CHECK_NARROWING();

NoiseGate::NoiseGate() = default;

NoiseGate::~NoiseGate() = default;

void NoiseGate::Configure(const int _sample_rate_hz, const float _0dbfs_sample_value,
                          const float threshold_db, const float attack_time_ms,
                          const float release_time_ms)
{
	assert(_sample_rate_hz > 0);
	assert(attack_time_ms > 0.0f);
	assert(release_time_ms > 0.0f);

	scale_in  = 1.0f / _0dbfs_sample_value;
	scale_out = _0dbfs_sample_value;

	threshold_value = std::exp2(threshold_db / 6.0f);

	const auto sample_rate_hz = static_cast<float>(_sample_rate_hz);

	attack_coeff = 1.0f /
	               std::pow(10.0f, 1000.0f / (attack_time_ms * sample_rate_hz));

	release_coeff = 1.0f /
	                std::pow(10.0f, 1000.0f / (release_time_ms * sample_rate_hz));

	seek_v  = 1.0f;
	seek_to = 1.0f;

	// High-pass filter to remove DC offset and useless ultra-low frequency
	// rumble that would throw off the threshold detector.
	constexpr auto HighpassFrequencyHz = 5;
	for (auto& f : highpass_filter) {
		f.setup(sample_rate_hz, HighpassFrequencyHz);
	}
}

AudioFrame NoiseGate::Process(const AudioFrame in)
{
	// Scale input to [-1.0, 1.0] range and apply high-pass filter
	// to any remove DC offset.
	const float left  = highpass_filter[0].filter(in.left * scale_in);
	const float right = highpass_filter[1].filter(in.right * scale_in);

	const auto is_open = std::abs(left) > threshold_value ||
	                     std::abs(right) > threshold_value;

	if (is_open) {
		// attack phase
		seek_v = seek_v * attack_coeff + (1 - attack_coeff);
	} else {
		// release phase
		seek_v *= release_coeff;
	}

	const auto gain_scalar = seek_v * scale_out;

	return {left * gain_scalar, right * gain_scalar};
}
