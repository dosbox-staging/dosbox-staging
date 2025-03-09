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

#include "gate.h"

#include <cassert>
#include <cmath>

#include "audio_frame.h"
#include "checks.h"

CHECK_NARROWING();

Gate::Gate() = default;

Gate::~Gate() = default;

void Gate::Configure(const int _sample_rate_hz, const float threshold_db,
                     const float attack_time_ms, const float release_time_ms)
{
	assert(_sample_rate_hz > 0);
	assert(attack_time_ms > 0.0f);
	assert(release_time_ms > 0.0f);

	const auto sample_rate_hz = static_cast<float>(_sample_rate_hz);

	threshold_value = std::exp2(threshold_db / 6.0f);

	attack_coeff = 1.0f /
	               std::pow(10.0f, 1000.0f / (attack_time_ms * sample_rate_hz));

	release_coeff = 1.0f /
	                std::pow(10.0f, 1000.0f / (release_time_ms * sample_rate_hz));

	seek_v  = 1.0f;
	seek_to = 1.0f;
}

AudioFrame Gate::Process(const AudioFrame in)
{
	const auto is_open = std::abs(in.left) > threshold_value ||
	                     std::abs(in.right) > threshold_value;

	if (is_open) {
		// attack phase
		seek_v *= attack_coeff + (1 - attack_coeff);
	} else {
		// release phase
		seek_v *= release_coeff;
	}

	return {in.left * seek_v, in.right * seek_v};
}
