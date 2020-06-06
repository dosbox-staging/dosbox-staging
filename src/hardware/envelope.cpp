/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2020-2020  The dosbox-staging team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// #define DEBUG 1

#include "envelope.h"

#include "support.h"

void Envelope::Reset()
{
	edge_limit = 0u;
	frames_done = 0u;
	process = &Envelope::Apply;
}

void Envelope::Update(uint32_t sample_rate, uint32_t peak_magnitude)
{
	if (!sample_rate || !peak_magnitude)
		return;

	// Calculate how many frames are needed to create a 5 millsecond
	// envelope: (sample_rate frames/s * 5 ms/envelope) / 1000 ms/s, which
	// becomes freq / 200.
	total_frames = static_cast<uint16_t>(ceil_udivide(sample_rate, 200u));

	// Calculate how much the envelope's edge will grow per frame
	assert(total_frames > 0);
	edge_increment = ceil_udivide(peak_magnitude, total_frames);
#ifdef DEBUG
	LOG_MSG("ENVELOPE: initial envelope spans %u frames up to a peak "
	        "magnitude of %u.",
	        total_frames, peak_magnitude);
#endif
}

void Envelope::Apply(const bool is_stereo,
                     const bool is_interpolated,
                     intptr_t prev[],
                     intptr_t next[])
{
	// Only start the envelope once our samples have actual values
	if (prev[0] == 0 && frames_done == 0u)
		return;

	const Bits edge = static_cast<Bits>(edge_limit);
	prev[0] = clamp(prev[0], -edge, edge);
	if (is_stereo)
		prev[1] = clamp(prev[1], -edge, edge);
	if (is_interpolated) {
		next[0] = clamp(next[0], -edge, edge);
		if (is_stereo)
			next[1] = clamp(next[1], -edge, edge);
	}

	// Increment our edge, and check if we're done and should skip future
	// checks
	edge_limit += edge_increment;
	if (++frames_done > total_frames)
		process = &Envelope::Skip;
}

void Envelope::Skip(const bool is_stereo,
                    const bool is_interpolated,
                    intptr_t prev[],
                    intptr_t next[])
{
	(void)is_stereo;       // unused
	(void)is_interpolated; // unused
	(void)prev;            // unused
	(void)next;            // unused
}
