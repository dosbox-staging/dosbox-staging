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

#ifndef DOSBOX_GAMEBLASTER_H
#define DOSBOX_GAMEBLASTER_H

#include "dosbox.h"

#include <array>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "inout.h"
#include "math_utils.h"
#include "mixer.h"
#include "support.h"

#include "mame/emu.h"
#include "mame/saa1099.h"

#include "residfp/resample/TwoPassSincResampler.h"

class GameBlaster {
public:
	void Open(const int port_choice, const std::string &card_choice,
	          const std::string &filter_choice);

	void Close();
	~GameBlaster() { Close(); }

private:
	// Audio rendering
	bool MaybeRenderFrame(AudioFrame &frame);
	std::vector<int16_t> GetFrame();
	void AudioCallback(const uint16_t requested_frames);
	void RenderUpToNow();

	// IO callbacks to the left SAA1099 device
	void WriteDataToLeftDevice(io_port_t port, io_val_t value, io_width_t width);
	void WriteControlToLeftDevice(io_port_t port, io_val_t value, io_width_t width);

	// IO callbacks to the right SAA1099 device
	void WriteDataToRightDevice(io_port_t port, io_val_t value, io_width_t width);
	void WriteControlToRightDevice(io_port_t port, io_val_t value, io_width_t width);

	// IO callbacks to the Game Blaster detection chip
	void WriteToDetectionPort(io_port_t port, io_val_t value, io_width_t width);
	uint8_t ReadFromDetectionPort(io_port_t port, io_width_t width) const;

	const char *CardName() const;

	// Managed objects
	mixer_channel_t channel = nullptr;

	IO_WriteHandleObject write_handlers[4]           = {};
	IO_WriteHandleObject write_handler_for_detection = {};
	IO_ReadHandleObject read_handler_for_detection   = {};

	std::unique_ptr<saa1099_device> devices[2]                   = {};
	std::unique_ptr<reSIDfp::TwoPassSincResampler> resamplers[2] = {};

	std::queue<AudioFrame> fifo = {};

	// Static rate-related configuration
	static constexpr auto chip_clock     = 14318180 / 2;
	static constexpr auto render_divisor = 32;
	static constexpr auto render_rate_hz = ceil_sdivide(chip_clock,
	                                                    render_divisor);
	static constexpr auto ms_per_render  = MillisInSecond / render_rate_hz;

	// Runtime states
	double last_rendered_ms        = 0;
	io_port_t base_port            = 0;
	bool is_standalone_gameblaster = false;
	bool is_open                   = false;
	uint8_t cms_detect_register    = 0xff;
};

#endif
