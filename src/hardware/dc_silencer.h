/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2020-2021  Kevin R. Croft <krcroft@gmail.com>
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

#ifndef DOSBOX_DC_SILENCER_H
#define DOSBOX_DC_SILENCER_H

/*
DC Silencer
-----------
From a given DC-offset value, this class generates a fading cosine wave at an
inaudible frequency, which is suitable for eliminating a DC-offset within
the low 100s of milliseconds, versus "stepping" the DC-offset inward, which
produces and audible soft  zippering sound effect, unless carried out over
several seconds.

The number of waves and their frequency are configurable.

If audio audio equipment were perfect, we could assume only one wave would be
needed to take down a full DC-offset, and terminate perfectly at the centerline.
However in practice the speaker diaphragm oscillates across the centerline and
is dampened by the suspension material.  The literature: Langford, S. 2014.
Digital Audio Editing. Burlington: Focal Press. pp. 47-57 mentions fade-outs can
be performed in 500ms.  Assume we use 33 Hz inaudible waves, 500ms would require
roughly 15 waves.

Use
---
1. Configure() the silencer with the sample rate of the stream in need of
   DC-fixing, the number of full silencing waves to perform, and the frequency
   to use for those waves.  Nominal values might be 44100 Hz for the audio
   stream, 15 silencing waves, at 33 Hz.

2. Generate() when your stream in need of DC-offset correction. Pass it the
   current DC-offset value, and the number of samples to generate into the
   buffer.  Note that it's typically for generation to be performed over
   many calls.  The silencer keeps track of where it is in generating waves,
   and once done will fill the stream with silence.

   It returns true while still winding down the offset, and false once the
   signal's been populated with one full generate-round of zeros.

3. Reset() when you want to prepare the silencer for another round of
   silencing.  The silencer is reset as part of the configuration
   step (because the paramaters of the waves is changes and mixing new
   paramater with old setting can result in discontinuities).

*/

#include "config.h"

#include <cinttypes>
#include <cstdlib>

class DCSilencer {
public:
	DCSilencer() = default;
	void Configure(uint32_t sequence_hz, uint8_t full_waves, uint8_t wave_hz);
	bool Generate(int16_t dc_offset, size_t samples, int16_t *buffer);
	void Reset();

private:
	float rad_dt = 0.0f; // The delta radians added every sample
	float rad_pos = 0.0f; // The current position along our waves (in radians)
	float vol_dt = 0.0f;  // The delta volume decremented every sample
	float vol_pos = 0.0f; // The current volume level (as a fraction of 1)
};

#endif
