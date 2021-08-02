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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mixer.h"
#include "inout.h"
#include "rwqueue.h"
#include "../libs/residfp/SID.h"

class Innovation {
public:
	Innovation() : keep_rendering(false) {}
	void Open(const std::string &model_choice,
	          const std::string &clock_choice,
	          int filter_strength_6581,
	          int filter_strength_8580,
	          int port_choice);

	void Close();
	~Innovation() { Close(); }

private:
	using channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

	void Render();
	uint16_t GetRemainingSamples();
	void MixerCallBack(uint16_t requested_samples);
	uint8_t ReadFromPort(io_port_t port, io_width_t width);
	void WriteToPort(io_port_t port, uint8_t data, io_width_t width);

	// Managed objects
	channel_t channel{nullptr, MIXER_DelChannel};

	IO_ReadHandleObject read_handler = {};
	IO_WriteHandleObject write_handler = {};

	std::vector<int16_t> play_buffer = {};
	static constexpr auto num_buffers = 4;
	RWQueue<std::vector<int16_t>> playable{num_buffers};
	RWQueue<std::vector<int16_t>> backstock{num_buffers};
	std::thread renderer = {};
	std::mutex service_mutex = {};
	std::unique_ptr<reSIDfp::SID> service = {};
	std::atomic_bool keep_rendering = {};

	// Scalar members
	uint16_t base_port = 0;
	double chip_clock = 0;
	double sid_sample_rate = 0;
	size_t last_used = 0;
	uint16_t play_buffer_pos = 0;
	bool is_open = false;
};

#endif
