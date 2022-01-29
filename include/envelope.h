/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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
 *  1. Call the four Set*(..) functions to provide the Envelope with information
 *     about the audio stream: the frame rate (in Hz), peak possible sample
 *     amplitude (from zero to 2^16 -1), the expansion percentage relative to
 *     the peak sample, and how long, in seconds, the enveloper should run
 *     before expiring (zero seconds means run forever).
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
 *  By default, the evenlope does nothing; it needs to be Set()'d for it to
 *  do work.
 */

#include "dosbox.h"

#include <cstdint>
#include <functional>

class Envelope {
public:
	Envelope(const char* name);

	void Process(int prev[], int next[]);

	void SetFrameRate(int frame_rate);
	void SetPeakAmplitude(int peak);
	void SetExpansionPercentage(int percent);
	void SetExpiration(int seconds);

	void Reactivate();

private:
	Envelope(const Envelope &) = delete;            // prevent copying
	Envelope &operator=(const Envelope &) = delete; // prevent assignment

	void Apply(int prev[], int next[]);

	void Skip([[maybe_unused]] int prev[], [[maybe_unused]] int next[]) {}

	void Update();

	using process_f = std::function<void(Envelope &, int[], int[])>;
	process_f process = &Envelope::Apply;


	static constexpr auto num_channels = 4; // prev & next, L & R channels.

	// Configurable parameters
	int frame_rate = 0;           // audio frames per second
	int peak_amplitude = 0;       // The peak amplitude of the envelope.
	int expansion_percentage = 0; // The edge's maximum incremental amplitide.
	int expansion_increment = 0;  // The edge's incremental amplitude.
	int expire_after_seconds = 0; // The number of seconds the envelope will
	                              // last.
	int expire_after_frames = 0;  // Stop after processing this many frames.

	// State tracking variables
	int edge[num_channels] = {}; // The edge of each channel's envelope.
	int frames_processed = 0;    // The total number of frames processed.
};

#endif
