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

#ifndef DOSBOX_ENVELOPE_H
#define DOSBOX_ENVELOPE_H

/* Audio Envelope
 * --------------
 * When an audio device's channel starts up after being Enable()'d, this
 * this class applies a volume (or magnitude) envelope during the first five
 * milliseconds.
 *
 * An envelope does not scale or dampen the audio; instead, it only limits the
 * samples if they happen to exceed the envelope.  This prevents poorly
 * behaved initial signals and samples from causing a discontinuous click or
 * sharp pop; while at the same time the envelope does not alter the frequency.
 * If the audio is well-behaved (ie: its volume isn't spiked up in the first
 * couple samples), then no action is taken and the audio is entirely
 * passed-through.
 *
 *
 * Use
 * ---
 * 1. First Update(..) to provide Envelope with the channel's sample_rate and
 *    peak_magnitude.  It uses these to calculate the characteristics (height
 *    and duration) of the envelope.
 *
 * 2. Next call Process(..), passing it samples in their natural 16-bit signed
 *    form. Note: when the enveloping is complete (ie: 5ms later), this class
 *    replaced Process() with a null-call, eliminating further overhead.
 *
 * 3. When the audio channel is shutdown or disabled, call Reset() to put the
 *    envelope back to its original state.
 *
 * By default, the evenlope does nothing; it needs to be Update()'d for it to
 * do actual work.
 *
 *
 * CPU Usage
 * ---------
 * The envelope's sample count and rate of growth are computed only during the
 * Update() call.  This allows us to then use only trivial integer comparisons
 * and two integer increments when being applied, so the CPU cost is miniscule.
 * Plus, once the envelope is done, this function is swapped out for the empty
 * Skip() call, further reducing this overhead to a no-up.
 */

#include <cstdint>

class Envelope {
public:
	void Reset();
	void Update(uint32_t sample_rate, uint32_t peak_magnitude);
	void Process(bool is_stereo,
	             bool is_interpolated,
	             intptr_t prev[],
	             intptr_t next[])
	{
		(this->*process)(is_stereo, is_interpolated, prev, next);
	};

private:
	void Apply(bool is_stereo,
	           bool is_interpolated,
	           intptr_t prev[],
	           intptr_t next[]);

	void Skip(bool is_stereo,
	          bool is_interpolated,
	          intptr_t prev[],
	          intptr_t next[]);

	typedef void (Envelope::*process_f)(bool, bool, intptr_t[], intptr_t[]);
	process_f process = &Envelope::Apply;

	// Static properties: these are calculated on-update.
	uint16_t total_frames = 0u;   // The number of frames across which the
	                              // envelope is applied.
	uint16_t edge_increment = 0u; // The amount the edge of the envelope
	                              // expands per frame.

	// Dynamic state values: these increment when applying
	uint16_t edge_limit = 0u;  // the edge of the envelope beyond which
	                           // samples will be clipped
	uint16_t frames_done = 0u; // how far along we are in envelope. When
	                           // the total is reached then we're done.
};

#endif
