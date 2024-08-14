/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef SBLASTER_H
#define SBLASTER_H

#include "dma.h"
#include "mixer.h"
#include "rwqueue.h"

class SBLASTER final {
public:
	SBLASTER(Section* conf);
	~SBLASTER();

	void PicCallback(const int requested);

	void SetChannelRateHz(const int requested_rate_hz);

	// Public members used by MIXER_PullFromQueueCallback
	RWQueue<AudioFrame> output_queue{1};
	MixerChannelPtr channel = nullptr;
	float frame_counter     = 0.0f;

private:
	IO_ReadHandleObject read_handlers[0x10]   = {};
	IO_WriteHandleObject write_handlers[0x10] = {};

	static constexpr auto BlasterEnvVar = "BLASTER";

	OplMode oplmode = OplMode::None;

	bool cms = false;

	void SetupEnvironment();
	void ClearEnvironment();
};

extern std::unique_ptr<SBLASTER> sblaster;

#endif
