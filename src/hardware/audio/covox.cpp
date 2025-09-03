// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/covox.h"

#include <cassert>

#include "utils/checks.h"

CHECK_NARROWING();

void Covox::BindToPort(const io_port_t lpt_port)
{
	using namespace std::placeholders;

	const auto write_data  = std::bind(&Covox::WriteData, this, _1, _2, _3);
	const auto read_status = std::bind(&Covox::ReadStatus, this, _1, _2);
	const auto write_control = std::bind(&Covox::WriteControl, this, _1, _2, _3);

	BindHandlers(lpt_port, write_data, read_status, write_control);

	LOG_MSG("%s: Initialised Covox Speech Thing on LPT port %03xh",
	        ChannelName::CovoxDac,
	        lpt_port);
}

void Covox::ConfigureFilters(const FilterState state)
{
	assert(channel);
	if (state == FilterState::On) {
		constexpr auto lp_order          = 2;
		constexpr auto lp_cutoff_freq_hz = 9000;

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
