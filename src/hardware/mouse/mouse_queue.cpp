/*
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#include "mouse_queue.h"
#include "mouse_config.h"
#include "mouse_interfaces.h"

#include <array>

#include "checks.h"
#include "pic.h"

CHECK_NARROWING();

// ***************************************************************************
// Debug code, normally not enabled
// ***************************************************************************

// #define DEBUG_QUEUE_ENABLE

#ifndef DEBUG_QUEUE_ENABLE
#	define DEBUG_QUEUE(...) ;
#else
// TODO: after migrating to C++20, allow to skip the 2nd argument by using
// '__VA_OPT__(,) __VA_ARGS__' instead of ', __VA_ARGS__'
#	define DEBUG_QUEUE(fmt, ...) \
		LOG_INFO("(queue) %04d: " fmt, DEBUG_GetDiffTicks(), __VA_ARGS__);

static uint32_t DEBUG_GetDiffTicks()
{
	static uint32_t previous_ticks = 0;
	uint32_t diff_ticks            = 0;

	if (previous_ticks)
		diff_ticks = PIC_Ticks - previous_ticks;

	previous_ticks = PIC_Ticks;
	return diff_ticks;
}

#endif

// ***************************************************************************
// Mouse event queue implementation
// ***************************************************************************

void mouse_queue_tick(uint32_t)
{
	MouseQueue::GetInstance().Tick();
}

MouseQueue &MouseQueue::GetInstance()
{
	static MouseQueue mouse_queue;
	return mouse_queue;
}

MouseQueue::~MouseQueue()
{
	PIC_RemoveEvents(mouse_queue_tick);
}

void MouseQueue::SetRateDOS(const uint16_t rate_hz)
{
	// Convert rate in Hz to delay in milliseconds
	start_delay.dos_ms = MOUSE_GetDelayFromRateHz(rate_hz);
}

void MouseQueue::SetRatePS2(const uint16_t rate_hz)
{
	// Convert rate in Hz to delay in milliseconds
	start_delay.ps2_ms = MOUSE_GetDelayFromRateHz(rate_hz);
}

void MouseQueue::AddEvent(MouseEvent &ev)
{
	DEBUG_QUEUE("AddEvent:   %s %s",
	            ev.request_dos         ? "DOS"
	            : "---" ev.request_ps2 ? "PS2"
	                                   : "---");

	// Prevent unnecessary processing
	AggregateDosEvents(ev);
	if (!ev.request_dos && !ev.request_ps2)
		return; // event not relevant any more

	bool restart_timer = false;
	if (ev.request_dos) {
		if (!HasEventDos() && timer_in_progress && !delay.dos_ms) {
			DEBUG_QUEUE("AddEvent: restart timer for %s", "DOS");
			// We do not want the timer to start only then PS/2
			// event gets processed - for minimum latency it is
			// better to restart the timer
			restart_timer = true;
		}

		if (ev.dos_moved) {
			// Mouse has moved
			pending_dos_moved = true;
		} else if (ev.dos_wheel) {
			// Wheel has moved
			pending_dos_wheel = true;
		} else {
			// Button press/release
			pending_dos_button        = true;
			pending_dos_buttons_state = ev.dos_buttons;
		}
	}

	if (ev.request_ps2) {
		if (!HasEventPS2() && timer_in_progress && !delay.ps2_ms) {
			DEBUG_QUEUE("AddEvent: restart timer for %s", "PS2");
			// We do not want the timer to start only when other
			// event gets processed - for minimum latency it is
			// better to restart the timer
			restart_timer = true;
		}

		// Events for PS/2 interface (or virtual machine compatible
		// drivers) do not carry any information - they are only
		// notifications that new data is available
		pending_ps2 |= ev.request_ps2;
	}

	if (restart_timer) {
		timer_in_progress = false;
		PIC_RemoveEvents(mouse_queue_tick);
		UpdateDelayCounters();
		StartTimerIfNeeded();
	} else if (!timer_in_progress) {
		DEBUG_QUEUE("ActivateIRQ, in %s", __FUNCTION__);
		// If no timer in progress, handle the event now
		PIC_ActivateIRQ(mouse_predefined.IRQ_PS2);
	}
}

void MouseQueue::AggregateDosEvents(MouseEvent &ev)
{
	// We do not need duplicate move / wheel events
	if (pending_dos_moved)
		ev.dos_moved = false;
	if (pending_dos_wheel)
		ev.dos_wheel = false;

	// Same for mouse buttons - but in such case always update button data
	if (pending_dos_button && ev.dos_button) {
		ev.dos_button             = false;
		pending_dos_buttons_state = ev.dos_buttons;
	}

	// Check if we still need this event
	if (!ev.dos_moved && !ev.dos_wheel && !ev.dos_button)
		ev.request_dos = false;
}

void MouseQueue::FetchEvent(MouseEvent &ev)
{
	// First try (prioritized) DOS events
	if (HasReadyEventDos()) {
		DEBUG_QUEUE("FetchEvent %s", "DOS");
		// Mark event as DOS one
		ev.request_dos = true;
		ev.dos_moved   = pending_dos_moved;
		ev.dos_button  = pending_dos_button;
		ev.dos_wheel   = pending_dos_wheel;
		ev.dos_buttons = pending_dos_buttons_state;
		// Set delay before next DOS events
		delay.dos_ms = start_delay.dos_ms;
		// Clear event information
		pending_dos_moved  = false;
		pending_dos_button = false;
		pending_dos_wheel  = false;
		return;
	}

	// Now try PS/2 event
	if (HasReadyEventPS2()) {
		DEBUG_QUEUE("FetchEvent %s", "PS2");
		// Set delay before next PS/2 events
		delay.ps2_ms = start_delay.ps2_ms;
		// PS/2 events are really dummy - merely a notification
		// that something has happened and driver has to react
		ev.request_ps2 = true;
		pending_ps2    = false;
		return;
	}

	// Nothing to provide to interrupt handler,
	// event will stay empty
}

void MouseQueue::ClearEventsDOS()
{
	// Clear DOS relevant part of the queue
	pending_dos_moved  = false;
	pending_dos_button = false;
	pending_dos_wheel  = false;
	delay.dos_ms       = 0;

	// If timer is not needed, stop it
	if (!HasEventAny()) {
		timer_in_progress = false;
		PIC_RemoveEvents(mouse_queue_tick);
	}
}

void MouseQueue::StartTimerIfNeeded()
{
	// Do nothing if timer is already in progress
	if (timer_in_progress)
		return;

	bool timer_needed = false;
	uint8_t delay_ms  = UINT8_MAX; // dummy delay, will never be used

	if (HasEventPS2() || delay.ps2_ms) {
		timer_needed = true;
		delay_ms     = std::min(delay_ms, delay.ps2_ms);
	}
	if (HasEventDos() || delay.dos_ms) {
		timer_needed = true;
		delay_ms     = std::min(delay_ms, delay.dos_ms);
	}

	// If queue is empty and all expired, we need no timer
	if (!timer_needed)
		return;

	// Enforce some non-zero delay between events; needed
	// for example if DOS interrupt handler is busy
	delay_ms = std::max(delay_ms, static_cast<uint8_t>(1));

	// Start the timer
	DEBUG_QUEUE("StartTimer, %d", delay_ms);
	pic_ticks_start   = PIC_Ticks;
	timer_in_progress = true;
	PIC_AddEvent(mouse_queue_tick, static_cast<double>(delay_ms));
}

void MouseQueue::UpdateDelayCounters()
{
	const uint32_t tmp = (PIC_Ticks > pic_ticks_start)
	                           ? (PIC_Ticks - pic_ticks_start)
	                           : 1;
	uint8_t elapsed    = static_cast<uint8_t>(
                std::min(tmp, static_cast<uint32_t>(UINT8_MAX)));
	if (!pic_ticks_start)
		elapsed = 1;

	auto calc_new_delay = [](const uint8_t base_delay, const uint8_t elapsed) {
		return static_cast<uint8_t>((base_delay > elapsed) ? (base_delay - elapsed) : 0);
	};

	delay.dos_ms = calc_new_delay(delay.dos_ms, elapsed);
	delay.ps2_ms = calc_new_delay(delay.ps2_ms, elapsed);

	pic_ticks_start = 0;
}

void MouseQueue::Tick()
{
	DEBUG_QUEUE("%s", "Tick");

	timer_in_progress = false;
	UpdateDelayCounters();

	// If we have anything to pass to guest side via INT74, activate
	// interrupt; otherwise start the timer again
	if (HasReadyEventDos() || HasReadyEventPS2()) {
		DEBUG_QUEUE("ActivateIRQ, in %s", __FUNCTION__);
		PIC_ActivateIRQ(mouse_predefined.IRQ_PS2);
	} else
		StartTimerIfNeeded();
}

bool MouseQueue::HasEventDos() const
{
	return pending_dos_moved || pending_dos_button || pending_dos_wheel;
}

bool MouseQueue::HasEventPS2() const
{
	return pending_ps2;
}

bool MouseQueue::HasEventAny() const
{
	return HasEventDos() || HasEventPS2();
}

bool MouseQueue::HasReadyEventDos() const
{
	return HasEventDos() && !delay.dos_ms &&
	       // do not launch DOS callback if it's busy
	       !mouse_shared.dos_cb_running;
}

bool MouseQueue::HasReadyEventPS2() const
{
	return HasEventPS2() && !delay.ps2_ms;
}
