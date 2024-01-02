/*
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_MOUSE_COMMON_H
#define DOSBOX_MOUSE_COMMON_H

#include "mouse.h"

#include "bit_view.h"

// ***************************************************************************
// Common variables
// ***************************************************************************

class MouseShared {
public:
	bool active_bios = false; // true = BIOS has a registered callback
	bool active_vmm  = false; // true = Virtual Machine Manager (VMM)
	                          //        compatible driver is active

	// true = Virtual Machine Manager (VMM) compatible mouse driver wants
	// the host to display its mouse pointer
	bool vmm_wants_pointer = false;

	bool dos_cb_running = false; // true = DOS callback is running

	// Readiness for initialization
	bool ready_init   = false; // if allowed to init in the main startup sequence
	bool ready_config = false; // if configuration was read
	bool ready_gfx    = false; // if GFX subsystem is ready

	bool started = false;

	// Resolution (screen size) in logical units to which guest image is
	// scaled, excluding black borders.
	uint32_t resolution_x = 640;
	uint32_t resolution_y = 400;
};

class MouseInfo {
public:
	std::vector<MouseInterfaceInfoEntry> interfaces = {};
	std::vector<MousePhysicalInfoEntry> physical    = {};
};

extern MouseInfo   mouse_info;   // information which can be shared externally
extern MouseShared mouse_shared; // shared internal information

// ***************************************************************************
// Common helper calculations
// ***************************************************************************

float MOUSE_SensitivityCoefficient(const int16_t user_setting);

float MOUSE_GetBallisticsCoeff(const float speed);
uint8_t MOUSE_GetDelayFromRateHz(const uint16_t rate_hz);

float MOUSE_ClampRelativeMovement(const float rel);
uint16_t MOUSE_ClampRateHz(const uint16_t rate_hz);

// ***************************************************************************
// Mouse speed calculation
// ***************************************************************************

class MouseSpeedCalculator final {
public:
	MouseSpeedCalculator(const float scaling);

	float Get() const;
	void Update(const float delta);

private:
	MouseSpeedCalculator()                                        = delete;
	MouseSpeedCalculator(const MouseSpeedCalculator &)            = delete;
	MouseSpeedCalculator &operator=(const MouseSpeedCalculator &) = delete;

	uint32_t ticks_start = 0;

	const float scaling;

	float distance = 0.0f;
	float speed    = 0.0f;
};

// ***************************************************************************
// Types for storing mouse buttons
// ***************************************************************************

// NOTE: bit layouts has to be compatible with each other and with INT 33
// (DOS driver) functions 0x03 / 0x05 / 0x06 and it's callback interface

union MouseButtons12 {
	// For storing left and right buttons only
	uint8_t _data = 0;

	bit_view<0, 1> left;
	bit_view<1, 1> right;

	MouseButtons12() : _data(0) {}
	MouseButtons12(const uint8_t data) : _data(data) {}
	MouseButtons12(const MouseButtons12 &other) : _data(other._data) {}
	MouseButtons12 &operator=(const MouseButtons12 &other);
};

union MouseButtons345 {
	// For storing middle and extra buttons
	uint8_t _data = 0;

	bit_view<2, 1> middle;
	bit_view<3, 1> extra_1;
	bit_view<4, 1> extra_2;

	MouseButtons345() : _data(0) {}
	MouseButtons345(const uint8_t data) : _data(data) {}
	MouseButtons345(const MouseButtons345 &other) : _data(other._data) {}
	MouseButtons345 &operator=(const MouseButtons345 &other);
};

union MouseButtonsAll {
	// For storing all 5 mouse buttons
	uint8_t _data = 0;

	bit_view<0, 1> left;
	bit_view<1, 1> right;
	bit_view<2, 1> middle;
	bit_view<3, 1> extra_1;
	bit_view<4, 1> extra_2;

	MouseButtonsAll() : _data(0) {}
	MouseButtonsAll(const uint8_t data) : _data(data) {}
	MouseButtonsAll(const MouseButtonsAll &other) : _data(other._data) {}
	MouseButtonsAll &operator=(const MouseButtonsAll &other);
};

union MouseButtons12S {
	// To be used where buttons 3/4/5 are squished
	// into a virtual middle button
	uint8_t _data = 0;

	bit_view<0, 1> left;
	bit_view<1, 1> right;
	bit_view<2, 1> middle;

	MouseButtons12S() : _data(0) {}
	MouseButtons12S(const uint8_t data) : _data(data) {}
	MouseButtons12S(const MouseButtons12S &other) : _data(other._data) {}
	MouseButtons12S &operator=(const MouseButtons12S &other);
};

#endif // DOSBOX_MOUSE_COMMON_H
