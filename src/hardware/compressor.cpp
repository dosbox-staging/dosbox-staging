#include <math.h>

#include "mixer.h"
#include "compressor.h"

constexpr double log_to_db = 8.685889638065035;  // 20.0 / log(10.0)
constexpr double db_to_log = 0.1151292546497022; // log(10.0) / 20.0

Compressor::Compressor()
{
	for (int i = 0; i < num_attack_times; ++i)
		attack_times_ms[i] = ((0.08924 / i) + (0.60755 / (i * i)) - 0.00006);
}

Compressor::~Compressor() {}

void Compressor::Configure(const uint16_t _sample_rate_hz,
                           const float threshold_db, const float _ratio,
                           const float release_time_ms, const float rms_window_ms)
{
	sample_rate_hz = _sample_rate_hz;
	ratio          = _ratio;

	threshold_value = exp(threshold_db * db_to_log);

	constexpr auto millis_in_second = 1000.0;

	release_coeff = exp(-millis_in_second / (release_time_ms * sample_rate_hz));
	rms_coeff     = exp(-millis_in_second / (rms_window_ms   * sample_rate_hz));

	Reset();
}

void Compressor::Reset()
{
	attack_time_ms = 0.010;
	comp_ratio     = 0.0;
	run_db         = 0.0;
	over_db        = 0.0;
	run_max_db     = 0.0;
	max_over_db    = 0.0;
	attack_coeff   = exp(-1.0 / (attack_time_ms * sample_rate_hz));
}

AudioFrame Compressor::Process(const AudioFrame &in)
{
	const double left  = in.left;
	const double right = in.right;

	const auto average = (left * left) + (right * right);
	run_average        = average + rms_coeff * (run_average - average);
	const auto det     = sqrt(fmax(0.0, run_average));

	over_db = 2.08136898 * log(det / threshold_value) * log_to_db;

	if (over_db > max_over_db) {
		max_over_db    = over_db;
		const auto i   = static_cast<size_t>(fmax(0.0, floor(fabs(over_db))));
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

