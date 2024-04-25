/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#ifndef DOSBOX_LPT_DAC_H
#define DOSBOX_LPT_DAC_H

#include "dosbox.h"

#include <queue>
#include <set>
#include <string_view>

#include "inout.h"
#include "lpt.h"
#include "mixer.h"

// Provides mandatory scafolding for derived LPT DAC devices
class LptDac {
public:
	LptDac(const std::string_view name, const uint16_t channel_rate_hz,
	       std::set<ChannelFeature> extra_features = {});

	virtual ~LptDac();

	// public interfaces
	virtual void ConfigureFilters(const FilterState state) = 0;
	virtual void BindToPort(const io_port_t lpt_port)      = 0;

	bool TryParseAndSetCustomFilter(const std::string& filter_choice);

	LptDac() = delete;

	// prevent copying
	LptDac(const LptDac&) = delete;

	// prevent assignment
	LptDac& operator=(const LptDac&) = delete;

protected:
	// Base LPT DAC functionality
	virtual AudioFrame Render() = 0;
	void RenderUpToNow();
	void AudioCallback(const uint16_t requested_frames);
	std::queue<AudioFrame> render_queue = {};

	MixerChannelPtr channel = {};

	double last_rendered_ms = 0.0;
	double ms_per_frame     = 0.0;

	std::string dac_name = {};

	// All LPT devices support data write, status read, and control write
	void BindHandlers(const io_port_t lpt_port, const io_write_f write_data,
	                  const io_read_f read_status,
	                  const io_write_f write_control);

	IO_WriteHandleObject data_write_handler    = {};
	IO_ReadHandleObject status_read_handler    = {};
	IO_WriteHandleObject control_write_handler = {};

	uint8_t data_reg = Mixer_GetSilentDOSSample<uint8_t>();

	LptStatusRegister status_reg   = {};
	LptControlRegister control_reg = {};
};

#endif
