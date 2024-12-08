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

#include "event_list.h"

#include <cassert>

#include "logging.h"
#include "midi.h"

namespace Clap {

static uint32_t size(const struct clap_input_events* in)
{
	auto* list = static_cast<const EventList*>(in->ctx);
	return static_cast<uint32_t>(list->Size());
}

static const clap_event_header_t* get(const struct clap_input_events* in,
                                      uint32_t index)
{
	auto* list = static_cast<const EventList*>(in->ctx);
	return list->Get(index);
}

static bool try_push([[maybe_unused]] const struct clap_output_events* out,
                     [[maybe_unused]] const clap_event_header_t* event)
{
	// Unimplemented (false means the event could not be pushed into queue)
	return false;
}

EventList::EventList()
{
	constexpr auto NumBytes  = 8192;
	constexpr auto NumEvents = 1024;

	event_data.reserve(NumBytes);
	event_offsets.reserve(NumEvents);

	sysex_data.reserve(MaxMidiSysExBytes);

	input_events  = {this, &size, &get};
	output_events = {this, &try_push};
}

void EventList::Clear()
{
	event_data.clear();
	event_offsets.clear();

	sysex_data.clear();
}

void EventList::AddMidiEvent(const std::vector<uint8_t>& msg,
                             const uint32_t sample_offset)
{
	assert(msg.size() == 2 || msg.size() == 3);

	clap_event_midi ev = {};

	ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
	ev.header.type     = CLAP_EVENT_MIDI;
	ev.header.time     = sample_offset;
	ev.header.flags    = 0;
	ev.header.size     = sizeof(ev);
	ev.port_index      = 0;
	ev.data[0]         = msg[0];
	ev.data[1]         = msg[1];

	if (msg.size() == 3) {
		ev.data[2] = msg[2];
	} else {
		ev.data[2] = 0;
	}

	const auto offset = event_data.size();
	event_offsets.emplace_back(offset);

	const auto ev_start = reinterpret_cast<uint8_t*>(&ev);
	const auto ev_end   = ev_start + sizeof(ev);

	event_data.insert(event_data.end(), ev_start, ev_end);
}

void EventList::AddMidiSysExEvent(const std::vector<uint8_t>& msg,
                                  const uint32_t sample_offset)
{
	assert(msg.size() > 0);

	clap_event_midi_sysex ev = {};

	ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
	ev.header.type     = CLAP_EVENT_MIDI_SYSEX;
	ev.header.time     = sample_offset;
	ev.header.flags    = 0;
	ev.header.size     = sizeof(ev);
	ev.port_index      = 0;
	ev.buffer          = sysex_data.data() + sysex_data.size();
	ev.size            = msg.size();

	sysex_data.insert(sysex_data.end(), msg.begin(), msg.end());

	const auto offset = event_data.size();
	event_offsets.emplace_back(offset);

	const auto ev_start = reinterpret_cast<uint8_t*>(&ev);
	const auto ev_end   = ev_start + sizeof(ev);

	event_data.insert(event_data.end(), ev_start, ev_end);
}

uint32_t EventList::Size() const
{
	return event_offsets.size();
}

const clap_event_header_t* EventList::Get(const uint32_t index) const
{
	if (index < event_offsets.size()) {
		return reinterpret_cast<const clap_event_header_t*>(
		        &event_data[event_offsets[index]]);
	}
	return nullptr;
}

const clap_input_events* EventList::GetInputEvents() const
{
	return &input_events;
}

const clap_output_events* EventList::GetOutputEvents() const
{
	return &output_events;
}

} // namespace Clap
