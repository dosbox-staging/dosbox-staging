// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "envelope.h"

#include "audio/mixer.h"
#include "math_utils.h"

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

