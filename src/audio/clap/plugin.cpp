/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include "plugin.h"

#include <cassert>

#include "clap/all.h"

#include "dynlib.h"
#include "event_list.h"

namespace Clap {

Plugin::Plugin(dynlib_handle _lib, const clap_plugin_entry_t* _plugin_entry,
               const clap_plugin_t* _plugin)
        : lib(_lib),
          plugin_entry(_plugin_entry),
          plugin(_plugin)
{
	assert(plugin_entry);
	assert(plugin);

	constexpr auto NoInputChannels = 0;

	audio_in.channel_count = NoInputChannels;
	audio_in.data32        = nullptr;
	audio_in.data64        = nullptr;
	audio_in.constant_mask = 0;
	audio_in.latency       = 0;

	constexpr auto NumChannelsStereo = 2;

	audio_out.channel_count = NumChannelsStereo;
	audio_out.data32        = nullptr;
	audio_out.data64        = nullptr;
	audio_out.constant_mask = 0;
	audio_out.latency       = 0;

	process.transport           = nullptr;
	process.audio_inputs        = &audio_in;
	process.audio_inputs_count  = 0;
	process.audio_outputs       = &audio_out;
	process.audio_outputs_count = 1;
}

Plugin::~Plugin()
{
	if (plugin) {
		plugin->reset(plugin);
		plugin->deactivate(plugin);
		plugin->destroy(plugin);
		plugin = nullptr;
	}
	if (plugin_entry) {
		plugin_entry->deinit();
		plugin_entry = nullptr;
	}
	dynlib_close(lib);
}

void Plugin::Activate(const int sample_rate_hz)
{
	constexpr auto MinFrameCount = 1;
	constexpr auto MaxFrameCount = 8192;

	plugin->activate(plugin, sample_rate_hz, MinFrameCount, MaxFrameCount);
}

void Plugin::Process(float** audio_out, const int num_frames, EventList& event_list)
{
	process.audio_outputs->data32 = audio_out;

	constexpr auto SteadyTimeNotAvailable = -1;

	process.frames_count = num_frames;
	process.steady_time  = SteadyTimeNotAvailable;

	process.in_events  = event_list.GetInputEvents();
	process.out_events = event_list.GetOutputEvents();

	plugin->process(plugin, &process);
}

} // namespace Clap
