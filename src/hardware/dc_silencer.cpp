/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  The dosbox-staging team
 *  Copyright (c) 2019-2020  Kevin R Croft <krcroft@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dc_silencer.h"
#include "support.h"

void DCSilencer::Configure(const uint32_t sequence_hz,
                           const uint8_t silent_waves,
                           const uint8_t silent_wave_hz)
{
	// N curves + a quarter-radian split into 20ms (50-Hz) worth of samples
	assertm(silent_waves >= 2u,
	        "The number of silencing waves should be at least two");
	assertm(silent_wave_hz >= 20u && silent_wave_hz <= 60u,
	        "The silent wave should be inaudible: between 20 Hz and 60 Hz");
	assertm(sequence_hz > silent_wave_hz,
	        "If the sequence is silent too, then you don't need the "
	        "silencer");

	// Add 90-degrees (1/4th-rad) to get from the DC-offset down to zero
	const float waves = 0.25f + silent_waves;

	// How many sample points exist to generate our waves
	constexpr auto pi_f = static_cast<float>(M_PI);
	const float steps = waves * sequence_hz / silent_wave_hz;
	rad_dt = waves * 2 * pi_f / steps; // delta radian increment per step
	vol_dt = 1 / steps; // delta volume decrement per step
	// DEBUG_LOG_MSG("SILENCER: Using %u waves at %u-Hz across %.0f samples",
	//               silent_waves, silent_wave_hz, steps);
	Reset();
}

bool DCSilencer::Generate(const int16_t dc_offset, const size_t samples, int16_t *buffer)
{
	assertm(rad_dt > 0 && vol_dt > 0, "Configure the silencer first");
	uint16_t i = 0;
	while (vol_pos > 0 && i < samples) {
		vol_pos -= vol_dt; // keep turning down the volume ..
		rad_pos += rad_dt; // keep walking around our circle ..
		const float sample = dc_offset * cosf(rad_pos) * vol_pos;
		buffer[i++] = static_cast<int16_t>(sample);
	}
	// only consider the job done when we haven't generated any samples.
	const bool still_generating = i > 0;

	// When the waves are done, fill any reamainder with silence.
	while (i < samples)
		buffer[i++] = 0;

	return still_generating;
}

void DCSilencer::Reset()
{
	rad_pos = 0;
	vol_pos = 1;
}
