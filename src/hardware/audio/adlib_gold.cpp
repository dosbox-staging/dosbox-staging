// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "adlib_gold.h"

#include "checks.h"
#include "math_utils.h"

CHECK_NARROWING();

// #define DEBUG_ADLIB_GOLD

// Yamaha YM7128B Surround Processor emulation
// -------------------------------------------

SurroundProcessor::SurroundProcessor(const int sample_rate_hz)
{
	assert(sample_rate_hz >= 10);

	YM7128B_ChipIdeal_Ctor(&chip);
	YM7128B_ChipIdeal_Setup(&chip, sample_rate_hz);
	YM7128B_ChipIdeal_Reset(&chip);
	YM7128B_ChipIdeal_Start(&chip);
}

SurroundProcessor::~SurroundProcessor()
{
	YM7128B_ChipIdeal_Stop(&chip);
	YM7128B_ChipIdeal_Dtor(&chip);
}

union SurroundControlReg {
	uint8_t data = 0;
	bit_view<0, 1> din; // serial data
	bit_view<1, 1> sci; // bit clock
	bit_view<2, 1> a0;  // word clock
};

void SurroundProcessor::ControlWrite(const uint8_t val)
{
	SurroundControlReg reg = {val};

	// Change register data at the falling edge of 'a0' word clock
	if (control_state.a0 && !reg.a0) {
#ifdef DEBUG_ADLIB_GOLD
		LOG_DEBUG("ADLIBGOLD: Surround: Write control register %d, data: %d",
		          control_state.addr,
		          control_state.data);
#endif

		YM7128B_ChipIdeal_Write(&chip, control_state.addr, control_state.data);
	} else {
		// Data is sent in serially through 'din' in MSB->LSB order,
		// synchronised by the 'sci' bit clock. Data should be read on
		// the rising edge of 'sci'.
		if (!control_state.sci && reg.sci) {
			// The 'a0' word clock determines the type of the data.
			if (reg.a0) {
				// Data cycle
				control_state.data = static_cast<uint8_t>(
				                             control_state.data << 1) |
				                     reg.din;
			} else {
				// Address cycle
				control_state.addr = static_cast<uint8_t>(
				                             control_state.addr << 1) |
				                     reg.din;
			}
		}
	}

	control_state.sci = reg.sci;
	control_state.a0  = reg.a0;
}

AudioFrame SurroundProcessor::Process(const AudioFrame frame)
{
	YM7128B_ChipIdeal_Process_Data data = {};

	data.inputs[0] = frame.left + frame.right;

	YM7128B_ChipIdeal_Process(&chip, &data);

	return {data.outputs[0], data.outputs[1]};
}

// Philips Semiconductors TDA8425 hi-fi stereo audio processor emulation
// ---------------------------------------------------------------------

StereoProcessor::StereoProcessor(const int _sample_rate_hz)
        : sample_rate_hz(_sample_rate_hz)
{
	assert(sample_rate_hz > 0);

	constexpr auto allpass_freq_hz = 400.0;
	constexpr auto q_factor        = 1.7;
	allpass.setup(sample_rate_hz, allpass_freq_hz, q_factor);

	Reset();
}

void StereoProcessor::SetLowShelfGain(const double gain_db)
{
	constexpr auto cutoff_freq_hz = 400.0;
	constexpr auto slope          = 0.5;
	for (auto& f : lowshelf) {
		f.setup(sample_rate_hz, cutoff_freq_hz, gain_db, slope);
	}
}

void StereoProcessor::SetHighShelfGain(const double gain_db)
{
	constexpr auto cutoff_freq_hz = 2500.0;
	constexpr auto slope          = 0.5;
	for (auto& f : highshelf) {
		f.setup(sample_rate_hz, cutoff_freq_hz, gain_db, slope);
	}
}

StereoProcessor::~StereoProcessor() = default;

constexpr auto volume_0db_value       = 60;
constexpr auto shelf_filter_0db_value = 6;

