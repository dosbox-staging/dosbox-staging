// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "disney.h"

#include <cassert>

#include "channel_names.h"
#include "checks.h"

CHECK_NARROWING();

Disney::Disney() : LptDac(ChannelName::DisneySoundSourceDac, UseMixerRate)
{
	// Prime the FIFO with a single silent sample
	fifo.emplace(data_reg);
}

void Disney::BindToPort(const io_port_t lpt_port)
{
	using namespace std::placeholders;

	// Register port handlers for 8-bit IO
	const auto write_data = std::bind(&Disney::WriteData, this, _1, _2, _3);
	const auto read_status = std::bind(&Disney::ReadStatus, this, _1, _2);
	const auto write_control = std::bind(&Disney::WriteControl, this, _1, _2, _3);
	BindHandlers(lpt_port, write_data, read_status, write_control);
	LOG_MSG("LPT_DAC: Initialised Disney Sound Source on LPT port %03xh", lpt_port);
}

void Disney::ConfigureFilters(const FilterState state)
{
	assert(channel);

	// Run the ZoH up-sampler at the higher mixer rate
	const auto mixer_rate_hz = channel->GetSampleRate();
	channel->SetZeroOrderHoldUpsamplerTargetRate(mixer_rate_hz);
	channel->SetResampleMethod(ResampleMethod::ZeroOrderHoldAndResample);

	// Pull audio frames from the Disney DAC at 7 kHz
	channel->SetSampleRate(DisneySampleRateHz);
	ms_per_frame = MillisInSecond / DisneySampleRateHz;

	if (state == FilterState::On) {
		// The filters are meant to emulate the Disney's bandwidth
		// limitations both by ear and spectrum analysis when compared
		// against LGR Oddware's recordings of an authentic Disney Sound
		// Source in ref: https://youtu.be/A1YThKmV2dk?t=1126

		constexpr auto hp_order          = 2;
		constexpr auto hp_cutoff_freq_hz = 100;
		channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq_hz);

		constexpr auto lp_order          = 2;
		constexpr auto lp_cutoff_freq_hz = 2000;
		channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq_hz);
	}
	channel->SetHighPassFilter(state);
	channel->SetLowPassFilter(state);
}

// Eight bit data sent to the D/A convener is loaded into a 16 level FIFO. Data
// is clocked from this FIFO at the fixed rate of 7 kHz +/- 5%.
AudioFrame Disney::Render()
{
	assert(fifo.size());
	const float sample = lut_u8to16[fifo.front()];
	if (fifo.size() > 1)
		fifo.pop();
	return {sample, sample};
}

bool Disney::IsFifoFull() const
{
	return fifo.size() >= MaxFifoSize;
}

void Disney::WriteData(const io_port_t, const io_val_t data, const io_width_t)
{
	data_reg = check_cast<uint8_t>(data);
}

uint8_t Disney::ReadStatus(const io_port_t, const io_width_t)
{
	// The Disney ACK's (active-low) when the FIFO has room
	status_reg.ack = IsFifoFull();
	return status_reg.data;
}

void Disney::WriteControl(const io_port_t, const io_val_t value, const io_width_t)
{
	RenderUpToNow();

	const auto new_control = LptControlRegister{check_cast<uint8_t>(value)};

	// The rising edge of the pulse on Pin 17 from the printer interface
	// is used to clock data into the FIFO. Note from diagram 1 that the
	// SELECT and INIT inputs to the D/A chip are isolated from pin 17 by
	// an RC lime constant. Ref:
	// https://archive.org/stream/dss-programmers-guide/dss-programmers-guide_djvu.txt

	if (!control_reg.select && new_control.select)
		if (!IsFifoFull())
			fifo.emplace(data_reg);

	control_reg.data = new_control.data;
}

Disney::~Disney()
{
	fifo = {};
}
