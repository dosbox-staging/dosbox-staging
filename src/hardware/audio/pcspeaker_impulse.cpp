// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2011  ripa, from vogons.org
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// NOTE: a lot of this code assumes that the callback is called every emulated
// millisecond

#include "pcspeaker_impulse.h"

#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

// Comment out the following to use the reference implementation.
#define USE_LOOKUP_TABLES 1

void PcSpeakerImpulse::AddPITOutput(const float index)
{
	if (prev_port_b.speaker_output) {
		AddImpulse(index, pit.amplitude);
	}
}

void PcSpeakerImpulse::ForwardPIT(const float new_index)
{
	auto passed = new_index - pit.last_index;

	auto delay_base = pit.last_index;

	pit.last_index = new_index;

	switch (pit.mode) {
	case PitMode::Inactive: return;

	case PitMode::InterruptOnTerminalCount:
		if (pit.index >= pit.max_ms) {
			/// counter reached zero before previous call so do nothing
			return;
		}
		pit.index += passed;
		if (pit.index >= pit.max_ms) {
			// counter reached zero between previous and this call
			const auto delay = delay_base + pit.max_ms - pit.index + passed;
			pit.amplitude    = positive_amplitude;
			AddPITOutput(delay);
		}
		return;

	case PitMode::OneShot:
		if (pit.mode1_waiting_for_counter) {
			// assert output amplitude is high
			return; // counter not written yet
		}
		if (pit.mode1_waiting_for_trigger) {
			// assert output amplitude is high
			return; // no pulse yet
		}
		if (pit.index >= pit.max_ms) {
			// counter reached zero before previous call so do nothing
			return;
		}
		pit.index += passed;
		if (pit.index >= pit.max_ms) {
			// counter reached zero between previous and this call
			const auto delay = delay_base + pit.max_ms - pit.index + passed;
			pit.amplitude    = positive_amplitude;
			AddPITOutput(delay);
			// finished with this pulse
			pit.mode1_waiting_for_trigger = 1;
		}
		return;

	case PitMode::RateGenerator:
	case PitMode::RateGeneratorAlias:
		while (passed > 0) {
			// passed the initial low cycle?
			if (pit.index >= pit.half_ms) {
				// Start a new low cycle
				if ((pit.index + passed) >= pit.max_ms) {
					const auto delay = pit.max_ms - pit.index;
					delay_base += delay;
					passed -= delay;
					pit.amplitude = negative_amplitude;
					AddPITOutput(delay_base);
					pit.index = 0;
				} else {
					pit.index += passed;
					return;
				}
			} else {
				if ((pit.index + passed) >= pit.half_ms) {
					const auto delay = pit.half_ms - pit.index;
					delay_base += delay;
					passed -= delay;
					pit.amplitude = positive_amplitude;
					AddPITOutput(delay_base);
					pit.index = pit.half_ms;
				} else {
					pit.index += passed;
					return;
				}
			}
		}
		break;

	case PitMode::SquareWave:
	case PitMode::SquareWaveAlias:
		if (!pit.mode3_counting)
			break;
		while (passed > 0) {
			// Determine where in the wave we're located
			if (pit.index >= pit.half_ms) {
				if ((pit.index + passed) >= pit.max_ms) {
					const auto delay = pit.max_ms - pit.index;
					delay_base += delay;
					passed -= delay;
					pit.amplitude = positive_amplitude;
					AddPITOutput(delay_base);
					pit.index = 0;
					// Load the new count
					pit.max_ms  = pit.new_max_ms;
					pit.half_ms = pit.new_half_ms;
				} else {
					pit.index += passed;
					return;
				}
			} else {
				if ((pit.index + passed) >= pit.half_ms) {
					const auto delay = pit.half_ms - pit.index;
					delay_base += delay;
					passed -= delay;
					pit.amplitude = negative_amplitude;
					AddPITOutput(delay_base);
					pit.index = pit.half_ms;
					// Load the new count
					pit.max_ms  = pit.new_max_ms;
					pit.half_ms = pit.new_half_ms;
				} else {
					pit.index += passed;
					return;
				}
			}
		}
		break;

	case PitMode::SoftwareStrobe:
		if (pit.index < pit.max_ms) {
			// Check if we're gonna pass the end this block
			if (pit.index + passed >= pit.max_ms) {
				const auto delay = pit.max_ms - pit.index;
				delay_base += delay;
				// passed -= delay; // not currently used
				pit.amplitude = negative_amplitude;
				// No new events unless reprogrammed
				AddPITOutput(delay_base);
				pit.index = pit.max_ms;
			} else
				pit.index += passed;
		}
		break;
	default:
		// other modes not implemented
		break;
	}
}

