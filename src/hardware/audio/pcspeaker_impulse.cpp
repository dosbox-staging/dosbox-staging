// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/pcspeaker_impulse.h"

#include <algorithm>
#include <cmath>

#include "utils/checks.h"

CHECK_NARROWING();

#if 0
#define DEBUG_PCSPEAKER 1
#endif

// Unnormalized sinc: sinc(0) = 1, sinc(x) = sin(x)/x
static constexpr double sinc(const double x)
{
	if (x == 0.0) {
		return 1.0;
	}
	return std::sin(x) / x;
}

float PcSpeakerImpulse::CalcImpulse(const double t) const
{
	// Raised-cosine-windowed sinc, identical to the impulse model
	constexpr double Fs = SampleRateHz;
	constexpr double Fc = Fs / (2.0 + CutoffMargin);
	constexpr double Q  = static_cast<double>(SincFilterQuality);

	// The model integrates these impulses, so the integrated step height is
	// proportional to the sum of the taps, which grows with the sample rate
	// (more taps span the same fixed-duration window). Normalise by the
	// reference rate so the step height (output loudness) matches
	// the original 32 kHz model.
	constexpr double Gain = ReferenceSampleRateHz / Fs;

	float res = 0.0f;

	if ((0 < t) && (t * Fs < Q)) {
		constexpr auto Midpoint = Q / (2.0 * Fs);
		const auto window = 1.0 +
		                    std::cos(2.0 * Fs * M_PI * (Midpoint - t) / Q);
		const auto amplitude = Gain * window *
		                       sinc(2.0 * Fc * M_PI * (t - Midpoint)) / 2.0;
		res = static_cast<float>(amplitude);
	}

	return res;
}

void PcSpeakerImpulse::HandleWakeUp()
{
	if (channel->WakeUp()) {
		prev_amplitude = NeutralAmplitude;
	}
}

void PcSpeakerImpulse::InitializeLut()
{
	const auto sample_step = 1.0 /
	                         (static_cast<double>(SampleRateHz) *
	                          SincOversamplingFactor);

	double sample_time = 0.0;
	for (auto it = impulse_lut.begin(); it != impulse_lut.end(); ++it) {
		*it = CalcImpulse(sample_time);
		sample_time += sample_step;
	}
}

void PcSpeakerImpulse::AddImpulse(const float index, const int16_t amplitude)
{
	if (amplitude == prev_amplitude) {
		return;
	}
	prev_amplitude = amplitude;

	const auto clamped = std::clamp(index, 0.0f, 1.0f);

	const auto base      = static_cast<double>(clamped) * SampleRatePerMs;
	const auto phase_raw = static_cast<int>(base * SincOversamplingFactor) %
	                       SincOversamplingFactor;

	const auto offset = static_cast<int>(base) + (phase_raw != 0 ? 1 : 0);

	const auto phase = (phase_raw != 0) ? (SincOversamplingFactor - phase_raw)
	                                    : 0;
	const auto k_start = (phase_raw == 0) ? 1 : 0;

	const auto k_end = std::min(SincFilterQuality,
	                            static_cast<int>(waveform.size()) - offset);

	for (auto k = k_start; k < k_end; ++k) {
		waveform[static_cast<size_t>(offset) + static_cast<size_t>(k)] +=
		        amplitude *
		        impulse_lut[static_cast<size_t>(phase) +
		                    static_cast<size_t>(k) * static_cast<size_t>(SincOversamplingFactor)];
	}
}

int16_t PcSpeakerImpulse::OutputAmplitude(const bool speaker_enabled,
                                          const OutputState output)
{
	if (!speaker_enabled) {
		return NegativeAmplitude;
	}
	return (output == OutputState::High) ? PositiveAmplitude : NegativeAmplitude;
}

void PcSpeakerImpulse::ApplyTransitions(const std::vector<PitCounter::Transition>& transitions,
                                        const float index_base,
                                        const bool speaker_enabled)
{
	for (const auto& t : transitions) {
		const auto index = index_base + t.offset_ms;
		AddImpulse(index, OutputAmplitude(speaker_enabled, t.output));
	}
}

float PcSpeakerImpulse::SyncPitToTick()
{
	HandleWakeUp();

	const auto index = static_cast<float>(PIC_TickIndex());
	const auto delta = index - pit_last_index;
	if (delta > 0.0f) {
		const auto t = pit_counter.Advance(delta);
		ApplyTransitions(t, pit_last_index, port_b.speaker_output);
	}
	pit_last_index = index;
	return index;
}

