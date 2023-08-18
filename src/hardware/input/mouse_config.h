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

	// Constant to move 'intersection point' for the acceleration curve
	// Requires raw mouse input, otherwise there is no effect
	// Larger values = higher mouse acceleration
	const float acceleration_vmm = 1.0f;

	// Maximum allowed user sensitivity value
	const int16_t sensitivity_user_max = 999;

	// IRQ used by PS/2 mouse - do not change unless you really know
	// what you are doing!
	const uint8_t IRQ_PS2 = 12;
};

extern MousePredefined mouse_predefined;

// ***************************************************************************
// Configuration file content
// ***************************************************************************

enum class MouseCapture : uint8_t { Seamless, OnClick, OnStart, NoMouse };

enum class MouseModelPS2 : uint8_t {
	NoMouse      = 0xff,
	// Values below must match PS/2 protocol IDs
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

struct MouseConfig {
	MouseCapture capture = MouseCapture::OnStart;
	bool middle_release  = true;

	int16_t sensitivity_x    = 50; // default sensitivity values
	int16_t sensitivity_y    = 50;
	bool raw_input           = false; // true = relative input is raw data
	bool multi_display_aware = false;

	bool dos_driver    = false; // whether DOS virtual mouse driver should be enabled
	bool dos_immediate = false;

	MouseModelPS2 model_ps2 = MouseModelPS2::Standard;

	MouseModelCOM model_com = MouseModelCOM::Wheel;
	bool model_com_auto_msm = true;

#ifdef EXPERIMENTAL_VIRTUALBOX_MOUSE
	bool is_vmware_mouse_enabled     = false;
	bool is_virtualbox_mouse_enabled = false;
#else
	bool is_vmware_mouse_enabled     = true;
	bool is_virtualbox_mouse_enabled = false;
#endif

	// Helper functions for external modules

	static const std::vector<uint16_t> &GetValidMinRateList();
	static bool ParseCaptureType(const std::string_view capture_str,
	                             MouseCapture& capture);
	static bool ParseCOMModel(const std::string_view model_str,
	                          MouseModelCOM& model, bool& auto_msm);
	static bool ParsePS2Model(const std::string_view model_str,
	                          MouseModelPS2& model);
};

extern MouseConfig mouse_config;

#endif // DOSBOX_MOUSE_CONFIG_H