void PcSpeakerImpulse::SetPITControl(const PitMode pit_mode)
{
	const auto new_index = static_cast<float>(PIC_TickIndex());
	ForwardPIT(new_index);
#ifdef SPKR_DEBUGGING
	LOG_INFO("PCSPEAKER: %f pit command: %s", PIC_FullIndex(), pit_mode_to_string(pit_mode));
#endif
	// TODO: implement all modes
	switch (pit_mode) {
	case PitMode::OneShot:
		pit.mode      = pit_mode;
		pit.amplitude = positive_amplitude;

		pit.mode1_waiting_for_counter = 1;
		pit.mode1_waiting_for_trigger = 0;
		break;

	case PitMode::SquareWave:
	case PitMode::SquareWaveAlias:
		pit.mode      = pit_mode;
		pit.amplitude = positive_amplitude;

		pit.mode3_counting = false;
		break;

	default: return;
	}
	AddPITOutput(new_index);
}

void PcSpeakerImpulse::SetCounter(const int cntr, const PitMode pit_mode)
{
#ifdef SPKR_DEBUGGING
	LOG_INFO("PCSPEAKER: %f counter: %u, mode: %s", PIC_FullIndex(), cntr, pit_mode_to_string(pit_mode));
#endif
	const auto new_index = static_cast<float>(PIC_TickIndex());

	const auto duration_of_count_ms = ms_per_pit_tick * static_cast<float>(cntr);
	ForwardPIT(new_index);

	switch (pit_mode) {
	case PitMode::InterruptOnTerminalCount:
		// used with "realsound" (PWM)
		pit.index = 0;

		pit.amplitude = negative_amplitude;
		pit.max_ms    = duration_of_count_ms;

		AddPITOutput(new_index);
		break;

	case PitMode::OneShot: // used by Star Control 1
		pit.mode1_pending_max = duration_of_count_ms;
		if (pit.mode1_waiting_for_counter) {
			// assert output amplitude is high
			pit.mode1_waiting_for_counter = 0;
			pit.mode1_waiting_for_trigger = 1;
		}
		break;

	// Single cycle low, rest low high generator
	case PitMode::RateGenerator:
	case PitMode::RateGeneratorAlias:
		pit.index     = 0;
		pit.amplitude = negative_amplitude;

		AddPITOutput(new_index);

		pit.max_ms  = duration_of_count_ms;
		pit.half_ms = ms_per_pit_tick;
		break;

	case PitMode::SquareWave:
	case PitMode::SquareWaveAlias:
		if (cntr < minimum_counter) {
			// avoid breaking digger music
			pit.amplitude = positive_amplitude;
			pit.mode      = PitMode::Inactive;

			AddPITOutput(new_index);
			return;
		}
		pit.new_max_ms  = duration_of_count_ms;
		pit.new_half_ms = pit.new_max_ms / 2;
		if (!pit.mode3_counting) {
			pit.index = 0;

			pit.max_ms  = pit.new_max_ms;
			pit.half_ms = pit.new_half_ms;
			if (prev_port_b.timer2_gating) {
				pit.mode3_counting = true;
				// probably not necessary
				pit.amplitude = positive_amplitude;
				AddPITOutput(new_index);
			}
		}
		break;

	case PitMode::SoftwareStrobe:
		pit.amplitude = positive_amplitude;
		AddPITOutput(new_index);
		pit.index  = 0;
		pit.max_ms = duration_of_count_ms;
		break;

	default:
#ifdef SPKR_DEBUGGING
		LOG_WARNING("PCSPEAKER: Unhandled speaker PIT mode: '%s' at %f", pit_mode_to_string(pit_mode), PIC_FullIndex());
#endif
		return;
	}
	pit.mode = pit_mode;
}

