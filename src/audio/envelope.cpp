/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2025  The DOSBox Staging Team
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

#include "envelope.h"

#include "math_utils.h"
#include "mixer.h"

Envelope::Envelope(const char* name) : channel_name(name) {}

void Envelope::Reactivate()
{
	edge        = 0.0f;
	frames_done = 0;

	process = &Envelope::Apply;
}

void Envelope::Update(const int sample_rate_hz, const int peak_amplitude,
                      const uint8_t expansion_phase_ms,
                      const uint8_t expire_after_seconds)
{
	if (!sample_rate_hz || !peak_amplitude || !expansion_phase_ms) {
		return;
	}

	// How many frames should we inspect before expiring?
	expire_after_frames = expire_after_seconds * sample_rate_hz;
	assert(expire_after_frames > 0);

	// The furtherest allowed edge is the peak sample amplitude.
	edge_limit = static_cast<float>(peak_amplitude);

	// Permit the envelop to achieve peak volume within the expansion_phase
	// (in ms) if the samples happen to constantly press on the edges.
	const auto expansion_phase_frames = ceil_sdivide(sample_rate_hz * expansion_phase_ms,
	                                                 1000);

	// Calculate how much the envelope's edge will grow after a frame
	// presses against it.
	assert(expansion_phase_frames);
	edge_increment = static_cast<float>(
	        ceil_sdivide(peak_amplitude, expansion_phase_frames));

#if 0
	LOG_DEBUG("ENVELOPE: %s grows by %-3f to %-5f across %-3u frames (%u ms)",
	          channel_name.c_str(),
	          edge_increment,
	          edge_limit,
	          expansion_phase_frames,
	          expansion_phase_ms);
#endif
}

bool Envelope::ClampSample(float& sample, const float lip)
{
	if (std::abs(sample) > edge) {
		sample = clamp(sample, -lip, lip);
		return true;
	}
	return false;
}

void Envelope::Process(const bool is_stereo, AudioFrame& frame)
{
	process(*this, is_stereo, frame);
}

void Envelope::Apply(const bool is_stereo, AudioFrame& frame)
{
	// Only start the envelope once our samples have actual values
	if (frame.left == 0.0f && frames_done == 0u) {
		return;
	}

	// beyond the edge is the lip. Do any samples walk out onto the lip?
	const float lip   = edge + edge_increment;
	const bool on_lip = ClampSample(frame.left, lip) ||
	                    (is_stereo && ClampSample(frame.right, lip));

	// If any of the samples are out on the lip, then march the edge forward
	if (on_lip) {
		edge += edge_increment;
	}

	// Should we deactivate the envelope?
	if (++frames_done > expire_after_frames || edge >= edge_limit) {
		process = &Envelope::Skip;
		(void)channel_name; // [[maybe_unused]] in release builds
		LOG_DEBUG("ENVELOPE: %s done after %u frames, peak sample was %.4f",
		          channel_name.c_str(),
		          frames_done,
		          edge);
	}
}

