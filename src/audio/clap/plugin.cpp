// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "plugin.h"

#include <cassert>
#include <numeric>

#include "clap/all.h"

#include "event_list.h"
#include "library.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace Clap {

Plugin::AudioBuffers::AudioBuffers(const std::vector<uint32_t>& channel_counts)
        : ports(channel_counts.size())
{
	const auto num_channels = std::accumulate(channel_counts.begin(),
	                                          channel_counts.end(),
	                                          size_t{0});
	samples.resize(num_channels * MaxFrameCount);
	channels.resize(num_channels);
	for (size_t i = 0; i < num_channels; ++i) {
		channels[i] = samples.data() + i * MaxFrameCount;
	}

	size_t channel_offset = 0;
	for (size_t i = 0; i < channel_counts.size(); ++i) {
		ports[i].channel_count = channel_counts[i];
		ports[i].data32        = channels.data() + channel_offset;
		channel_offset += channel_counts[i];
	}
}

Plugin::Plugin(std::shared_ptr<Library> _library, const clap_plugin_t* _plugin,
               const std::vector<uint32_t>& input_channel_counts,
               const std::vector<uint32_t>& output_channel_counts)
        : library(std::move(_library)),
          plugin(_plugin),
          audio_in(input_channel_counts),
          audio_out(output_channel_counts)
{
	assert(plugin);
	assert(!audio_out.ports.empty());
	assert(audio_out.ports.front().channel_count == 2);

	process.transport    = nullptr;
	process.audio_inputs = audio_in.ports.empty() ? nullptr
	                                              : audio_in.ports.data();
	process.audio_inputs_count = check_cast<uint32_t>(audio_in.ports.size());
	process.audio_outputs = audio_out.ports.data();
	process.audio_outputs_count = check_cast<uint32_t>(audio_out.ports.size());
}

Plugin::~Plugin()
{
	assert(plugin);

	plugin->reset(plugin);
	plugin->deactivate(plugin);
	plugin->destroy(plugin);
	plugin = nullptr;
}

void Plugin::Activate(const int sample_rate_hz)
{
	constexpr auto MinFrameCount = 1;

	plugin->activate(plugin, sample_rate_hz, MinFrameCount, MaxFrameCount);
}

void Plugin::Process(float** audio_out, const int num_frames, EventList& event_list)
{
	assert(num_frames > 0 && num_frames <= MaxFrameCount);

	// Output metadata belongs to the plugin and may change on every call.
	for (auto& port : this->audio_out.ports) {
		port.constant_mask = 0;
	}
	process.audio_outputs->data32 = audio_out;

	constexpr auto SteadyTimeNotAvailable = -1;

	process.frames_count = num_frames;
	process.steady_time  = SteadyTimeNotAvailable;

	process.in_events  = event_list.GetInputEvents();
	process.out_events = event_list.GetOutputEvents();

	plugin->process(plugin, &process);
}

} // namespace Clap