void PcSpeakerImpulse::SetType(const PpiPortB &port_b)
{
#ifdef SPKR_DEBUGGING
	LOG_INFO("PCSPEAKER: %f output: %s, clock gate %s",
	         PIC_FullIndex(),
	         port_b.speaker_output ? "pit" : "forced low",
	         port_b.timer2_gating ? "on" : "off");
#endif
	const auto new_index = static_cast<float>(PIC_TickIndex());
	ForwardPIT(new_index);
	// pit clock gate enable rising edge is a trigger
	const bool pit_trigger = !prev_port_b.timer2_gating && port_b.timer2_gating;

	prev_port_b.data = port_b.data;
	if (pit_trigger) {
		switch (pit.mode) {
		case PitMode::OneShot:
			if (pit.mode1_waiting_for_counter) {
				// assert output amplitude is high
				break;
			}
			pit.amplitude = negative_amplitude;
			pit.index     = 0;
			pit.max_ms    = pit.mode1_pending_max;

			pit.mode1_waiting_for_trigger = 0;
			break;

		case PitMode::SquareWave:
		case PitMode::SquareWaveAlias:
			pit.mode3_counting = true;
			// pit.new_max_ms = pit.new_max_ms; // typo or bug?
			pit.index       = 0;
			pit.max_ms      = pit.new_max_ms;
			pit.new_half_ms = pit.new_max_ms / 2;
			pit.half_ms     = pit.new_half_ms;
			pit.amplitude   = positive_amplitude;
			break;

		default:
			// TODO: implement other modes
			break;
		}
	} else if (!port_b.timer2_gating) {
		switch (pit.mode) {
		case PitMode::OneShot:
			// gate amplitude does not affect mode1
			break;

		case PitMode::SquareWave:
		case PitMode::SquareWaveAlias:
			// low gate forces pit output high
			pit.amplitude      = positive_amplitude;
			pit.mode3_counting = false;
			break;

		default:
			// TODO: implement other modes
			break;
		}
	}
	if (port_b.speaker_output) {
		AddImpulse(new_index, pit.amplitude);
	} else {
		AddImpulse(new_index, negative_amplitude);
	}
}

// TODO: check if this is accurate
static double sinc(const double t)
{
	double result = 1.0;

	constexpr auto sinc_accuracy = 20;
	for (auto k = 1; k < sinc_accuracy; ++k) {
		result *= cos(t / pow(2.0, k));
	}
	return result;
}

float PcSpeakerImpulse::CalcImpulse(const double t) const
{
	// raised-cosine-windowed sinc function
	const double fs = sample_rate_hz;
	const auto fc   = fs / (2 + static_cast<double>(cutoff_margin));
	const auto q    = static_cast<double>(sinc_filter_quality);
	if ((0 < t) && (t * fs < q)) {
		const auto window    = 1.0 + cos(2 * fs * M_PI * (q / (2 * fs) - t) / q);
		const auto amplitude = window * (sinc(2 * fc * M_PI * (t - q / (2 * fs)))) / 2.0;
		return static_cast<float>(amplitude);
	} else
		return 0.0f;
}

void PcSpeakerImpulse::AddImpulse(float index, const int16_t amplitude)
{
	if (channel->WakeUp())
		pit.prev_amplitude = neutral_amplitude;

	// Did the amplitude change?
	if (amplitude == pit.prev_amplitude)
		return;

	pit.prev_amplitude = amplitude;
	// Make sure the time index is valid
	index = clamp(index, 0.0f, 1.0f);

#ifdef USE_LOOKUP_TABLES
	// Use pre-calculated sinc lookup tables
	const auto samples_in_impulse = index * sample_rate_per_ms;
	auto phase = static_cast<int>(samples_in_impulse * sinc_oversampling_factor) %
	             sinc_oversampling_factor;
	auto offset = static_cast<int>(samples_in_impulse);
	if (phase != 0) {
		offset++;
		phase = sinc_oversampling_factor - phase;
	}

	for (auto i = 0; i < sinc_filter_quality; ++i) {
		const auto wave_i    = check_cast<uint16_t>(offset + i);
		const auto impulse_i = check_cast<uint16_t>(
		        phase + i * sinc_oversampling_factor);
		waveform_deque.at(wave_i) += amplitude * impulse_lut.at(impulse_i);
	}
}

