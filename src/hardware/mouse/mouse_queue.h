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

#ifndef DOSBOX_MOUSE_QUEUE_H
#define DOSBOX_MOUSE_QUEUE_H

#include "mouse_common.h"

class MouseQueue final {
public:
	static MouseQueue &GetInstance();

	void SetRateDOS(const uint16_t rate_hz); // for DOS mouse driver
	void SetRatePS2(const uint16_t rate_hz); // for PS/2 AUX port mice

	void AddEvent(MouseEvent &ev);
	void FetchEvent(MouseEvent &ev);
	void ClearEventsDOS();
	void StartTimerIfNeeded();

private:
	MouseQueue()                              = default;
	~MouseQueue()                             = delete;
	MouseQueue(const MouseQueue &)            = delete;
	MouseQueue &operator=(const MouseQueue &) = delete;

	void Tick();
	friend void mouse_queue_tick(uint32_t);

	void AggregateDosEvents(MouseEvent &ev);
	void UpdateDelayCounters();
	uint8_t ClampStartDelay(float value_ms) const;

	struct { // initial value of delay counters, in milliseconds
		uint8_t dos_ms = 5;
		uint8_t ps2_ms = 5;
	} start_delay = {};

	// Time in milliseconds which has to elapse before event can take place
	struct {
		uint8_t dos_ms = 0;
		uint8_t ps2_ms = 0;
	} delay = {};

	// Pending events, waiting to be passed to guest system
	bool pending_dos_moved  = false;
	bool pending_dos_button = false;
	bool pending_dos_wheel  = false;
	bool pending_ps2        = false;

	MouseButtons12S pending_dos_buttons_state = 0;

	bool timer_in_progress   = false;
	uint32_t pic_ticks_start = 0; // PIC_Ticks value when timer starts

	// Helpers to check if there are events in the queue
	bool HasEventDos() const;
	bool HasEventPS2() const;
	bool HasEventAny() const;

	// Helpers to check if there are events ready to be handled
	bool HasReadyEventDos() const;
	bool HasReadyEventPS2() const;
};

#endif // DOSBOX_MOUSE_QUEUE_H
