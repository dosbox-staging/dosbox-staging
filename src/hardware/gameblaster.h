/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#ifndef DOSBOX_GAMEBLASTER_H
#define DOSBOX_GAMEBLASTER_H

#include "dosbox.h"

#include <array>
#include <memory>
#include <queue>
#include <string_view>

#include "inout.h"
#include "mixer.h"

#include "mame/emu.h"
#include "mame/saa1099.h"


class GameBlaster {
public:
	void Open(const int port_choice, const std::string_view card_choice);
	void Close();
	~GameBlaster() { Close(); }

	using frame_t = std::array<int16_t, 2>;
private:
	// Autio rendering
	frame_t RenderOnce();
	double ConvertFramesToMs(const int frames) const;
	void RenderForMs(const double duration_ms);
	void AudioCallback(uint16_t requested_frames);
	void RenderUpToNow();

	// IO callbacks to the left SAA1099 device
	void WriteDataToLeftDevice(io_port_t port, io_val_t value, io_width_t width);
	void WriteControlToLeftDevice(io_port_t port, io_val_t value, io_width_t width);

	// IO callbacks to the right SAA1099 device
	void WriteDataToRightDevice(io_port_t port, io_val_t value, io_width_t width);
	void WriteControlToRightDevice(io_port_t port, io_val_t value, io_width_t width);

	// IO callbacks to the GameBlaster detection chip
	void WriteToDetectionPort(io_port_t port, io_val_t value, io_width_t width);
	uint8_t ReadFromDetectionPort(io_port_t port, io_width_t width) const;

	const char *CardName() const;

	// Managed objects
	mixer_channel_t channel = nullptr;
	IO_WriteHandleObject write_handlers[4] = {};
	IO_WriteHandleObject write_handler_for_detection = {};
	IO_ReadHandleObject read_handler_for_detection = {};
	std::unique_ptr<saa1099_device> devices[2] = {};
	std::queue<frame_t> fifo = {};

	// Initial configuration
	static constexpr auto chip_clock = 14318180 / 2;
	static constexpr auto frame_rate_hz = chip_clock / 256;
	static constexpr auto frame_rate_per_ms = frame_rate_hz / 1000.0;
	io_port_t base_port = 0;

	// Runtime states
	double last_render_time = 0;
	int unwritten_for_ms = 0;
	bool is_standalone_gameblaster = false;
	bool is_open = false;
	uint8_t cms_detect_register = 0xff;
};

#endif
