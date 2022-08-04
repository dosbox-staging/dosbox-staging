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

#ifndef DOSBOX_MOUSE_CORE_H
#define DOSBOX_MOUSE_CORE_H

#include "dosbox.h"

#include "bit_view.h"

// IntelliMouse Explorer emulation is currently disabled - there is probably
// no way to test it. The IntelliMouse 3.0 software can use it, but it seems
// to require physical PS/2 mouse registers to work correctly, and these
// are not emulated yet.

// #define ENABLE_EXPLORER_MOUSE

// ***************************************************************************
// Common defines
// ***************************************************************************

// Mouse equalization for consistent user experience - please adjust values
// so that on full screen, with RAW mouse input, the mouse feel is similar
// to Windows 3.11 for Workgroups with PS/2 mouse driver and default settings
constexpr float sensitivity_dos  = 1.0f;
constexpr float sensitivity_vmm  = 3.0f;
// Constants to move 'intersection point' for the acceleration curve
// Requires raw mouse input, otherwise there is no effect
// Larger values = higher mouse acceleration
constexpr float acceleration_vmm = 1.0f;

// ***************************************************************************
// Common structures and variables
// ***************************************************************************

class MouseShared {
public:
    bool active_bios = false; // true = BIOS has a registered callback
    bool active_dos  = false; // true = DOS driver has a functioning callback
    bool active_vmm  = false; // true = VMware-compatible driver is active

    bool dos_cb_running = false; // true = DOS callback is running
};

class MouseVideo {
public:
    bool fullscreen = true;

    uint16_t res_x = 640; // resolution to which guest image is scaled,
    uint16_t res_y = 400; // excluding black borders

    uint16_t clip_x = 0; // clipping = size of black border (one side)
    uint16_t clip_y = 0;

    // TODO: once the mechanism is fully implemented, provide an option
    // in the configuration file to enable it
    bool autoseamless = false;
};

extern MouseShared mouse_shared;
extern MouseVideo mouse_video;

extern bool mouse_is_captured;

// ***************************************************************************
// Types for storing mouse buttons
// ***************************************************************************

// NOTE: bit layouts has to be compatible with each other and with INT 33
// (DOS driver) functions 0x03 / 0x05 / 0x06 and it's callback interface

union MouseButtons12 { // for storing left and right buttons only
    uint8_t data = 0;

    bit_view<0, 1> left;
    bit_view<1, 1> right;

    MouseButtons12() : data(0) {}
    MouseButtons12(const uint8_t data) : data(data) {}
    MouseButtons12(const MouseButtons12 &other) : data(other.data) {}
    MouseButtons12 &operator=(const MouseButtons12 &other)
    {
        data = other.data;
        return *this;
    }
};

union MouseButtons345 { // for storing middle and extra buttons
    uint8_t data = 0;

    bit_view<2, 1> middle;
    bit_view<3, 1> extra_1;
    bit_view<4, 1> extra_2;

    MouseButtons345() : data(0) {}
    MouseButtons345(const uint8_t data) : data(data) {}
    MouseButtons345(const MouseButtons345 &other) : data(other.data) {}
    MouseButtons345 &operator=(const MouseButtons345 &other)
    {
        data = other.data;
        return *this;
    }
};

union MouseButtonsAll { // for storing all 5 mouse buttons
    uint8_t data = 0;

    bit_view<0, 1> left;
    bit_view<1, 1> right;
    bit_view<2, 1> middle;
    bit_view<3, 1> extra_1;
    bit_view<4, 1> extra_2;

    MouseButtonsAll() : data(0) {}
    MouseButtonsAll(const uint8_t data) : data(data) {}
    MouseButtonsAll(const MouseButtonsAll &other) : data(other.data) {}
    MouseButtonsAll &operator=(const MouseButtonsAll &other)
    {
        data = other.data;
        return *this;
    }
};

union MouseButtons12S { // use where buttons 3/4/5 are squished into a virtual
                    // middle button
    uint8_t data = 0;

    bit_view<0, 1> left;
    bit_view<1, 1> right;
    bit_view<2, 1> middle;

