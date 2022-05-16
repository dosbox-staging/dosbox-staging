/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2019-2021  kcgen <kcgen@users.noreply.github.com>
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

#ifndef DOSBOX_ENVELOPE_H
#define DOSBOX_ENVELOPE_H

/*  Audio Envelope
 *  --------------
 *  This class applies a step-wise earned-volume envelope with a fixed
 * expiration period. The envelope is "earned" in the sense that the edge is
 * expanded when a sample meets or exceeds it. This helps minimize the
 * impact of unnatural waveforms that can whipsaw wildly, such as those
 * generated from digital machine noise or binary data.
 *
 *  Use
 *  ---
 *  1. Call Update(..) to provide the Envelope with information about the audio
 *     stream: the frame rate (in Hz), peak possible sample amplitude (from zero
 *     to 2^16 -1), the expansion phase duration in milliseconds that represents
 *     the shortest possible time the envelope will be expanded from zero to
 *     peak volume if the samples "earn" it (reasonable values are <30ms), and
 *     the desired expiration period in seconds (reasonable values are <60s).
 *
 *  2. Call Process(..), passing it samples in their natural 16-bit signed
 *     form. Note: when the envelope is fully expanded or has expired, this
 *     function becomes the a null-call, eliminating further overhead.
 *     To be clear - there are no runtime checks you need to perform to
 *     determine if you should use the envelope or not.  It simply goes
 *     dormant when done.
 *
 *  3. Call Reactivate() to perform another round of enveloping. Note that the
 *     characteristics about the envelope provided in the Update() call are
 *     retained and do not need to be provided after a reactivating.
 *
 *  By default, the evenlope does nothing; it needs to be Update()'d for it to
 *  do work.
 */

#include "dosbox.h"

#include <cstdint>
#include <functional>

class Envelope {
public:
	Envelope(const char* name);

	void Process(bool is_stereo, int frame[]);

	void Update(int frame_rate,
	            int peak_amplitude,
	            uint8_t expansion_phase_ms,
	            uint8_t expire_after_seconds);

	void Reactivate();

private:
	Envelope(const Envelope &) = delete;            // prevent copying
	Envelope &operator=(const Envelope &) = delete; // prevent assignment

	bool ClampSample(int &sample, int next_edge);

	void Apply(bool is_stereo, int frame[]);

	void Skip([[maybe_unused]] bool is_stereo,
	          [[maybe_unused]] int frame[])
	{}

	using process_f = std::function<void(Envelope &, bool, int[])>;
	process_f process = &Envelope::Apply;

	const char *channel_name = nullptr;
	int expire_after_frames = 0; // Stop enveloping when this many
	                             // frames have been processed.
	int frames_done = 0;         // A tally of processed frames.
	int edge = 0;                // The current edge of the envelope, which
	              // increments outward when samples press against it.
	int edge_increment = 0; // The amount the edge grows by once a
	                        // sample is found to be beyond it.
	int edge_limit = 0;     // Stop enveloping when the current edge is
	                        // hits or exceeds this limit.
};

#endif
