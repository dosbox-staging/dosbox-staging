/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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

#include "residfp/SID.h"

class Innovation {
public:
	void Open(const std::string_view model_choice,
	          const std::string_view clock_choice, int filter_strength_6581,
	          int filter_strength_8580, int port_choice,
	          const std::string& channel_filter_choice);

	void Close();
	~Innovation()
	{
		Close();
	}

private:
	bool MaybeRenderFrame(float &frame);
	void AudioCallback(const uint16_t requested_frames);
	uint8_t ReadFromPort(io_port_t port, io_width_t width);
	void RenderUpToNow();
	int16_t TallySilence(const int16_t sample);
	void WriteToPort(io_port_t port, io_val_t value, io_width_t width);

	// Managed objects
	MixerChannelPtr channel               = nullptr;
	IO_ReadHandleObject read_handler      = {};
	IO_WriteHandleObject write_handler    = {};
	std::unique_ptr<reSIDfp::SID> service = {};
	std::queue<float> fifo                = {};

	// Initial configuration
	double chip_clock            = 0.0;
	double ms_per_clock          = 0.0;
	io_port_t base_port          = 0;
	int idle_after_silent_frames = 0;

	// Runtime states
	double last_rendered_ms = 0.0;
	bool is_open            = false;
};

#endif
