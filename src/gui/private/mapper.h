// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUI_PRIVATE_MAPPER_H
#define DOSBOX_GUI_PRIVATE_MAPPER_H

#include <optional>
#include <span>
#include <utility>

#include <SDL3/SDL_gamepad.h>

// Scales an analog stick position from the circle to the square, so a full
// diagonal reaches full deflection on both axes. The direction stays the same;
// the result is clamped to the int16_t range of SDL's axis values.
std::pair<int, int> CircleToSquare(int x, int y);

/**
 * Find the joystick axis that an SDL gamepad binds to a gamepad axis.
 *
 * @param bindings     The gamepad's bindings from SDL_GetGamepadBindings().
 * @param gamepad_axis The gamepad axis, e.g. SDL_GAMEPAD_AXIS_LEFTX.
 * @param num_axes     The number of joystick axes the caller can handle.
 *
 * @return The joystick axis if it covers the full range and is below
 * 'num_axes'; nothing for a button, hat, half axis, a missing binding or a
 * larger axis number.
 */
std::optional<int> FindFullRangeAxis(std::span<SDL_GamepadBinding* const> bindings,
                                     SDL_GamepadAxis gamepad_axis, int num_axes);

#endif // DOSBOX_GUI_PRIVATE_MAPPER_H
