/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2019-2022  kcgen <kcgen@users.noreply.github.com>
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

#include "support.h"

Envelope::Envelope([[maybe_unused]]const char *name)
{
}

void Envelope::Reactivate()
{
	edge[0] = 0;
	edge[1] = 0;
	edge[2] = 0;
	edge[3] = 0;
	frames_processed = 0;
	process = &Envelope::Apply;
}

void Envelope::SetFrameRate(const int rate)
{
	frame_rate = rate;
	Update();
}

void Envelope::SetPeakAmplitude(const int peak)
{
	peak_amplitude = peak;
	Update();
}

void Envelope::SetExpansionPercentage(const int percentage)
{
	expansion_percentage = percentage;
	Update();
}
void Envelope::SetExpiration(const int expire_s)
{
	expire_after_seconds = expire_s;
	Update();
}

void Envelope::Update()
{
	if (!frame_rate || !peak_amplitude || !expansion_percentage) {
		process = &Envelope::Skip;
		return;
	}

	// How many frames should we inspect before expiring?
	expire_after_frames = expire_after_seconds * frame_rate;

	// Calculate how much the envelope's edge can grow after a frame
	// presses it outward.
	assert(expansion_percentage <= 100);
	expansion_increment = ceil_sdivide(peak_amplitude * expansion_percentage, 100);
	Reactivate();
}

void Envelope::Process(int prev[], int next[])
{
	process(*this, prev, next);
}

void Envelope::Apply(int prev[], int next[])
{
	// Let us easily iterate over the frames and channels.
	int *samples[]{&prev[0], &prev[1], &next[0], &next[1]};

	for (auto i = 0; i < num_channels; ++i) {
		// compute the channels' envelope
		const auto lower_lip = edge[i] - expansion_increment;
		const auto upper_lip = edge[i] + expansion_increment;

		// clamp with the envelope and within the overall range
		auto sample = clamp(*samples[i], lower_lip, upper_lip);
		sample = clamp(sample, -peak_amplitude, peak_amplitude);

		// update the sample and it's new edge
		*samples[i] = sample;
		edge[i] = sample;
	}
	// Maybe expire the envelope
	if (expire_after_frames && ++frames_processed > expire_after_frames) {
		process = &Envelope::Skip;
	}
}
