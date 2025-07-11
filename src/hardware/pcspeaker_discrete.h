// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pcspeaker.h"

#include <queue>
#include <string>

#include "channel_names.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "support.h"
#include "timer.h"

class PcSpeakerDiscrete final : public PcSpeaker {
public:
	PcSpeakerDiscrete();
	~PcSpeakerDiscrete() override;

	void SetFilterState(const FilterState filter_state) override;
	bool TryParseAndSetCustomFilter(const std::string& filter_choice) override;
	void SetCounter(const int cntr, const PitMode m) override;
	void SetPITControl(const PitMode) override {}
	void SetType(const PpiPortB& b) override;
	void PicCallback(const int requested_frames) override;

private:
	void AddDelayEntry(const float index, float vol);
	void ForwardPIT(const float newindex);
	bool IsWaveSquare() const;
	float NeutralOr(const float fallback) const;
	float NeutralLastPitOr(const float fallback) const;

	// Constants
	static constexpr auto device_name = ChannelName::PcSpeaker;
	static constexpr auto model_name  = "discrete";

	// The discrete PWM scalar was manually adjusted to roughly match
	// voltage levels recorded from a hardware PC speaker
	// Ref:https://github.com/dosbox-staging/dosbox-staging/files/9494469/3.audio.samples.zip
	static constexpr float pwm_scalar = 0.75f;
	static constexpr float sqw_scalar = pwm_scalar / 2.0f;

	// Amplitude constants
	static constexpr float amp_positive = Max16BitSampleValue * pwm_scalar;
	static constexpr float amp_negative = Min16BitSampleValue * pwm_scalar;
	static constexpr float amp_neutral = (amp_positive + amp_negative) / 2.0f;

	struct DelayEntry {
		float index = 0.0f;
		float vol   = 0.0f;
	};

	std::queue<DelayEntry> entries = {};

	PpiPortB port_b      = {};
	PpiPortB prev_port_b = {};

	PitMode pit_mode      = PitMode::SquareWave;
	PitMode prev_pit_mode = PitMode::SquareWave;

	float pit_last     = 0.0f;
	float pit_max      = period_of_1k_pit_ticks_f * 1320.0f;
	float pit_half     = pit_max / 2.0f;
	float pit_new_max  = pit_max;
	float pit_new_half = pit_half;
	float pit_index    = 0.0f;
	float volwant      = 0.0f;
	float volcur       = 0.0f;
	float last_index   = 0.0f;

	int sample_rate_hz    = 0;
	int minimum_tick_rate = 0;
};