void StereoProcessor::Reset()
{
	ControlWrite(StereoProcessorControlReg::VolumeLeft, volume_0db_value);
	ControlWrite(StereoProcessorControlReg::VolumeRight, volume_0db_value);
	ControlWrite(StereoProcessorControlReg::Bass, shelf_filter_0db_value);
	ControlWrite(StereoProcessorControlReg::Treble, shelf_filter_0db_value);

	StereoProcessorSwitchFunctions sf = {};

	sf.source_selector = static_cast<uint8_t>(
	        StereoProcessorSourceSelector::Stereo1);

	sf.stereo_mode = static_cast<uint8_t>(StereoProcessorStereoMode::LinearStereo);

	ControlWrite(StereoProcessorControlReg::SwitchFunctions, sf.data);
}

void StereoProcessor::ControlWrite(const StereoProcessorControlReg reg,
                                   const uint8_t data)
{
	auto calc_volume_gain = [](const int value) {
		constexpr auto min_gain_db = -128.0f;
		constexpr auto max_gain_db = 6.0f;
		constexpr auto step_db     = 2.0f;

		auto val     = static_cast<float>(value - volume_0db_value);
		auto gain_db = clamp(val * step_db, min_gain_db, max_gain_db);
		return decibel_to_gain(gain_db);
	};

	auto calc_filter_gain_db = [](const int value) {
		constexpr auto min_gain_db = -12.0;
		constexpr auto max_gain_db = 15.0;
		constexpr auto step_db     = 3.0;

		auto val = value - shelf_filter_0db_value;
		return clamp(val * step_db, min_gain_db, max_gain_db);
	};

	constexpr auto volume_control_width = 6;
	constexpr auto volume_control_mask  = (1 << volume_control_width) - 1;

	constexpr auto filter_control_width = 4;
	constexpr auto filter_control_mask  = (1 << filter_control_width) - 1;

	switch (reg) {
	case StereoProcessorControlReg::VolumeLeft: {
		const auto value = data & volume_control_mask;
		gain.left        = calc_volume_gain(value);

#ifdef DEBUG_ADLIB_GOLD
		LOG_DEBUG("ADLIBGOLD: Stereo: Final left volume set to %.2fdB (value %d)",
		          gain.left,
		          value);
#endif
	} break;

	case StereoProcessorControlReg::VolumeRight: {
		const auto value = data & volume_control_mask;
		gain.right       = calc_volume_gain(value);

#ifdef DEBUG_ADLIB_GOLD
		LOG_DEBUG("ADLIBGOLD: Stereo: Final right volume set to %.2fdB (value %d)",
		          gain.right,
		          value);
#endif
	} break;

	case StereoProcessorControlReg::Bass: {
		const auto value   = data & filter_control_mask;
		const auto gain_db = calc_filter_gain_db(value);
		SetLowShelfGain(gain_db);

#ifdef DEBUG_ADLIB_GOLD
		LOG_DEBUG("ADLIBGOLD: Stereo: Bass gain set to %.2fdB (value %d)",
		          gain_db,
		          value);
#endif
	} break;

	case StereoProcessorControlReg::Treble: {
		const auto value = data & filter_control_mask;
		// Additional treble boost to make the emulated sound more
		// closely resemble real hardware recordings.
		constexpr auto extra_treble = 1;
		const auto gain_db = calc_filter_gain_db(value + extra_treble);
		SetHighShelfGain(gain_db);

#ifdef DEBUG_ADLIB_GOLD
		LOG_DEBUG("ADLIBGOLD: Stereo: Treble gain set to %.2fdB (value %d)",
		          gain_db,
		          value);
#endif
	} break;

	case StereoProcessorControlReg::SwitchFunctions: {
		auto sf = StereoProcessorSwitchFunctions{data};

		source_selector = StereoProcessorSourceSelector(
		        static_cast<uint8_t>(sf.source_selector));
		stereo_mode = StereoProcessorStereoMode(
		        static_cast<uint8_t>(sf.stereo_mode));

#ifdef DEBUG_ADLIB_GOLD
		LOG_DEBUG("ADLIBGOLD: Stereo: Source selector set to %d, stereo mode set to %d",
		          static_cast<int>(source_selector),
		          static_cast<int>(stereo_mode));
#endif
	} break;
	}
}

