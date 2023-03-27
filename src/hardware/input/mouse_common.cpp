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

#include "mouse_common.h"

#include <algorithm>

#include "checks.h"
#include "pic.h"

CHECK_NARROWING();

// ***************************************************************************
// Common variables
// ***************************************************************************

MouseInfo mouse_info     = {};
MouseShared mouse_shared = {};

// ***************************************************************************
// Common helper calculations
// ***************************************************************************

float MOUSE_GetBallisticsCoeff(const float speed)
{
	// This routine provides a function for mouse ballistics
	// (cursor acceleration), to be reused by various mouse interfaces.
	// Since this is a DOS emulator, the acceleration model is
	// based on a historic PS/2 mouse scaling specification, desribed
	// for exampel here:
	// - https://wiki.osdev.org/Mouse_Input

	// Input: mouse speed
	// Output: acceleration coefficient (1.0f for speed >= 6.0f)

	// NOTE: If we don't have raw mouse input, stay with flat profile;
	// in such case the acceleration is already handled by the host OS,
	// adding our own could lead to hard to predict (most likely
	// undesirable) effects

	constexpr float a      = 0.017153417f;
	constexpr float b      = 0.382477002f;
	constexpr float lowest = 0.5f;

	// Normal PS/2 mouse 2:1 scaling algorithm is just a substitution:
	// 0 => 0, 1 => 1, 2 => 1, 3 => 3, 4 => 6, 5 => 9, other x => x * 2
	// and the same for negatives. But we want smooth cursor movement,
	// therefore we use approximation model (least square regression,
	// 3rd degree polynomial, on points -6, -5, ..., 0, ... , 5, 6,
	// here scaled to give f(6.0) = 6.0). Polynomial would be:
	//
	// f(x) = a*(x^3) + b*(x^1) = x*(a*(x^2) + b)
	//
	// This C++ function provides not the full polynomial, but rather
	// a coefficient (0.0f ... 1.0f) calculated from supplied speed,
	// by which the relative mouse measurement should be multiplied

	if (speed > -6.0f && speed < 6.0f)
		return std::max((a * speed * speed + b), lowest);
	else
		return 1.0f;

	// Please consider this algorithm as yet another nod to the past,
	// one more small touch of 20th century PC computing history :)
}

uint8_t MOUSE_GetDelayFromRateHz(const uint16_t rate_hz)
{
	assert(rate_hz);
	const auto period_in_ms = std::lround(1000.0f / MOUSE_ClampRateHz(rate_hz));
	return static_cast<uint8_t>(period_in_ms);
}

float MOUSE_ClampRelativeMovement(const float rel)
{
	// Enforce sane upper limit of relative mouse movement
	return std::clamp(rel, -2048.0f, 2048.0f);
}

uint16_t MOUSE_ClampRateHz(const uint16_t rate_hz)
{
	constexpr uint16_t rate_min = 10;
	constexpr uint16_t rate_max = 500;

	return std::clamp(rate_hz, rate_min, rate_max);
}

// ***************************************************************************
// Mouse speed calculation
// ***************************************************************************

MouseSpeedCalculator::MouseSpeedCalculator(const float scaling)
        : ticks_start(PIC_Ticks),
          scaling(scaling * 1000.0f) // to convert from units/ms to units/s
{}

float MouseSpeedCalculator::Get() const
{
	return speed;
}

void MouseSpeedCalculator::Update(const float delta)
{
	// For the measurement require at least certain amount
	// of PIC ticks; if too much time passes without meaningful
	// movement, consider mouse speed to be 0
	constexpr uint32_t min_ticks = 40;
	constexpr uint32_t max_ticks = 400;

	// Calculate time from the beginning of measurement
	const auto diff_ticks = PIC_Ticks - ticks_start;

	// Try to calculate cursor speed
	if (PIC_Ticks >= ticks_start) {
		if (diff_ticks > max_ticks)
			// Do not wait any more for the movement, consider speed
			// to be 0
			speed = 0.0f;
		else {
			// Update distance travelled by the cursor
			distance += delta;

			// Make sure enough time passed for accurate speed
			// calculation
			if (diff_ticks < min_ticks)
				return;

			// Calculate cursor speed
			speed = scaling * distance / static_cast<float>(diff_ticks);
		}
	}

	// Note: if PIC_Ticks < ticks_start, we have a counter overflow - this
	// can only happen if emulator is run for many weeks at a time. In such
	// case we just assume previous speed is still valid, for simplicity.

	// Start new measurement
	distance    = 0.0f;
	ticks_start = PIC_Ticks;
}

// ***************************************************************************
// Types for storing mouse buttons
// ***************************************************************************

MouseButtons12 &MouseButtons12::operator=(const MouseButtons12 &other)
{
	_data = other._data;
	return *this;
}

MouseButtons345 &MouseButtons345::operator=(const MouseButtons345 &other)
{
	_data = other._data;
	return *this;
}

MouseButtonsAll &MouseButtonsAll::operator=(const MouseButtonsAll &other)
{
	_data = other._data;
	return *this;
}

MouseButtons12S &MouseButtons12S::operator=(const MouseButtons12S &other)
{
	_data = other._data;
	return *this;
}
