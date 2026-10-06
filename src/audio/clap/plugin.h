// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_PLUGIN_H
#define DOSBOX_CLAP_PLUGIN_H

#include <memory>
#include <vector>

#include "clap/all.h"

#include "event_list.h"
#include "library.h"

namespace Clap {

// Object-oriented wrapper to a CLAP MIDI synth. MIDI events target the first
// note input port, and the first stereo audio output port is played back.
// Additional audio inputs receive silence and additional outputs are discarded.
//
class Plugin {

public:
	// Maximum number of audio frames accepted by Process().
	static constexpr int MaxFrameCount = 8192;

	Plugin(const std::shared_ptr<Library> library, const clap_plugin_t* plugin,
	       const std::vector<uint32_t>& input_channel_counts,
	       const std::vector<uint32_t>& output_channel_counts);
	~Plugin();

	// Must be called before the first `Process()` call
	void Activate(const int sample_rate_hz);

	void Process(float** audio_out, const int num_frames, EventList& event_list);
	// Call on the render thread before deactivating or destroying the plugin.
	void StopProcessing();

	// prevent copying
	Plugin(const Plugin&) = delete;
	// prevent assignment
	Plugin& operator=(const Plugin&) = delete;

private:
	// Keep the CLAP port array contiguous, with separate sample storage and
	// channel pointer arrays. All buffers are allocated before processing.
	struct AudioBuffers {
		AudioBuffers(const std::vector<uint32_t>& channel_counts);

		std::vector<clap_audio_buffer_t> ports = {};
		std::vector<float> samples             = {};
		std::vector<float*> channels           = {};
	};

	// Reference to the CLAP library that wraps the underlying dynamic-link
	// library. A single library can contain multiple plugins, or the same
	// plugin can be instantiated multiple times -- all these plugin
	// instances would reference the same library via shared_ptrs.
	//
	// This accomplishes automatic lifecycle management: once the last
	// ref-counted library shared_ptr is destructed, that triggers the
	// desctruction of the library itself.
	//
	std::shared_ptr<Library> library = nullptr;

	const clap_plugin_t* plugin = nullptr;
	bool is_active              = false;
	bool is_processing          = false;
	bool processing_failed      = false;

	AudioBuffers audio_in;
	AudioBuffers audio_out;

	clap_process_t process = {};
};

} // namespace Clap

#endif // DOSBOX_CLAP_PLUGIN_H
