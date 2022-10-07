/*
 *  Copyright (C) 2022       The DOSBox Staging Team
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

MouseInfo   mouse_info;
MouseShared mouse_shared;
MouseVideo  mouse_video;

// ***************************************************************************
// Common helper calculations
// ***************************************************************************

float MOUSE_GetBallisticsCoeff(const float speed)
{
    // This routine provides a function for mouse ballistics
    // (cursor acceleration), to be reused by various mouse interfaces.
    // Since this is a DOS emulator, the acceleration model is
    // based on a historic PS/2 mouse specification.

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
    const auto tmp = std::lround(1000.0f / MOUSE_ClampRateHz(rate_hz));
    return static_cast<uint8_t>(tmp);
}

float MOUSE_ClampRelativeMovement(const float rel)
{
    // Enforce sane upper limit of relative mouse movement
    return std::clamp(rel, -2048.0f, 2048.0f);
}

uint16_t MOUSE_ClampRateHz(const uint16_t rate_hz)
{
    constexpr auto rate_min = static_cast<uint16_t>(10);
    constexpr auto rate_max = static_cast<uint16_t>(500);

    return std::clamp(rate_hz, rate_min, rate_max);
}

int8_t MOUSE_ClampToInt8(const int32_t val)
{
    const auto tmp = std::clamp(val,
                                static_cast<int32_t>(INT8_MIN),
                                static_cast<int32_t>(INT8_MAX));

    return static_cast<int8_t>(tmp);
}

int16_t MOUSE_ClampToInt16(const int32_t val)
{
    const auto tmp = std::clamp(val,
                                static_cast<int32_t>(INT16_MIN),
                                static_cast<int32_t>(INT16_MAX));

    return static_cast<int16_t>(tmp);
}

// ***************************************************************************
// Mouse speed calculation
// ***************************************************************************

MouseSpeedCalculator::MouseSpeedCalculator(const float scaling) :
    scaling(scaling * 1000.0f) // to convert from units/ms to units/s
{
    clock_time_start = std::chrono::steady_clock::now();
    pic_ticks_start  = PIC_Ticks;
}

float MouseSpeedCalculator::Get() const
{
    return speed;
}

void MouseSpeedCalculator::Update(const float delta)
{
    constexpr auto n = static_cast<float>(std::chrono::steady_clock::period::num);
    constexpr auto d = static_cast<float>(std::chrono::steady_clock::period::den);
    constexpr auto period_ms = 1000.0f * n / d;
    // For the measurement duration require no more than 400 milliseconds
    // and at least 10 times the clock granularity
    constexpr uint32_t max_diff_ms = 400;
    constexpr uint32_t min_diff_ms = std::max(static_cast<uint32_t>(1),
                                              static_cast<uint32_t>(period_ms * 10));
    // Require at least 40 ticks of PIC emulator to pass
    constexpr uint32_t min_diff_ticks = 40;

    // Get current time, calculate difference
    const auto time_now   = std::chrono::steady_clock::now();
    const auto diff_time  = time_now - clock_time_start;
    const auto diff_ticks = PIC_Ticks - pic_ticks_start;
    const auto diff_ms    =
            std::chrono::duration_cast<std::chrono::milliseconds>(diff_time).count();

    // Try to calculate cursor speed
    if (diff_ms > std::max(max_diff_ms, static_cast<uint32_t>(10 * period_ms))) {
        // Do not wait any more for movement, consider speed to be 0
        speed = 0.0f;
    } else if (diff_ms >= 0) {
        // Update distance travelled by the cursor
        distance += delta;

        // Make sure enough time passed for accurate speed calculation
        if (diff_ms < min_diff_ms || (diff_ticks < min_diff_ticks && diff_ticks > 0))
            return;

        // Update cursor speed
        speed = scaling * distance / static_cast<float>(diff_ms);
    }

    // Start new measurement
    distance         = 0.0f;
    clock_time_start = time_now;
    pic_ticks_start  = PIC_Ticks;
}

// ***************************************************************************
// Types for storing mouse buttons
// ***************************************************************************

MouseButtons12 &MouseButtons12::operator=(const MouseButtons12 &other)
{
    data = other.data;
    return *this;
}

MouseButtons345 &MouseButtons345::operator=(const MouseButtons345 &other)
{
    data = other.data;
    return *this;
}

MouseButtonsAll &MouseButtonsAll::operator=(const MouseButtonsAll &other)
{
    data = other.data;
    return *this;
}

MouseButtons12S &MouseButtons12S::operator=(const MouseButtons12S &other)
{
    data = other.data;
    return *this;
}
