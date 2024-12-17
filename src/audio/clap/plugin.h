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
