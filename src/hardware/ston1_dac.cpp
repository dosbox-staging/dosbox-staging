/*
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ston1_dac.h"

#include <cassert>

#include "checks.h"

CHECK_NARROWING();
void StereoOn1::BindToPort(const io_port_t lpt_port)
{
	using namespace std::placeholders;
	const auto write_data = std::bind(&StereoOn1::WriteData, this, _1, _2, _3);
	const auto read_status = std::bind(&StereoOn1::ReadStatus, this, _1, _2);
	const auto write_control = std::bind(&StereoOn1::WriteControl, this, _1, _2, _3);
	BindHandlers(lpt_port, write_data, read_status, write_control);
	LOG_MSG("LPT_DAC: Initialised Stereo-On-1 DAC on LPT port %03xh", lpt_port);
}

void StereoOn1::ConfigureFilters(const FilterState state)
{
	assert(channel);
 	if (state == FilterState::On) {
		constexpr uint8_t lp_order = 2;
		const uint16_t lp_cutoff_freq = 9000;
		channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq);
	}
	channel->SetLowPassFilter(state);
}

AudioFrame StereoOn1::Render()
{
	const float left = lut_u8to16[stereo_data[0]];
	const float right = lut_u8to16[stereo_data[1]];
	return {left, right};
}

void StereoOn1::WriteData(const io_port_t, const io_val_t data, const io_width_t)
{
	data_reg = check_cast<uint8_t>(data);
}

uint8_t StereoOn1::ReadStatus(const io_port_t, const io_width_t)
{
	const LptStatusRegister data_status = {data_reg};

	// The Stereo-On-1 DAC ties pin 9 to 11 for detection, which
	// is the last bit of the data inversely tied to the last bit of the
	// status. Ref: modplay 2.x hardware documention.
	status_reg.busy = !data_status.busy;
	return status_reg.data;
}

void StereoOn1::WriteControl(const io_port_t, const io_val_t value, const io_width_t)
{
	RenderUpToNow();

	const auto new_control = LptControlRegister{check_cast<uint8_t>(value)};

	// Write data to the left channel
	if (control_reg.auto_lf && !new_control.auto_lf)
		stereo_data[0] = data_reg;

	// Write data to the right channel
	if (control_reg.strobe && !new_control.strobe)
		stereo_data[1] = data_reg;

	control_reg.data = new_control.data;
}