// Control word written (timer.cpp calls this before SetCounter when mode changes)
void PcSpeakerImpulse::SetPITControl(const PitMode pit_mode)
{
#ifdef DEBUG_PCSPEAKER
	LOG_INFO("PCSPEAKER: %.3f ctrl=%s gate=%d m3=%d spk=%d",
	         PIC_FullIndex(),
	         pit_mode_to_string(pit_mode),
	         pit_counter.IsGateEnabled(),
	         pit_counter.IsMode3Active(),
	         static_cast<int>(port_b.speaker_output));
#endif
	const auto index = SyncPitToTick();

	const auto t = pit_counter.WriteControl(pit_mode);
	ApplyTransitions(t, index, port_b.speaker_output);
}

// Count register written
void PcSpeakerImpulse::SetCounter(const int counter, const PitMode pit_mode)
{
#ifdef DEBUG_PCSPEAKER
	LOG_INFO("PCSPEAKER: %.3f counter=%d mode=%s gate=%d m3=%d spk=%d under=%d",
	         PIC_FullIndex(),
	         counter,
	         pit_mode_to_string(pit_mode),
	         pit_counter.IsGateEnabled(),
	         pit_counter.IsMode3Active(),
	         static_cast<int>(port_b.speaker_output),
	         static_cast<int>(have_undersampled_reload));
#endif
	const auto index = SyncPitToTick();

	const bool is_square = (pit_mode == PitMode::SquareWave ||
	                        pit_mode == PitMode::SquareWaveAlias);

	if (is_square && counter > 0 && counter < MinimumCounter) {
		// Counter is too high-frequency to represent at our sample
		// rate. Rapid reloads of undersampled counts are used by some
		// programs as a noise source; toggle amplitude for each such
		// reload.
		const auto now_ms = static_cast<float>(PIC_FullIndex());
		const auto previous_reload_gap_ms = now_ms - undersampled_reload_ms;
		const auto is_rapid_reload = have_undersampled_reload &&
		                             previous_reload_gap_ms >= 0.0f &&
		                             previous_reload_gap_ms <=
		                                     MaxUndersampledReloadGapMs;

		if (port_b.timer2_gating_and_speaker_out.all() && is_rapid_reload) {
			// Toggle — emit whatever the opposite of current output is
			const int16_t toggled = (prev_amplitude == PositiveAmplitude)
			                              ? NegativeAmplitude
			                              : PositiveAmplitude;
			AddImpulse(index, toggled);
		}

		// Stop the PIT from producing further output at the old
		// frequency. Without this, Advance() keeps oscillating at the
		// prior valid period.
		pit_counter.Invalidate();

		have_undersampled_reload = true;
		undersampled_reload_ms   = now_ms;
		return;
	}

	have_undersampled_reload = false;

	const auto t = pit_counter.WriteCount(counter);
	ApplyTransitions(t, index, port_b.speaker_output);

	// If a gate-rising edge was suppressed while in undersampled state,
	// pit_counter.gate is still false. Sync it now so oscillation can start.
	// In the normal path (gate already synced), EnableGate(true) is a no-op.
	if (port_b.timer2_gating) {
		const auto gate_t = pit_counter.EnableGate(true);
		ApplyTransitions(gate_t, index, port_b.speaker_output);
	}

#ifdef DEBUG_PCSPEAKER
	LOG_INFO("PCSPEAKER: %.3f counter=%d -> gate=%d m3=%d",
	         PIC_FullIndex(),
	         counter,
	         pit_counter.IsGateEnabled(),
	         pit_counter.IsMode3Active());
#endif
}

