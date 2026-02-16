// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MOUSE_CONFIG_H
#define DOSBOX_MOUSE_CONFIG_H

#include "hardware/input/mouse.h"

#include "dosbox.h"

// ***************************************************************************
// Predefined calibration
// ***************************************************************************

namespace Mouse {

// Mouse equalization for consistent user experience - please adjust values so
// that on full screen, with RAW mouse input, the mouse feel is similar to
// Windows 3.11 for Workgroups with PS/2 mouse driver and default settings
constexpr float SensitivityDos = 1.0f;
constexpr float SensitivityPs2 = 1.0f;
constexpr float SensitivityVmm = 3.0f;
constexpr float SensitivityCom = 1.0f;

// Constant to move 'intersection point' for the acceleration curve
// Requires raw mouse input, otherwise there is no effect
// Larger values = higher mouse acceleration
constexpr float AccelerationVmm = 1.0f;

// Default and maximum/minimum allowed user sensitivity value
constexpr int DefaultSensitivity = 100;
constexpr int MaxSensitivity     = 999;
constexpr int MinSensitivity     = -MaxSensitivity;

// Default and maximum/minimum allowed mouse mickey threshold
constexpr int DefaultMoveThreshold = 1;
constexpr int MinMoveThreshold     = 1;
constexpr int MaxMoveThreshold     = 9;

// Default builtin mouse driver options
constexpr auto DefaultDriverOptions = "";

// PS/2 mouse IRQ, do not change unless you really know what you are doing!
constexpr uint8_t IrqPs2 = 12;

} // namespace Mouse

// ***************************************************************************
// Configuration file content
// ***************************************************************************

enum class MouseCapture { Seamless, OnClick, OnStart, NoMouse };

enum class MouseModelDos {
	TwoButton,
	ThreeButton,
	Wheel,
};

enum class MouseModelPs2 : uint8_t {
	NoMouse      = 0xff,
	// Values below must match PS/2 protocol IDs
	Standard     = 0x00,
	IntelliMouse = 0x03,
	Explorer     = 0x04,
};

struct MouseConfig {
	MouseCapture capture = MouseCapture::OnStart;
	bool middle_release  = true;

	float sensitivity_coeff_x = 1.0f;
	float sensitivity_coeff_y = 1.0f;

	bool raw_input           = false; // true = relative input is raw data
	bool multi_display_aware = false;

	bool dos_driver_autoexec = false;
	bool dos_driver_no_tsr   = false;

	bool dos_driver_modern         = false;
	bool dos_driver_immediate      = false;
	bool dos_driver_no_granularity = false;

	float dos_driver_move_threshold_x = 1.0f;
	float dos_driver_move_threshold_y = 1.0f;

	MouseModelDos model_dos = MouseModelDos::TwoButton;

	MouseModelPs2 model_ps2 = MouseModelPs2::Standard;

	MouseModelCom model_com = MouseModelCom::Wheel;
	bool model_com_auto_msm = true;

	bool is_vmware_mouse_enabled     = false;
	bool is_virtualbox_mouse_enabled = false;

	// Helper functions for external modules

	static const std::vector<uint16_t>& GetValidMinRateList();
};

extern MouseConfig mouse_config;

#endif // DOSBOX_MOUSE_CONFIG_H
