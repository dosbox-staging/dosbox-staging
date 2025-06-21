// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2004-2004  Cockos Incorporated
// SPDX-License-Identifier: GPL-2.0-or-later

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
