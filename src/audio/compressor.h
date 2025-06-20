/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2025  The DOSBox Staging Team
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
 * This is a simplified port of Thomas Scott Stillwell's "Master Tom
 * Compressor" JSFX effect bundled with REAPER (just the RMS & feedforward
 * path).
 *
 * Copyright notice of the original effect plugin:
 *
 *
 * Copyright 2006, Thomas Scott Stillwell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * The name of Thomas Scott Stillwell may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOSBOX_COMPRESSOR_H
#define DOSBOX_COMPRESSOR_H

#include "dosbox.h"

#include <cstdint>

typedef struct AudioFrame AudioFrame_;

// Implements a dynamic-range reducing audio signal compressor to reduce the
// volume of loud sounds above a given threshold.
//
// The compressor uses the standard set of of adjustable control parameters
// common to all compressors; the following Wikipedia page gives a good
// overview about them:
//
// https://en.wikipedia.org/wiki/Dynamic_range_compression#Controls_and_features
//
class Compressor {
public:
	Compressor();
	~Compressor();

	void Configure(const int sample_rate_hz,
	               const float _0dbfs_sample_value, const float threshold_db,
	               const float ratio, const float attack_time_ms,
	               const float release_time_ms, const float rms_window_ms);
	void Reset();

	AudioFrame Process(const AudioFrame in);

	// prevent copying
	Compressor(const Compressor &) = delete;
	// prevent assignment
	Compressor &operator=(const Compressor &) = delete;

private:
	float sample_rate_hz = {};
	float scale_in       = {};
	float scale_out      = {};

	float threshold_value = {};
	float ratio           = {};
	float attack_coeff    = {};
	float release_coeff   = {};
	float rms_coeff       = {};

	// state variables
	float comp_ratio      = {};
	float run_db          = {};
	float run_sum_squares = {};
	float over_db         = {};
	float run_max_db      = {};
	float max_over_db     = {};
};

#endif