    MouseButtons12S() : data(0) {}
    MouseButtons12S(const uint8_t data) : data(data) {}
    MouseButtons12S(const MouseButtons12S &other) : data(other.data) {}
    MouseButtons12S &operator=(const MouseButtons12S &other)
    {
        data = other.data;
        return *this;
    }
};

// ***************************************************************************
// Main mouse module
// ***************************************************************************

float MOUSE_GetBallisticsCoeff(const float speed);
float MOUSE_ClampRelativeMovement(const float rel);

void MOUSE_NotifyMovedFake(); // for VMware protocol support
void MOUSE_NotifyRateDOS(const uint8_t rate_hz);
void MOUSE_NotifyRatePS2(const uint8_t rate_hz);
void MOUSE_NotifyResetDOS();
void MOUSE_NotifyStateChanged();

// ***************************************************************************
// Serial mouse
// ***************************************************************************

// - needs relative movements
// - understands up to 3 buttons
// - needs index of button which changed state

void MOUSESERIAL_NotifyMoved(const float x_rel, const float y_rel);
void MOUSESERIAL_NotifyPressedReleased(const MouseButtons12S buttons_12S,
                                       const uint8_t idx);
void MOUSESERIAL_NotifyWheel(const int16_t w_rel);

// ***************************************************************************
// PS/2 mouse
// ***************************************************************************

void MOUSEPS2_Init();
void MOUSEPS2_UpdateButtonSquish();
void MOUSEPS2_PortWrite(const uint8_t byte);
void MOUSEPS2_UpdatePacket();
bool MOUSEPS2_SendPacket();

// - needs relative movements
// - understands up to 5 buttons in Intellimouse Explorer mode
// - understands up to 3 buttons in other modes
// - provides a way to generate dummy event, for VMware mouse integration

bool MOUSEPS2_NotifyMoved(const float x_rel, const float y_rel);
bool MOUSEPS2_NotifyPressedReleased(const MouseButtons12S buttons_12S,
                                    const MouseButtonsAll buttons_all);
bool MOUSEPS2_NotifyWheel(const int16_t w_rel);

// ***************************************************************************
// BIOS mouse interface for PS/2 mouse
// ***************************************************************************

uintptr_t MOUSEBIOS_DoCallback();

// ***************************************************************************
// VMware protocol extension for PS/2 mouse
// ***************************************************************************

void MOUSEVMM_Init();
void MOUSEVMM_NewScreenParams(const uint16_t x_abs, const uint16_t y_abs);
void MOUSEVMM_Deactivate();

// - needs absolute mouse position
// - understands up to 3 buttons

bool MOUSEVMM_NotifyMoved(const float x_rel, const float y_rel,
                          const uint16_t x_abs, const uint16_t y_abs);
bool MOUSEVMM_NotifyPressedReleased(const MouseButtons12S buttons_12S);
bool MOUSEVMM_NotifyWheel(const int16_t w_rel);

// ***************************************************************************
// DOS mouse driver
// ***************************************************************************

// This enum has to be compatible with mask in DOS driver function 0x0c
enum class MouseEventId : uint8_t {
    NotDosEvent    = 0,
    MouseHasMoved  = 1 << 0,
    PressedLeft    = 1 << 1,
    ReleasedLeft   = 1 << 2,
    PressedRight   = 1 << 3,
    ReleasedRight  = 1 << 4,
    PressedMiddle  = 1 << 5,
    ReleasedMiddle = 1 << 6,
    WheelHasMoved  = 1 << 7,
};

void MOUSEDOS_Init();
void MOUSEDOS_DrawCursor();

bool MOUSEDOS_HasCallback(const uint8_t mask);
uintptr_t MOUSEDOS_DoCallback(const uint8_t mask, const MouseButtons12S buttons_12S);
// - needs relative movements
// - understands up to 3 buttons
// - needs index of button which changed state

bool MOUSEDOS_NotifyMoved(const float x_rel,
                          const float y_rel,
                          const uint16_t x_abs,
                          const uint16_t y_abs);
bool MOUSEDOS_NotifyWheel(const int16_t w_rel);

bool MOUSEDOS_UpdateMoved();
bool MOUSEDOS_UpdateButtons(const MouseButtons12S buttons_12S);
bool MOUSEDOS_UpdateWheel();

#endif // DOSBOX_MOUSE_CORE_H