#else
	// Mathematically intensive reference implementation
	const auto portion_of_ms = static_cast <double>(index) / MillisInSecond;
	for (size_t i = 0; i < waveform_deque.size(); ++i) {
		const auto impulse_time = static_cast<double>(i) / sample_rate_hz -
		                          portion_of_ms;

		waveform_deque[i] += amplitude * CalcImpulse(impulse_time);
	}
}
#endif

void PcSpeakerImpulse::PicCallback(const int requested_frames)
{
	ForwardPIT(1.0f);
	pit.last_index = 0;
	int remaining_frames = requested_frames;

	static float accumulator = 0;
	while (remaining_frames > 0 && waveform_deque.size()) {
		// Pop the first sample off the waveform
		accumulator += waveform_deque.front();
		waveform_deque.pop_front();
		waveform_deque.push_back(0.0f);

		// std::move only used here because it won't compile without
		// This is just a float so it's safe to use afterwards
		output_queue.NonblockingEnqueue(std::move(accumulator));
		--remaining_frames;

		// Keep a tally of sequential silence so we can sleep the channel
		tally_of_silence = fabsf(accumulator) > 1.0f
		                         ? 0
		                         : tally_of_silence + 1;

		// Scale down the running volume amplitude. Eventually it will
		// hit 0 if no other waveforms are generated.
		accumulator *= sinc_amplitude_fade;
	}

	// Write silence if the waveform deque ran out
	if (remaining_frames)
		pit.prev_amplitude = neutral_amplitude;

	while (remaining_frames) {
		output_queue.NonblockingEnqueue(neutral_amplitude);
		++tally_of_silence;
		--remaining_frames;
	}
}

void PcSpeakerImpulse::InitializeImpulseLUT()
{
	assert(impulse_lut.size() == sinc_filter_width);

	for (auto i = 0u; i < sinc_filter_width; ++i) {
		impulse_lut[i] = CalcImpulse(i / (static_cast<double>(sample_rate_hz) *
		                                  sinc_oversampling_factor));
	}
}

void PcSpeakerImpulse::SetFilterState(const FilterState filter_state)
{
	assert(channel);

	// Setup filters
	if (filter_state == FilterState::On) {
		// The filters are meant to emulate the bandwidth limited
		// sound of the small PC speaker. This more accurately
		// reflects people's actual experience of the PC speaker
		// sound than the raw unfiltered output, and it's a lot
		// more pleasant to listen to, especially in headphones.
		constexpr auto hp_order          = 3;
		constexpr auto hp_cutoff_freq_hz = 120;
		channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq_hz);
		channel->SetHighPassFilter(FilterState::On);

		constexpr auto lp_order          = 3;
		constexpr auto lp_cutoff_freq_hz = 4300;
		channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq_hz);
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
	// The implementation is tuned to working with sample rates that are
	// multiples of 8000, such as 8 Khz, 16 Khz, or 32 Khz. Anything besides
	// these will produce unwanted artifacts.
	static_assert(sample_rate_hz >= 8000, "Sample rate must be at least 8 kHz");
	static_assert(sample_rate_hz % 1000 == 0,
	              "Sample rate must be a multiple of 1000");

	InitializeImpulseLUT();

	// Size the waveform queue
	constexpr auto waveform_size = sinc_filter_quality + sample_rate_per_ms;
	waveform_deque.resize(waveform_size, 0.0f);

	// Register the sound channel
	constexpr bool Stereo = false;
	constexpr bool SignedData = true;
	constexpr bool NativeOrder = true;
	const auto callback = std::bind(MIXER_PullFromQueueCallback<PcSpeakerImpulse, float, Stereo, SignedData, NativeOrder>,
	                                std::placeholders::_1,
	                                this);

	channel = MIXER_AddChannel(callback,
	                           sample_rate_hz,
	                           device_name,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::Synthesizer});
	assert(channel);

	LOG_MSG("%s: Initialised %s model", device_name, model_name);

	channel->SetPeakAmplitude(positive_amplitude);
}

PcSpeakerImpulse::~PcSpeakerImpulse()
{
	LOG_MSG("%s: Shutting down %s model", device_name, model_name);

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);
}
