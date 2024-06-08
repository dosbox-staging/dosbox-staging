/*
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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

#include "covox.h"

#include <cassert>

#include "checks.h"

CHECK_NARROWING();

void Covox::BindToPort(const io_port_t lpt_port)
{
	using namespace std::placeholders;
	const auto write_data  = std::bind(&Covox::WriteData, this, _1, _2, _3);
	const auto read_status = std::bind(&Covox::ReadStatus, this, _1, _2);
	const auto write_control = std::bind(&Covox::WriteControl, this, _1, _2, _3);
	BindHandlers(lpt_port, write_data, read_status, write_control);
	LOG_MSG("LPT_DAC: Initialised Covox Speech Thing on LPT port %03xh", lpt_port);
}

void Covox::ConfigureFilters(const FilterState state)
{
	assert(channel);
	if (state == FilterState::On) {
		constexpr uint8_t lp_order = 2;
		const uint16_t lp_cutoff_freq_hz = 9000;
		channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq_hz);
	}
	channel->SetLowPassFilter(state);
}

AudioFrame Covox::Render()
{
	const float sample = lut_u8to16[data_reg];
	return {sample, sample};
}

void Covox::WriteData(const io_port_t, const io_val_t data, const io_width_t)
{
	RenderUpToNow();
	data_reg = check_cast<uint8_t>(data);
}

uint8_t Covox::ReadStatus(const io_port_t, const io_width_t)
{
	return status_reg.data;
}
