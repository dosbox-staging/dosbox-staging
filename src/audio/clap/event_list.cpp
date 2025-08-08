// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "event_list.h"

#include <cassert>

#include "checks.h"
#include "logging.h"
#include "midi/midi.h"

CHECK_NARROWING();

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
	// Start with reasonable initial sizes to avoid reallocations
	constexpr auto InitialNumBytes  = 8192;
	constexpr auto InitialNumEvents = 1024;

	event_data.reserve(InitialNumBytes);
	event_offsets.reserve(InitialNumEvents);

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
	ev.data[2]         = (msg.size() == 3) ? msg[2] : 0;

	const auto new_event_offset = event_data.size();
	event_offsets.emplace_back(new_event_offset);

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
	ev.size            = check_cast<uint32_t>(msg.size());

	// We reserved `MaxMidiSysExBytes` for `sysex_data` to avoid
	// reallocations, so make sure we never trigger a realloc here.
	assert(sysex_data.size() + msg.size() <= MaxMidiSysExBytes);
	sysex_data.insert(sysex_data.end(), msg.begin(), msg.end());

	const auto new_event_offset = event_data.size();
	event_offsets.emplace_back(new_event_offset);

	const auto ev_start = reinterpret_cast<uint8_t*>(&ev);
	const auto ev_end   = ev_start + sizeof(ev);

	event_data.insert(event_data.end(), ev_start, ev_end);
}

uint32_t EventList::Size() const
{
	return check_cast<uint32_t>(event_offsets.size());
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
