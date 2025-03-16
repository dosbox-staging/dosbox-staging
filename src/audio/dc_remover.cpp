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

#include "dc_remover.h"

#include <cassert>
#include <cmath>

#include "audio_frame.h"
#include "checks.h"

CHECK_NARROWING();

DcRemover::DcRemover() = default;

DcRemover::~DcRemover() = default;

void DcRemover::Configure(const int _sample_rate_hz, const float _0dbfs_sample_value,
                          const float _bias_threshold)
{
	scale_in  = 1.0f / _0dbfs_sample_value;
	scale_out = _0dbfs_sample_value;

	bias_threshold = bias_threshold / _0dbfs_sample_value;

	num_frames_to_average = _sample_rate_hz / 600;
}

AudioFrame DcRemover::Process(const AudioFrame in)
{
	// Scale input to [-1.0, 1.0] range
	const auto in_scaled = in * scale_in;

	// Clear the queue if the stream isn't biased
	if (in_scaled.left < bias_threshold || in_scaled.right < bias_threshold) {
		sum    = {};
		frames = {};
		return in;
	}

	// Keep a running sum and push the frame to the back of the queue
	sum += in_scaled;
	frames.push(in_scaled);

	AudioFrame average     = {};
	AudioFrame front_frame = {};
	if (frames.size() == num_frames_to_average) {
		// Compute the average and deduct it from the front frame
		average     = sum / num_frames_to_average;
		front_frame = frames.front();
		sum -= front_frame;
		frames.pop();
	}
	return (front_frame - average) * scale_out;
}
