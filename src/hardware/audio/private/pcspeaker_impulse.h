// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PCSPEAKER_IMPULSE_H
#define DOSBOX_PCSPEAKER_IMPULSE_H

#include "pcspeaker.h"
#include "pcspeaker_pit.h"

#include <array>
#include <deque>
#include <string>
#include <vector>

#include "audio/channel_names.h"
#include "config/setup.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "misc/support.h"
#include "utils/math_utils.h"

class PcSpeakerImpulse final : public PcSpeaker {
public:
	PcSpeakerImpulse();
	~PcSpeakerImpulse() override;

	void SetFilterState(const FilterState filter_state) override;
	bool TryParseAndSetCustomFilter(const std::string& filter_choice) override;
	void SetCounter(const int counter, const PitMode pit_mode) override;
	void SetPITControl(const PitMode pit_mode) override;
	void SetType(const PpiPortB& port_b) override;
	void PicCallback(const int requested_frames) override;

private:
	void AddImpulse(const float index, const int16_t amplitude);
	float CalcImpulse(double t) const;
	void InitializeLut();
	void ApplyTransitions(const std::vector<PitCounter::Transition>& transitions,
	                      const float index_base, const bool speaker_enabled);

	// Wake, advance the PIT to the current position within this 1ms tick,
	// emitting any transitions that occurred since the last sync. Returns the
	// new position (PIC_TickIndex). Shared preamble of the public entry points.
	float SyncPitToTick();

	// Output amplitude for a given speaker-enable and PIT output level.
	static int16_t OutputAmplitude(const bool speaker_enabled,
	                               const OutputState output);

	// Wake the channel and reset dedup state if we were sleeping. While
	// asleep the mixer emits silence, so prev_amplitude must be reset to
	// neutral on wake or AddImpulse may dedup-skip a legitimate emission.
	// Call at the top of any public entry point that may produce impulses.
	void HandleWakeUp();

	static constexpr auto DeviceName = ChannelName::PcSpeaker;
	static constexpr auto ModelName  = "impulse";

	// Amplitude: manually tuned to roughly match hardware voltage levels.
	// Ref:
	// https://github.com/dosbox-staging/dosbox-staging/files/9494469/3.audio.samples.zip
	static constexpr float PwmScalar = 0.5f;

	static constexpr int16_t PositiveAmplitude = static_cast<int16_t>(
	        Max16BitSampleValue * PwmScalar);
	static constexpr int16_t NegativeAmplitude = -PositiveAmplitude;
	static constexpr int16_t NeutralAmplitude  = 0;

	// Fixed sample rate; must be a multiple of 1000 so each 1ms PIC callback
	// produces an exact integer number of samples (SampleRateHz / 1000).
	static constexpr auto SampleRateHz    = 48000;
	static constexpr auto SampleRatePerMs = SampleRateHz / 1000;

	// Minimum PIT count representable at this sample rate (Nyquist)
	static constexpr auto MinimumCounter = ceil_sdivide(2 * PIT_TICK_RATE,
	                                                    SampleRateHz);

	// Reference sample rate of the original impulse model; the cutoff
	// frequency is kept constant (not scaled with SampleRateHz) so the
	// two models are tonally equivalent.
	static constexpr auto ReferenceSampleRateHz = 32000.0;
	static constexpr auto ReferenceCutoffMargin = 0.2;
	static constexpr auto CutoffMargin = (SampleRateHz / ReferenceSampleRateHz) *
	                                             (2.0 + ReferenceCutoffMargin) -
	                                     2.0;

	static constexpr float SincAmplitudeFade = 0.999f;

	// Keep the same 3.125 ms impulse span as the former 32 kHz /
	// 100-tap configuration.
	static constexpr auto SincFilterDurationUs = 3125;
	static constexpr auto SincFilterQuality =
	        ceil_sdivide(SampleRateHz * SincFilterDurationUs, 1'000'000);

	static constexpr auto SincOversamplingFactor = 32;
	static constexpr auto SincLutSize = SincFilterQuality * SincOversamplingFactor;

	// Undersampled mode 3: rapid reloads used as a noise source
	static constexpr float MaxUndersampledReloadGapMs = 1.0f;

	PitCounter pit_counter = {};

	// Waveform accumulation buffer; large enough for one ms plus sinc tail.
	// This stores fractional impulse contributions until they are integrated
	// into output samples in PicCallback().
	static constexpr auto WaveformSize = SincFilterQuality + SampleRatePerMs;
	std::deque<float> waveform = {};

	std::array<float, SincLutSize> impulse_lut = {};

	// Position within current tick advanced so far (ms, 0..1)
	float pit_last_index = 0.0f;

	// Running amplitude integrator
	float accumulator = 0.0f;

	// Silence tracking for channel sleep
	int tally_of_silence = 0;

	// Port B state (speaker_output + timer2_gating) from the Intel 8255
	// Programmable Peripheral Interface (PPI).
	PpiPortB port_b = {};

	// Undersampled mode 3 reload tracking
	bool have_undersampled_reload = false;
	float undersampled_reload_ms  = 0.0f;

	// Amplitude at last impulse. Re-emitting an identical level would apply
	// another impulse step at the same level and incorrectly bias loudness.
	int16_t prev_amplitude = NeutralAmplitude;

	std::vector<float> render_buf = {};
};

#endif // DOSBOX_PCSPEAKER_IMPULSE_H
