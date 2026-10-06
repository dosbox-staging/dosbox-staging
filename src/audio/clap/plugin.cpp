// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "plugin.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <stdexcept>

#include "clap/all.h"

#include "event_list.h"
#include "library.h"
#include "misc/logging.h"
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

	for (size_t channel_index = 0; channel_index < num_channels; ++channel_index) {
		channels[channel_index] = samples.data() +
		                          channel_index * MaxFrameCount;
	}

	size_t channel_offset = 0;

	for (size_t port_index = 0; port_index < channel_counts.size(); ++port_index) {
		ports[port_index].channel_count = channel_counts[port_index];
		ports[port_index].data32 = channels.data() + channel_offset;
		channel_offset += channel_counts[port_index];
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

	// PluginManager validates that the first audio output port exists
	// and is stereo before constructing this wrapper.
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

	assert(!is_processing);
	if (is_active) {
		plugin->deactivate(plugin);
	}

	plugin->destroy(plugin);
	plugin = nullptr;
}

void Plugin::Activate(const int sample_rate_hz)
{
	constexpr auto MinFrameCount = 1;

	assert(!is_active);
	if (!plugin->activate(plugin, sample_rate_hz, MinFrameCount, MaxFrameCount)) {
		LOG_WARNING("CLAP: Plugin activation failed at %d Hz", sample_rate_hz);
		throw std::runtime_error("CLAP: Plugin activation failed");
	}
	is_active = true;
}

void Plugin::Process(float** audio_out, const int num_frames, EventList& event_list)
{
	assert(num_frames > 0 && num_frames <= MaxFrameCount);
	assert(is_active);

	if (!is_processing && !processing_failed) {
		is_processing = plugin->start_processing(plugin);
		if (!is_processing) {
			processing_failed = true;
			LOG_ERR("CLAP: Plugin failed to start audio processing");
		}
	}

	auto audio_out_fill_zero = [&]() {
		std::fill_n(audio_out[0], num_frames, 0.0f);
		std::fill_n(audio_out[1], num_frames, 0.0f);
	};

	if (processing_failed) {
		audio_out_fill_zero();
		return;
	}

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

	if (plugin->process(plugin, &process) == CLAP_PROCESS_ERROR) {
		if (!process_error_reported) {
			LOG_ERR("CLAP: Audio processing failed; discarding the output block");
		}

		process_error_reported = true;
		audio_out_fill_zero();
	} else {
		process_error_reported = false;
	}
}

void Plugin::StopProcessing()
{
	if (is_processing) {
		plugin->stop_processing(plugin);
		is_processing = false;
	}
}

} // namespace Clap
