/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_INNOVATION_H
#define DOSBOX_INNOVATION_H

#include "dosbox.h"

#include <memory>
#include <queue>
#include <string>

#include "mixer.h"
#include "inout.h"
#include "../libs/residfp/SID.h"

class Innovation {
public:
	void Open(const std::string &model_choice,
	          const std::string &clock_choice,
	          int filter_strength_6581,
	          int filter_strength_8580,
	          int port_choice);

	void Close();
	~Innovation() { Close(); }

private:
	void Render();
	double ConvertFramesToMs(const int samples);

	int16_t RenderOnce();
	void RenderForMs(const double duration_ms);

	void MixerCallBack(uint16_t requested_frames);
	uint8_t ReadFromPort(io_port_t port, io_width_t width);
	void WriteToPort(io_port_t port, io_val_t value, io_width_t width);

	// Managed objects
	mixer_channel_t channel = nullptr;

	IO_ReadHandleObject read_handler = {};
	IO_WriteHandleObject write_handler = {};

	std::unique_ptr<reSIDfp::SID> service = {};
	std::queue<int16_t> fifo = {};

	// Initial configuration
	io_port_t base_port = 0;
	double chip_clock = 0;
	double frame_rate_per_ms = 0;
	int idle_after_silent_frames = 0;

	// Runtime states
	double last_render_time = 0;
	int unwritten_for_ms = 0;
	int silent_frames = 0;
	bool is_enabled = false;
	bool is_open = false;
};

#endif
