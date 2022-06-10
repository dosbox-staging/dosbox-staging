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

#include "mouse.h"

#include "../hardware/serialport/serialmouse.h"

// Implementation here is very primitive, it mainly just passes notifications
// to registered listeners, which emulate a particular mouse on a particular
// serial (COM) port

static std::vector<CSerialMouse*> listeners;        // list of registered listeners
static float                      delta_x, delta_y; // accumulated mouse movement since last reported

// ***************************************************************************
// Serial interface implementation
// ***************************************************************************

void MouseSER_RegisterListener(CSerialMouse *listener) {
    if (listener)
        listeners.push_back(listener);
}

void MouseSER_UnRegisterListener(CSerialMouse *listener) {
    auto iter = std::find(listeners.begin(), listeners.end(), listener);
    if (iter != listeners.end())
        listeners.erase(iter);
}

void MouseSER_NotifyMoved(int32_t x_rel, int32_t y_rel) {
    static constexpr float MAX = 16384.0f;

    delta_x += std::clamp(x_rel * mouse_config.sensitivity_x, -MAX, MAX);
    delta_y += std::clamp(y_rel * mouse_config.sensitivity_y, -MAX, MAX);

    int16_t dx = static_cast<int16_t>(std::round(delta_x));
    int16_t dy = static_cast<int16_t>(std::round(delta_y));

    if (dx != 0 || dy != 0) {
        for (auto &listener : listeners)
            listener->onMouseEventMoved(dx, dy);
        delta_x -= dx;
        delta_y -= dy;
    }
}

void MouseSER_NotifyPressed(uint8_t buttons_12S, uint8_t idx) {
    for (auto &listener : listeners)
        listener->onMouseEventButton(buttons_12S, idx);
}

void MouseSER_NotifyReleased(uint8_t buttons_12S, uint8_t idx) {
    for (auto &listener : listeners)
        listener->onMouseEventButton(buttons_12S, idx);
}

void MouseSER_NotifyWheel(int32_t w_rel) {
    for (auto &listener : listeners)
        listener->onMouseEventWheel(std::clamp(w_rel, -0x80, 0x7f));
}
