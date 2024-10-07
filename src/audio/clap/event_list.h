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

#ifndef DOSBOX_CLAP_EVENT_LIST_H
#define DOSBOX_CLAP_EVENT_LIST_H

#include <vector>

#include "clap/all.h"

namespace Clap {

// Object-oriented helper to create an event list in the format expected by
// the CLAP interface.
//
// CLAP plugins don't support "real-time" events; you must pass in an event
// list along with sample-accurate timing data (the sample offset of each
// event from the start of the buffer) when calling the plugin's process
// function.
//
class EventList {
public:
	EventList();
	~EventList() = default;

	void Clear();

	void AddMidiEvent(const std::vector<uint8_t>& msg,
	                  const uint32_t sample_offset);

	void AddMidiSysExEvent(const std::vector<uint8_t>& msg,
	                       const uint32_t sample_offset);

	uint32_t Size() const;
	const clap_event_header_t* Get(const uint32_t index) const;

	const clap_input_events* GetInputEvents() const;
	const clap_output_events* GetOutputEvents() const;

private:
	std::vector<uint8_t> event_data   = {};
	std::vector<size_t> event_offsets = {};

	std::vector<uint8_t> sysex_data = {};

	clap_input_events input_events   = {};
	clap_output_events output_events = {};
};

} // namespace Clap

#endif // DOSBOX_CLAP_EVENT_LIST_H
