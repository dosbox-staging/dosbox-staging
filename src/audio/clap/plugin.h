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

#include "dynlib.h"
#include "event_list.h"

namespace Clap {

// Object-oriented wrapper to a CLAP plugin instance. Only plugins with
// exactly two 32-bit float output channels are currently supported (i.e.,
// MIDI synths).
//
class Plugin {

public:
	Plugin(dynlib_handle lib, const clap_plugin_entry_t* plugin_entry,
	       const clap_plugin_t* plugin);

	~Plugin();

	void Activate(const int sample_rate_hz);

	void Process(float** audio_out, const int num_frames, EventList& event_list);

	// prevent copying
	Plugin(const Plugin&) = delete;
	// prevent assignment
	Plugin& operator=(const Plugin&) = delete;

private:
	dynlib_handle lib = {};

	const clap_plugin_entry_t* plugin_entry = nullptr;
	const clap_plugin_t* plugin             = nullptr;

	clap_audio_buffer_t audio_in  = {};
	clap_audio_buffer_t audio_out = {};

	clap_process_t process = {};
};

} // namespace Clap

#endif // DOSBOX_CLAP_PLUGIN_H
