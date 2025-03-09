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
 * This is a simplified port of the noisegate JSFX REAPER effect by unknown
 * author.
 *
 * Copyright notice of the original effect plugin:
 *
 * This effect Copyright (C) 2004 and later Cockos Incorporated
 * License: GPL - http://www.gnu.org/licenses/gpl.html
 */

#ifndef DOSBOX_GATE_H
#define DOSBOX_GATE_H

#include "dosbox.h"

typedef struct AudioFrame AudioFrame_;

// TODO
// Implements a dynamic-range reducing audio signal compressor to reduce the
// volume of loud sounds above a given threshold.
//
// The compressor uses the standard set of of adjustable control parameters
// common to all compressors; the following Wikipedia page gives a good
// overview about them:
//
// https://en.wikipedia.org/wiki/Dynamic_range_compression#Controls_and_features
//
class Gate {
public:
	Gate();
	~Gate();

	void Configure(const int sample_rate_hz, const float _0dbfs_sample_value,
	               const float threshold_db, const float attack_time_ms,
	               const float release_time_ms);

	AudioFrame Process(const AudioFrame in);

	// prevent copying
	Gate(const Gate&) = delete;
	// prevent assignment
	Gate& operator=(const Gate&) = delete;

private:
	float scale_in  = {};
	float scale_out = {};

	float threshold_value = {};
	float attack_coeff    = {};
	float release_coeff   = {};

	// state variables
	float seek_v  = {};
	float seek_to = {};
};

#endif // DOSBOX_GATE_H