// Port B changed (speaker_output and/or timer2_gating bits)
void PcSpeakerImpulse::SetType(const PpiPortB& new_port_b)
{
#ifdef DEBUG_PCSPEAKER
	LOG_INFO("PCSPEAKER: %.3f type spk=%d gate=%d (was spk=%d gate=%d) m3=%d under=%d",
	         PIC_FullIndex(),
	         static_cast<int>(new_port_b.speaker_output),
	         static_cast<int>(new_port_b.timer2_gating),
	         static_cast<int>(port_b.speaker_output),
	         static_cast<int>(port_b.timer2_gating),
	         pit_counter.IsMode3Active(),
	         static_cast<int>(have_undersampled_reload));
#endif
	const auto index = SyncPitToTick();

	const bool gate_changed = new_port_b.timer2_gating != port_b.timer2_gating;
	const bool speaker_enabled = new_port_b.speaker_output;
	port_b.data                = new_port_b.data;

	const bool gate_triggered = gate_changed && new_port_b.timer2_gating;

	if (gate_changed) {
		// Don't allow a gate-rising edge to restart the PIT while we
		// are in the undersampled-suppressed state; the stored period
		// is ultrasonic. Gate-falling still propagates so modes 2/3
		// correctly force output HIGH.
		if (!gate_triggered || !have_undersampled_reload) {
			const auto t = pit_counter.EnableGate(new_port_b.timer2_gating);
			ApplyTransitions(t, index, speaker_enabled);
		}
	}

	AddImpulse(index,
	           OutputAmplitude(speaker_enabled, pit_counter.GetOutputState()));
}

void PcSpeakerImpulse::PicCallback(const int requested_frames)
{
	HandleWakeUp();

	// Advance PIT to end of this 1 ms tick
	const auto remaining = 1.0f - pit_last_index;
	if (remaining > 0.0f) {
		const auto t = pit_counter.Advance(remaining);
		ApplyTransitions(t, pit_last_index, port_b.speaker_output);
	}
	pit_last_index = 0.0f;

	render_buf.clear();

	int remaining_frames = requested_frames;

	while (remaining_frames > 0 && !waveform.empty()) {
		accumulator += waveform.front();
		waveform.pop_front();
		waveform.push_back(0.0f);

		render_buf.emplace_back(accumulator);
		--remaining_frames;

		tally_of_silence = (std::abs(accumulator) > 1.0f)
		                         ? 0
		                         : tally_of_silence + 1;

		accumulator *= SincAmplitudeFade;
	}

	// Pad with silence if waveform deque ran out
	if (remaining_frames > 0) {
		prev_amplitude = NeutralAmplitude;
	}
	while (remaining_frames > 0) {
		render_buf.emplace_back(NeutralAmplitude);
		++tally_of_silence;
		--remaining_frames;
	}

	output_queue.NonblockingBulkEnqueue(render_buf);
}

void PcSpeakerImpulse::SetFilterState(const FilterState filter_state)
{
	assert(channel);

	if (filter_state == FilterState::On) {
		constexpr auto HpOrder        = 3;
		constexpr auto HpCutoffFreqHz = 120;
		channel->ConfigureHighPassFilter(HpOrder, HpCutoffFreqHz);
		channel->SetHighPassFilter(FilterState::On);

		constexpr auto LpOrder        = 3;
		constexpr auto LpCutoffFreqHz = 4300;
		channel->ConfigureLowPassFilter(LpOrder, LpCutoffFreqHz);
		channel->SetLowPassFilter(FilterState::On);
	} else {
		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}
}

bool PcSpeakerImpulse::TryParseAndSetCustomFilter(const std::string& filter_choice)
{
	assert(channel);
	return channel->TryParseAndSetCustomFilter(filter_choice);
}

PcSpeakerImpulse::PcSpeakerImpulse()
{
	static_assert(SampleRateHz > 0 && SampleRateHz % 8000 == 0,
	              "Sample rate must be a multiple of 8000 to avoid artifacts");

	InitializeLut();
	waveform.resize(WaveformSize, 0.0f);

	constexpr bool Stereo      = false;
	constexpr bool SignedData  = true;
	constexpr bool NativeOrder = true;

	const auto callback = [this](int frames) {
		MIXER_PullFromQueueCallback<PcSpeakerImpulse, float, Stereo, SignedData, NativeOrder>(
		        frames, this);
	};

	channel = MIXER_AddChannel(callback,
	                           SampleRateHz,
	                           DeviceName,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::Synthesizer});
	assert(channel);

	channel->SetPeakAmplitude(PositiveAmplitude);

	LOG_MSG("%s: Initialised %s model", DeviceName, ModelName);
}

PcSpeakerImpulse::~PcSpeakerImpulse()
{
	LOG_MSG("%s: Shutting down %s model", DeviceName, ModelName);

	assert(channel);
	MIXER_DeregisterChannel(channel);
}
