// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_PLUGIN_H
#define DOSBOX_CLAP_PLUGIN_H

#include <memory>

#include "clap/all.h"

#include "event_list.h"
#include "library.h"

namespace Clap {

// Object-oriented wrapper to a CLAP plugin instance. Only plugins with
// exactly two 32-bit float output channels are supported currently (i.e.,
// MIDI synths).
//
class Plugin {

public:
	Plugin(const std::shared_ptr<Library> library, const clap_plugin_t* plugin);
	~Plugin();

	// Must be called before the first `Process()` call
	void Activate(const int sample_rate_hz);

	void Process(float** audio_out, const int num_frames, EventList& event_list);

	// prevent copying
	Plugin(const Plugin&) = delete;
	// prevent assignment
	Plugin& operator=(const Plugin&) = delete;

private:
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

	clap_audio_buffer_t audio_in  = {};
	clap_audio_buffer_t audio_out = {};

	clap_process_t process = {};
};

} // namespace Clap

#endif // DOSBOX_CLAP_PLUGIN_H
