// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
#include <string>

typedef struct AudioFrame AudioFrame_;

class Envelope {
public:
	Envelope(const char* name);

	void Process(const bool is_stereo, AudioFrame& frame);

	void Update(const int sample_rate_hz, const int peak_amplitude,
	            const uint8_t expansion_phase_ms,
	            const uint8_t expire_after_seconds);

	void Reactivate();

	// prevent copying
	Envelope(const Envelope&) = delete;

	// prevent assignment
	Envelope& operator=(const Envelope&) = delete;

private:
	bool ClampSample(float& sample, const float next_edge);

	void Apply(const bool is_stereo, AudioFrame& frame);

	void Skip([[maybe_unused]] const bool is_stereo,
	          [[maybe_unused]] AudioFrame& frame)
	{}

	using process_f   = std::function<void(Envelope&, bool, AudioFrame&)>;
	process_f process = &Envelope::Apply;

	std::string channel_name = {};

	// Stop enveloping when this frames have been processed many
	int expire_after_frames = 0;

	// A tally of processed frames.
	int frames_done = 0;

	// The current edge of the envelope, which increments outward when
	// samples press against it.
	float edge = 0.0f;

	// The amount the edge grows by once a sample is found to be beyond it.
	float edge_increment = 0.0f;

	// Stop enveloping when the current edge is hits or exceeds this limit.
	float edge_limit = 0.0f;
};

#endif
