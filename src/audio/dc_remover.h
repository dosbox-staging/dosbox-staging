/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2025-2025  The DOSBox Staging Team
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

/* ---------------------------------------------------------------------------
 * This is a modified port of the "noise gate" JSFX effect bundled with
 * REAPER by unknown author (most likely Justin Frankel).
 *
 * Copyright notice of the original effect plugin:
 *
 * This effect Copyright (C) 2004 and later Cockos Incorporated
 * License: GPL - http://www.gnu.org/licenses/gpl.html
 */

#ifndef DOSBOX_DC_REMOVER_H
#define DOSBOX_DC_REMOVER_H

#include "dosbox.h"

#include <array>
#include <queue>

#include "audio_frame.h"

class DcRemover {
public:
	DcRemover();
	~DcRemover();

	void Configure(const int sample_rate_hz, const float _0dbfs_sample_value,
	               const float release_time_ms);

	AudioFrame Process(const AudioFrame in);

	// prevent copying
	DcRemover(const DcRemover&) = delete;
	// prevent assignment
	DcRemover& operator=(const DcRemover&) = delete;

private:
	float scale_in       = {};
	float scale_out      = {};
	float bias_threshold = {};

	AudioFrame sum            = {};
	int num_frames_to_average = {};

	std::queue<AudioFrame> frames = {};
};

#endif // DOSBOX_DC_REMOVER_H