AudioFrame StereoProcessor::ProcessSourceSelection(const AudioFrame frame)
{
	switch (source_selector) {
	case StereoProcessorSourceSelector::SoundA1:
	case StereoProcessorSourceSelector::SoundA2:
		return {frame.left, frame.left};

	case StereoProcessorSourceSelector::SoundB1:
	case StereoProcessorSourceSelector::SoundB2:
		return {frame.right, frame.right};

	case StereoProcessorSourceSelector::Stereo1:
	case StereoProcessorSourceSelector::Stereo2:
	default:
		// Dune sends an invalid source selector value of 0 during the
		// intro; we'll just revert to stereo operation
		return frame;
	}
}

AudioFrame StereoProcessor::ProcessShelvingFilters(const AudioFrame frame)
{
	AudioFrame out_frame = {};

	for (std::size_t i = 0; i < 2; ++i) {
		out_frame[i] = lowshelf[i].filter(frame[i]);
		out_frame[i] = highshelf[i].filter(out_frame[i]);
	}
	return out_frame;
}

AudioFrame StereoProcessor::ProcessStereoProcessing(const AudioFrame frame)
{
	AudioFrame out_frame = {};

	switch (stereo_mode) {
	case StereoProcessorStereoMode::ForcedMono: {
		const auto m    = frame.left + frame.right;
		out_frame.left  = m;
		out_frame.right = m;
	} break;

	case StereoProcessorStereoMode::PseudoStereo:
		out_frame.left  = allpass.filter(frame.left);
		out_frame.right = frame.right;
		break;

	case StereoProcessorStereoMode::SpatialStereo: {
		constexpr auto crosstalk_percentage = 52.0f;
		constexpr auto k = crosstalk_percentage / 100.0f;
		const auto l     = frame.left;
		const auto r     = frame.right;
		out_frame.left   = l + (l - r) * k;
		out_frame.right  = r + (r - l) * k;
	} break;

	case StereoProcessorStereoMode::LinearStereo:
	default: out_frame = frame; break;
	}
	return out_frame;
}

AudioFrame StereoProcessor::Process(const AudioFrame frame)
{
	auto out_frame = ProcessSourceSelection(frame);
	out_frame      = ProcessShelvingFilters(out_frame);
	out_frame      = ProcessStereoProcessing(out_frame);

	out_frame.left *= gain.left;
	out_frame.right *= gain.right;

	return out_frame;
}

// AdLib Gold module
// -----------------

AdlibGold::AdlibGold(const int sample_rate_hz)
        : surround_processor(nullptr),
          stereo_processor(nullptr)
{
	surround_processor = std::make_unique<SurroundProcessor>(sample_rate_hz);
	stereo_processor   = std::make_unique<StereoProcessor>(sample_rate_hz);
}

AdlibGold::~AdlibGold() = default;

void AdlibGold::StereoControlWrite(const StereoProcessorControlReg reg,
                                   const uint8_t data)
{
	stereo_processor->ControlWrite(reg, data);
}

void AdlibGold::SurroundControlWrite(const uint8_t val)
{
	surround_processor->ControlWrite(val);
}

void AdlibGold::Process(const int16_t* in, const int frames, float* out)
{
	auto frames_remaining = frames;

	while (frames_remaining--) {
		AudioFrame frame = {static_cast<float>(in[0]),
		                    static_cast<float>(in[1])};

		const auto wet = surround_processor->Process(frame);

		// Additional wet signal level boost to make the emulated
		// sound more closely resemble real hardware recordings.
		constexpr auto wet_boost = 1.8f;
		frame.left += wet.left * wet_boost;
		frame.right += wet.right * wet_boost;

		frame = stereo_processor->Process(frame);

		out[0] = frame.left;
		out[1] = frame.right;
		in += 2;
		out += 2;
	}
}

