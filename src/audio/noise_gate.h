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

#ifndef DOSBOX_NOISE_GATE_H
#define DOSBOX_NOISE_GATE_H

#include "dosbox.h"

#include <array>

#include <Iir.h>

typedef struct AudioFrame AudioFrame_;

// Implements a simple noise gate that mutes the signal below a
// given threshold. The release and attack parameters control how
// quickly will the signal get muted or brought back from the
// muted state, respectively.
//
// You can read more about noise gates here:
// https://en.wikipedia.org/wiki/Noise_gate
//
class NoiseGate {
public:
	NoiseGate();
	~NoiseGate();

	void Configure(const int sample_rate_hz, const float _0dbfs_sample_value,
	               const float threshold_db, const float attack_time_ms,
	               const float release_time_ms);

	AudioFrame Process(const AudioFrame in);

	// prevent copying
	NoiseGate(const NoiseGate&) = delete;
	// prevent assignment
	NoiseGate& operator=(const NoiseGate&) = delete;

private:
	float scale_in  = {};
	float scale_out = {};

	float threshold_value = {};
	float attack_coeff    = {};
	float release_coeff   = {};

	// Second-order Butterworth high-pass filter (stereo)
	std::array<Iir::Butterworth::HighPass<2>, 2> highpass_filter = {};

	// state variables
	float seek_v  = {};
	float seek_to = {};
};

#endif // DOSBOX_NOISE_GATE_H
