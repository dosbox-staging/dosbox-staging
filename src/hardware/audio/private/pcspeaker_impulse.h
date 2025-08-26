// SPDX-FileSPDText:X Identifier: GPL-2.0-or-later
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PCSPEAKER_IMPULSE_H
#define DOSBOX_PCSPEAKER_IMPULSE_H

#include "pcspeaker.h"

#include <array>
#include <deque>
#include <string>

#include "audio/channel_names.h"
#include "config/setup.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "misc/support.h"

class PcSpeakerImpulse final : public PcSpeaker {
public:
	PcSpeakerImpulse();
	~PcSpeakerImpulse() override;

	void SetFilterState(const FilterState filter_state) override;
	bool TryParseAndSetCustomFilter(const std::string& filter_choice) override;
	void SetCounter(const int cntr, const PitMode pit_mode) override;
	void SetPITControl(const PitMode pit_mode) override;
	void SetType(const PpiPortB& port_b) override;
	void PicCallback(const int requested_frames) override;

private:
	void AddImpulse(float index, const int16_t amplitude);
	void AddPITOutput(const float index);
	void ForwardPIT(const float new_index);
	float CalcImpulse(const double t) const;
	void InitializeImpulseLUT();

	// Constants
	static constexpr auto device_name = ChannelName::PcSpeaker;
	static constexpr auto model_name  = "impulse";

	// Amplitude constants

	// The impulse PWM scalar was manually adjusted to roughly match voltage
	// levels recorded from a hardware PC speaker
	// Ref:https://github.com/dosbox-staging/dosbox-staging/files/9494469/3.audio.samples.zip
	static constexpr float pwm_scalar = 0.5f;

	static constexpr int16_t positive_amplitude = static_cast<int16_t>(
	        Max16BitSampleValue * pwm_scalar);

	static constexpr int16_t negative_amplitude = -positive_amplitude;
	static constexpr int16_t neutral_amplitude  = 0;

	static constexpr float ms_per_pit_tick = 1000.0f / PIT_TICK_RATE;

	// Mixer channel constants
	static constexpr auto sample_rate_hz     = 32000;
	static constexpr auto sample_rate_per_ms = sample_rate_hz / 1000;

	static constexpr auto minimum_counter = 2 * PIT_TICK_RATE / sample_rate_hz;

	// must be greater than 0.0f
	static constexpr float cutoff_margin = 0.2f;

	// Should be selected based on sampling rate
	static constexpr float sinc_amplitude_fade     = 0.999f;
	static constexpr auto sinc_filter_quality      = 100;
	static constexpr auto sinc_oversampling_factor = 32;

	static constexpr auto sinc_filter_width = sinc_filter_quality *
	                                          sinc_oversampling_factor;

	static constexpr float max_possible_pit_ms = 1320000.0f / PIT_TICK_RATE;

	// Compound types and containers
	struct PitState {
		// PIT starts in mode 3 (SquareWave) at ~903 Hz (pit_max) with
		// positive amplitude
		float max_ms            = max_possible_pit_ms;
		float new_max_ms        = max_possible_pit_ms;
		float half_ms           = max_possible_pit_ms / 2.0f;
		float new_half_ms       = max_possible_pit_ms / 2.0f;
		float index             = 0.0f;
		float last_index        = 0.0f;
		float mode1_pending_max = 0.0f;

		// PIT boolean state
		bool mode1_waiting_for_counter = false;
		bool mode1_waiting_for_trigger = true;
		bool mode3_counting            = false;

		PitMode mode = PitMode::SquareWave;

		int16_t amplitude      = positive_amplitude;
		int16_t prev_amplitude = negative_amplitude;
	} pit = {};

	std::deque<float> waveform_deque = {};

	std::array<float, sinc_filter_width> impulse_lut = {};

	PpiPortB prev_port_b = {};

	int tally_of_silence = 0;
};


#endif // DOSBOX_PCSPEAKER_IMPULSE_H
