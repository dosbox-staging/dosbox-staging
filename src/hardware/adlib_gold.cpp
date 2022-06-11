/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022  The DOSBox Staging Team
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

#include "adlib_gold.h"
#include "bit_view.h"

#include "../libs/YM7128B_emu/YM7128B_emu.h"

// Yamaha YM7128B Surround Processor emulation
// -------------------------------------------

class SurroundProcessor {
public:
	SurroundProcessor(const uint16_t sample_rate);
	~SurroundProcessor();

	void ControlWrite(const uint8_t val) noexcept;
	AudioFrame Process(const AudioFrame &frame) noexcept;

	// prevent copying and assignment
	SurroundProcessor(const SurroundProcessor &) = delete;
	SurroundProcessor &operator=(const SurroundProcessor &) = delete;

private:
	YM7128B_ChipIdeal *chip = nullptr;

	struct {
		uint8_t sci  = 0;
		uint8_t a0   = 0;
		uint8_t addr = 0;
		uint8_t data = 0;
	} control_state = {};
};

SurroundProcessor::SurroundProcessor(const uint16_t sample_rate) : chip(nullptr)
{
	chip = (YM7128B_ChipIdeal *)malloc(sizeof(YM7128B_ChipIdeal));

	YM7128B_ChipIdeal_Ctor(chip);
	YM7128B_ChipIdeal_Setup(chip, sample_rate);
	YM7128B_ChipIdeal_Reset(chip);
	YM7128B_ChipIdeal_Start(chip);
}

SurroundProcessor::~SurroundProcessor()
{
	YM7128B_ChipIdeal_Stop(chip);
	YM7128B_ChipIdeal_Dtor(chip);
}

union SurroundControlReg {
	uint8_t data = 0;
	bit_view<0, 1> din; // serial data
	bit_view<1, 1> sci; // bit clock
	bit_view<2, 1> a0;  // word clock
};

void SurroundProcessor::ControlWrite(const uint8_t val) noexcept
{
	SurroundControlReg reg = {val};

	// Change register data at the falling edge of 'a0' word clock
	if (control_state.a0 && !reg.a0) {
//		DEBUG_LOG_MSG("ADLIBGOLD.SURROUND: Write control register %d, data: %d",
//		              control_state.addr,
//		              control_state.data);
		YM7128B_ChipIdeal_Write(chip, control_state.addr, control_state.data);
	} else {
		// Data is sent in serially through 'din' in MSB->LSB order,
		// synchronised by the 'sci' bit clock. Data should be read on
		// the rising edge of 'sci'.
		if (!control_state.sci && reg.sci) {
			// The 'a0' word clock determines the type of the data.
			if (reg.a0)
				// Data cycle
				control_state.data = (control_state.data << 1) |
				                     reg.din;
			else
				// Address cycle
				control_state.addr = (control_state.addr << 1) |
				                     reg.din;
		}
	}

	control_state.sci = reg.sci;
	control_state.a0  = reg.a0;
}

AudioFrame SurroundProcessor::Process(const AudioFrame &frame) noexcept
{
	YM7128B_ChipIdeal_Process_Data data = {};

	data.inputs[0] = frame.left + frame.right;

	YM7128B_ChipIdeal_Process(chip, &data);

	return {data.outputs[0], data.outputs[1]};
}


// Philips Semiconductors TDA8425 hi-fi stereo audio processor emulation
// ----------------------------------------------------------------------

class StereoProcessor {
public:
	StereoProcessor(const uint16_t sample_rate);
	~StereoProcessor();

	void Reset() noexcept;
	void ControlWrite(const TDA8425_Reg reg, const TDA8425_Register data) noexcept;
	AudioFrame Process(const AudioFrame &frame) noexcept;

	// prevent copying and assignment
	StereoProcessor(const StereoProcessor &) = delete;
	StereoProcessor &operator=(const StereoProcessor &) = delete;

private:
	TDA8425_Chip *chip = nullptr;
};

StereoProcessor::StereoProcessor(const uint16_t sample_rate) : chip(nullptr)
{
	chip = (TDA8425_Chip *)malloc(sizeof(TDA8425_Chip));

	TDA8425_Chip_Ctor(chip);
	TDA8425_Chip_Setup(chip,
	                   sample_rate,
	                   TDA8425_Pseudo_C1_Table[TDA8425_Pseudo_Preset_2],
	                   TDA8425_Pseudo_C2_Table[TDA8425_Pseudo_Preset_2],
	                   TDA8425_Tfilter_Mode_Disabled);
	TDA8425_Chip_Reset(chip);
	TDA8425_Chip_Start(chip);

	Reset();
}

StereoProcessor::~StereoProcessor()
{
	TDA8425_Chip_Stop(chip);
	TDA8425_Chip_Dtor(chip);
}

void StereoProcessor::Reset() noexcept
{
	constexpr auto volume_0db = 60;
	constexpr auto bass_0db   = 6;
	constexpr auto treble_0db = 6;

	ControlWrite(TDA8425_Reg_VL, volume_0db);
	ControlWrite(TDA8425_Reg_VR, volume_0db);
	ControlWrite(TDA8425_Reg_BA, bass_0db);
	ControlWrite(TDA8425_Reg_TR, treble_0db);

	constexpr auto stereo_output = TDA8425_Selector_Stereo_1;
	constexpr auto linear_stereo = TDA8425_Mode_LinearStereo
	                            << TDA8425_Reg_SF_STL;

	ControlWrite(TDA8425_Reg_SF, stereo_output & linear_stereo);
}

void StereoProcessor::ControlWrite(const TDA8425_Reg addr,
                                   const TDA8425_Register data) noexcept
{
	TDA8425_Chip_Write(chip, static_cast<TDA8425_Address>(addr), data);
}

AudioFrame StereoProcessor::Process(const AudioFrame &frame) noexcept
{
	TDA8425_Chip_Process_Data data = {};

	data.inputs[0][0] = frame.left;
	data.inputs[1][0] = frame.left;
	data.inputs[0][1] = frame.right;
	data.inputs[1][1] = frame.right;

	TDA8425_Chip_Process(chip, &data);

	return {data.outputs[0], data.outputs[1]};
}

// AdLib Gold module
// -----------------

AdlibGold::AdlibGold(const uint16_t sample_rate)
        : surround_processor(nullptr),
          stereo_processor(nullptr)
{
	surround_processor = std::make_unique<SurroundProcessor>(sample_rate);
	stereo_processor   = std::make_unique<StereoProcessor>(sample_rate);
}

AdlibGold::~AdlibGold() = default;

void AdlibGold::StereoControlWrite(const TDA8425_Reg reg,
                                   const TDA8425_Register data) noexcept
{
	stereo_processor->ControlWrite(reg, data);
}

void AdlibGold::SurroundControlWrite(const uint8_t val) noexcept
{
	surround_processor->ControlWrite(val);
}

void AdlibGold::Process(const int16_t *in, const uint32_t frames, float *out) noexcept
{
	auto frames_remaining = frames;
	while (frames_remaining--) {
		AudioFrame frame = {static_cast<float>(in[0]),
		                    static_cast<float>(in[1])};

		const auto wet = surround_processor->Process(frame);
		// Additional wet signal level boost to make the emulated sound
		// more closely resemble real hardware recordings.
		constexpr auto wet_boost = 1.6f;
		frame.left += wet.left * wet_boost;
		frame.right += wet.right * wet_boost;

		frame = stereo_processor->Process(frame);

		out[0] = frame.left;
		out[1] = frame.right;
		in += 2;
		out += 2;
	}
}

