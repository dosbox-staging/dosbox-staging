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

#ifndef DOSBOX_MOUSE_CONFIG_H
#define DOSBOX_MOUSE_CONFIG_H

#include "dosbox.h"

#include "mouse.h"


// ***************************************************************************
// Predefined calibration
// ***************************************************************************

struct MousePredefined {
    // Mouse equalization for consistent user experience - please adjust
    // values so that on full screen, with RAW mouse input, the mouse feel
    // is similar to Windows 3.11 for Workgroups with PS/2 mouse driver
    // and default settings
    const float sensitivity_dos = 1.0f;
    const float sensitivity_ps2 = 1.0f;
    const float sensitivity_vmm = 3.0f;
    const float sensitivity_com = 1.0f;
    // Constants to move 'intersection point' for the acceleration curve
    // Requires raw mouse input, otherwise there is no effect
    // Larger values = higher mouse acceleration
    const float acceleration_dos = 1.0f;
    const float acceleration_vmm = 1.0f;

    const uint8_t sensitivity_user_default = 50;
    const uint8_t sensitivity_user_max     = 99;
};

extern MousePredefined mouse_predefined;

// ***************************************************************************
// Configuration file content
// ***************************************************************************

enum class MouseModelPS2 : uint8_t {
    // Values must match PS/2 protocol IDs
    Standard     = 0x00,
    IntelliMouse = 0x03,
    Explorer     = 0x04,
};

enum class MouseModelCOM : uint8_t {
    NoMouse, // dummy value or no mouse
    Microsoft,
    Logitech,
    Wheel,
    MouseSystems
};

enum class MouseModelBus : uint8_t {
    NoMouse,
    Bus,
    InPort,
};

struct MouseConfig {

    // From [sdl] section

    float sensitivity_gui_x = 0.0f;
    float sensitivity_gui_y = 0.0f;
    bool  no_mouse  = false; // true = NoMouse selected in GUI
    bool  raw_input = false; // true = relative input is raw data, without
                             // host OS mouse acceleration applied

    // From [mouse] section

    bool mouse_dos_enable = false;
    bool mouse_dos_immediate = false;

    MouseModelPS2 model_ps2 = MouseModelPS2::Standard;

    MouseModelCOM model_com = MouseModelCOM::Wheel;
    bool model_com_auto_msm = true;

    // Helper functions for external modules

    static const std::vector<uint16_t> &GetValidMinRateList();
    static bool ParseSerialModel(const std::string &model_str, MouseModelCOM &model, bool &auto_msm);
};

extern MouseConfig mouse_config;

#endif // DOSBOX_MOUSE_CONFIG_H
