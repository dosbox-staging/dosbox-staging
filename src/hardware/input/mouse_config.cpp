/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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

#include "mouse_config.h"
#include "mouse_common.h"
#include "mouse_interfaces.h"

#include "checks.h"
#include "control.h"
#include "math_utils.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"
#include "video.h"

#include <cmath>

CHECK_NARROWING();

MouseConfig mouse_config;
MousePredefined mouse_predefined;

constexpr auto capture_type_seamless_str = "seamless";
constexpr auto capture_type_onclick_str  = "onclick";
constexpr auto capture_type_onstart_str  = "onstart";
constexpr auto capture_type_nomouse_str  = "nomouse";

constexpr auto model_ps2_standard_str     = "standard";
constexpr auto model_ps2_intellimouse_str = "intellimouse";
constexpr auto model_ps2_explorer_str     = "explorer";
constexpr auto model_ps2_nomouse_str      = "none";

constexpr auto model_com_2button_str     = "2button";
constexpr auto model_com_3button_str     = "3button";
constexpr auto model_com_wheel_str       = "wheel";
constexpr auto model_com_msm_str         = "msm";
constexpr auto model_com_2button_msm_str = "2button+msm";
constexpr auto model_com_3button_msm_str = "3button+msm";
constexpr auto model_com_wheel_msm_str   = "wheel+msm";

static const std::vector<uint16_t> list_rates = {
        // Commented out values are probably not interesting
        // for the end user as "boosted" sampling rate
        //  10",  // PS/2 mouse
        //  20",  // PS/2 mouse
        //  30",  // bus/InPort mouse
        40, // PS/2 mouse, approx. limit for 1200 baud serial mouse
        //  50,   // bus/InPort mouse
        60,  // PS/2 mouse, used by Microsoft Mouse Driver 8.20
        80,  // PS/2 mouse, approx. limit for 2400 baud serial mouse
        100, // PS/2 mouse, bus/InPort mouse, used by CuteMouse 2.1b4
        125, // USB mouse (basic, non-gaming), Bluetooth mouse
        160, // approx. limit for 4800 baud serial mouse
        200, // PS/2 mouse, bus/InPort mouse
        250, // USB mouse (gaming)
        330, // approx. limit for 9600 baud serial mouse
        500, // USB mouse (gaming)

        // Todays gaming USB mice are capable of even higher sampling
        // rates (like 1000 Hz), but such values are way higher than
        // anything DOS games were designed for; most likely such rates
        // would only result in emulation slowdowns and compatibility
        // issues.
};

const std::vector<uint16_t>& MouseConfig::GetValidMinRateList()
{
	return list_rates;
}

bool MouseConfig::ParseComModel(const std::string_view model_str,
                                MouseModelCOM& model, bool& auto_msm)
{
	if (model_str == model_com_2button_str) {
		model    = MouseModelCOM::Microsoft;
		auto_msm = false;
	} else if (model_str == model_com_3button_str) {
		model    = MouseModelCOM::Logitech;
		auto_msm = false;
	} else if (model_str == model_com_wheel_str) {
		model    = MouseModelCOM::Wheel;
		auto_msm = false;
	} else if (model_str == model_com_msm_str) {
		model    = MouseModelCOM::MouseSystems;
		auto_msm = false;
	} else if (model_str == model_com_2button_msm_str) {
		model    = MouseModelCOM::Microsoft;
		auto_msm = true;
	} else if (model_str == model_com_3button_msm_str) {
		model    = MouseModelCOM::Logitech;
		auto_msm = true;
	} else if (model_str == model_com_wheel_msm_str) {
		model    = MouseModelCOM::Wheel;
		auto_msm = true;
	} else {
		return false;
	}

	return true;
}

static void SetCaptureType(const std::string_view capture_str)
{
	if (capture_str == capture_type_seamless_str) {
		mouse_config.capture = MouseCapture::Seamless;
	} else if (capture_str == capture_type_onclick_str) {
		mouse_config.capture = MouseCapture::OnClick;
	} else if (capture_str == capture_type_onstart_str) {
		mouse_config.capture = MouseCapture::OnStart;
	} else if (capture_str == capture_type_nomouse_str) {
		mouse_config.capture = MouseCapture::NoMouse;
	} else {
		assert(false);
	}
}

static void SetPs2Model(const std::string_view model_str)
{
	if (model_str == model_ps2_standard_str) {
		mouse_config.model_ps2 = MouseModelPS2::Standard;
	} else if (model_str == model_ps2_intellimouse_str) {
		mouse_config.model_ps2 = MouseModelPS2::IntelliMouse;
	} else if (model_str == model_ps2_explorer_str) {
		mouse_config.model_ps2 = MouseModelPS2::Explorer;
	} else if (model_str == model_ps2_nomouse_str) {
		mouse_config.model_ps2 = MouseModelPS2::NoMouse;
	} else {
		assert(false);
	}
}

static void SetComModel(const std::string_view model_str)
{
	[[maybe_unused]] const auto result = MouseConfig::ParseComModel(
	        model_str, mouse_config.model_com, mouse_config.model_com_auto_msm);
	assert(result);
}

static void SetSensitivity(const std::string_view sensitivity_str)
{
	// Coefficient to convert percentage in integer to float
	constexpr float coeff = 0.01f;

	// Default sensitivity value
	const auto& user_default = mouse_predefined.sensitivity_user_default;
	const auto default_value = coeff * user_default;

	auto set_mouse_sensitivity_setting = [](const int value) {
		set_section_property_value("mouse",
		                           "mouse_sensitivity",
		                           format_str("%d", value));
	};
	auto set_mouse_sensitivity_settings = [](const int val_x, const int val_y) {
		set_section_property_value("mouse",
		                           "mouse_sensitivity",
		                           format_str("%d %d", val_x, val_y));
	};

	// Split input string into values
	auto values_str = split(sensitivity_str, " \t,;");
	if (values_str.size() > 2) {
		LOG_WARNING("MOUSE: Too many values in 'mouse_sensitivity', using '%d'",
		            user_default);
		mouse_config.sensitivity_coeff_x = default_value;
		mouse_config.sensitivity_coeff_y = default_value;

		set_mouse_sensitivity_setting(user_default);
		return;
	}

	// If no values given, use defaults
	if (values_str.empty()) {
		mouse_config.sensitivity_coeff_x = default_value;
		mouse_config.sensitivity_coeff_y = default_value;

		set_mouse_sensitivity_setting(user_default);
		return;
	}

	// Convert values to integers
	std::vector<int> values_int = {};

	bool out_of_range   = false;
	const int value_min = -mouse_predefined.sensitivity_user_max;
	const int value_max = mouse_predefined.sensitivity_user_max;

	for (auto& value_str : values_str) {
		// Remove trailing '%' signs, if present
		if (value_str.ends_with('%')) {
			value_str.pop_back();
		}

		const auto value = parse_int(value_str);
		if (!value) {
			LOG_WARNING("MOUSE: Invalid 'mouse_sensitivity' setting: '%s', using '%d'",
			            value_str.c_str(),
			            user_default);
			mouse_config.sensitivity_coeff_x = default_value;
			mouse_config.sensitivity_coeff_y = default_value;

			set_mouse_sensitivity_setting(user_default);
			return;
		}

		const auto value_clamped = clamp(*value, value_min, value_max);
		if (value_clamped != *value) {
			out_of_range = true;
		}
		values_int.push_back(value_clamped);
	}

	if (out_of_range) {
		LOG_WARNING("MOUSE: 'mouse_sensitivity' adjusted to range %+d - %+d",
		            value_min,
		            value_max);
	}

	// Set the actual values
	assert(values_int.size() == 1 || values_int.size() == 2);
	const auto value_float_0         = static_cast<float>(values_int[0]);
	mouse_config.sensitivity_coeff_x = coeff * value_float_0;

	if (values_int.size() == 2) {
		const auto value_float_1 = static_cast<float>(values_int[1]);
		mouse_config.sensitivity_coeff_y = coeff * value_float_1;

		set_mouse_sensitivity_settings(values_int[0], values_int[1]);
	} else {
		mouse_config.sensitivity_coeff_y = mouse_config.sensitivity_coeff_x;

		set_mouse_sensitivity_setting(values_int[0]);
	}

	return;
}

static void config_read(Section* section)
{
	assert(section);
	const Section_prop* conf = dynamic_cast<Section_prop*>(section);
	assert(conf);
	if (!conf) {
		return;
	}

	// Settings changeable during runtime

	SetCaptureType(conf->Get_string("mouse_capture"));
	SetSensitivity(conf->Get_string("mouse_sensitivity"));

	mouse_config.multi_display_aware = conf->Get_bool("mouse_multi_display_aware");

	mouse_config.middle_release = conf->Get_bool("mouse_middle_release");
	mouse_config.raw_input      = conf->Get_bool("mouse_raw_input");
	mouse_config.dos_immediate  = conf->Get_bool("dos_mouse_immediate");

	// Settings below should be read only once

	if (mouse_shared.ready_config) {

		if (mouse_config.capture == MouseCapture::NoMouse) {
			// If NoMouse got configured at runtime,
			// immediately clear all the mapping
			MouseControlAPI mouse_config_api;
			mouse_config_api.UnMap(MouseControlAPI::ListIDs());
		}

		MOUSE_UpdateGFX();
		return;
	}

	// DOS driver configuration

	mouse_config.dos_driver = conf->Get_bool("dos_mouse_driver");

	// PS/2 AUX port mouse configuration

	SetPs2Model(conf->Get_string("ps2_mouse_model"));

	// COM port mouse configuration

	SetComModel(conf->Get_string("com_mouse_model"));

	// VMM PCI interfaces

	mouse_config.is_vmware_mouse_enabled = conf->Get_bool("vmware_mouse");
	mouse_config.is_virtualbox_mouse_enabled = conf->Get_bool("virtualbox_mouse");

	if (!GFX_HaveDesktopEnvironment() && mouse_config.is_virtualbox_mouse_enabled) {
		// VirtualBox guest side driver is able to request us to re-use
		// host side cursor (at least the 3rd party DOS driver does so)
		// and we have no way to refuse, there seems to be no easy way
		// to handle the situation gracefully in a no-desktop
		// environment unless we want to display our own mouse cursor.
		// Therefore, it is best to block the VirtualBox mouse API - it
		// wasn't designed for such a use case.
		LOG_WARNING("MOUSE: VirtualBox interface cannot work in a no-desktop environment");
		mouse_config.is_virtualbox_mouse_enabled = false;
	}

	// Start mouse emulation if everything is ready
	mouse_shared.ready_config = true;
	MOUSE_StartupIfReady();
}

static void config_init(Section_prop& secprop)
{
	constexpr auto always        = Property::Changeable::Always;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	Prop_bool* prop_bool  = nullptr;
	Prop_string* prop_str = nullptr;

	// General configuration

	prop_str = secprop.Add_string("mouse_capture", always, capture_type_onclick_str);
	assert(prop_str);
	prop_str->Set_values({capture_type_seamless_str,
	                      capture_type_onclick_str,
	                      capture_type_onstart_str,
	                      capture_type_nomouse_str});
	prop_str->Set_help(
	        "Set the mouse capture behaviour:\n"
	        "  onclick:   Capture the mouse when clicking any mouse button in the window\n"
	        "             (default).\n"
	        "  onstart:   Capture the mouse immediately on start. Might not work correctly\n"
	        "             on some host operating systems.\n"
	        "  seamless:  Let the mouse move seamlessly between the DOSBox window and the\n"
	        "             rest of the desktop; captures only with middle-click or hotkey.\n"
	        "             Seamless mouse does not work correctly with all the games.\n"
	        "             Windows 3.1x can be made compatible with a custom mouse driver.\n"
	        "  nomouse:   Hide the mouse and don't send mouse input to the game.\n"
	        "For touch-screen control, use 'seamless'.");

	prop_bool = secprop.Add_bool("mouse_middle_release", always, true);
	prop_bool->Set_help(
	        "Release the captured mouse by middle-clicking, and also capture it in\n"
	        "seamless mode (enabled by default).");

	prop_bool = secprop.Add_bool("mouse_multi_display_aware", always, true);
	prop_bool->Set_help(
	        "Allow seamless mouse behavior and mouse pointer release to work in fullscreen\n"
	        "mode on systems with more than one display (enabled by default).\n"
	        "Note: You should disable this if it incorrectly detects multiple displays\n"
	        "      when only one should actually be used. This might happen if you are\n"
	        "      using mirrored display mode or using an AV receiver's HDMI input for\n"
	        "      audio-only listening.");

	prop_str = secprop.Add_string("mouse_sensitivity", always, "100");
	prop_str->Set_help(
	        "Global mouse sensitivity for the horizontal and vertical axes, as a percentage\n"
	        "(100 by default). Values can be separated by spaces, commas, or semicolons\n"
	        "(i.e. 100,150). Negative values invert the axis, zero disables it.\n"
	        "Providing only one value sets sensitivity for both axes.\n"
	        "Sensitivity can be further fine-tuned per mouse interface using the internal\n"
	        "MOUSECTL.COM tool available on the Z drive.");

	prop_bool = secprop.Add_bool("mouse_raw_input", always, true);
	prop_bool->Set_help(
	        "Enable to bypass your operating system's mouse acceleration and sensitivity\n"
	        "settings (enabled by default). Works in fullscreen or when the mouse is\n"
	        "captured in windowed mode.");

	// DOS driver configuration

	prop_bool = secprop.Add_bool("dos_mouse_driver", only_at_start, true);
	assert(prop_bool);
	prop_bool->Set_help(
	        "Enable the built-in mouse driver (enabled by default). This results in the\n"
	        "lowest possible latency and the smoothest mouse movement, so only disable it\n"
	        "and load a real DOS mouse driver if it's really necessary (e.g., if a game is\n"
	        "not compatible with the built-in driver).\n"
	        "  on:   Enable the built-in mouse driver. `ps2_mouse_model` and\n"
	        "        `com_mouse_model` have no effect on the built-in driver.\n"
	        "  off:  Disable the built-in mouse driver (if you don't want mouse support or\n"
	        "        you want to load a real DOS mouse driver). To use a real DOS driver\n"
	        "        (e.g., MOUSE.COM or CTMOUSE.EXE), configure the mouse type with\n"
	        "        `ps2_mouse_model` or `com_mouse_model`, then load the driver.\n"
	        "        A real DOS driver might increase compatibility with some programs,\n"
	        "        but will introduce more input latency.\n"
	        "Note: The built-in driver is auto-disabled if you boot into real MS-DOS or\n"
	        "      Windows 9x under DOSBox. Under Windows 3.x, the driver is not disabled,\n"
	        "      but the Windows 3.x mouse driver takes over.");

	prop_bool = secprop.Add_bool("dos_mouse_immediate", always, false);
	assert(prop_bool);
	prop_bool->Set_help(
	        "Update mouse movement counters immediately, without waiting for interrupt\n"
	        "(disabled by default). May improve gameplay, especially in fast-paced games\n"
	        "(arcade, FPS, etc.), as for some games it effectively boosts the mouse\n"
	        "sampling rate to 1000 Hz, without increasing interrupt overhead.\n"
	        "Might cause compatibility issues. List of known incompatible games:\n"
	        "  - Ultima Underworld: The Stygian Abyss\n"
	        "  - Ultima Underworld II: Labyrinth of Worlds\n"
	        "Please report it if you find another incompatible game so we can update this\n"
	        "list.");

	// Physical mice configuration

	// TODO: PS/2 mouse might be hot-pluggable
	prop_str = secprop.Add_string("ps2_mouse_model",
	                              only_at_start,
	                              model_ps2_explorer_str);
	assert(prop_str);
	prop_str->Set_values({model_ps2_standard_str,
	                      model_ps2_intellimouse_str,
	                      model_ps2_explorer_str,
	                      model_ps2_nomouse_str});
	prop_str->Set_help(
	        "Set the PS/2 AUX port mouse model, or in other words, the type of the virtual\n"
	        "mouse plugged into the emulated PS/2 mouse port ('explorer' by default).\n"
	        "The setting has no effect on the built-in mouse driver (see 'dos_mouse_driver').\n"
	        "  standard:      3 buttons, standard PS/2 mouse.\n"
	        "  intellimouse:  3 buttons + wheel, Microsoft IntelliMouse.\n"
	        "  explorer:      5 buttons + wheel, Microsoft IntelliMouse Explorer (default).\n"
	        "  none:          no PS/2 mouse.");

	prop_str = secprop.Add_string("com_mouse_model",
	                              only_at_start,
	                              model_com_wheel_msm_str);
	assert(prop_str);
	prop_str->Set_values({model_com_2button_str,
	                      model_com_3button_str,
	                      model_com_wheel_str,
	                      model_com_msm_str,
	                      model_com_2button_msm_str,
	                      model_com_3button_msm_str,
	                      model_com_wheel_msm_str});
	prop_str->Set_help(
	        "Set the default COM (serial) mouse model, or in other words, the type of the\n"
	        "virtual mouse plugged into the emulated COM ports ('wheel+msm' by default).\n"
	        "The setting has no effect on the built-in mouse driver (see 'dos_mouse_driver').\n"
	        "  2button:      2 buttons, Microsoft mouse.\n"
	        "  3button:      3 buttons, Logitech mouse;\n"
	        "                mostly compatible with Microsoft mouse.\n"
	        "  wheel:        3 buttons + wheel;\n"
	        "                mostly compatible with Microsoft mouse.\n"
	        "  msm:          3 buttons, Mouse Systems mouse;\n"
	        "                NOT compatible with Microsoft mouse.\n"
	        "  2button+msm:  Automatic choice between '2button' and 'msm'.\n"
	        "  3button+msm:  Automatic choice between '3button' and 'msm'.\n"
	        "  wheel+msm:    Automatic choice between 'wheel' and 'msm' (default).\n"
	        "Note: Enable COM port mice in the [serial] section.");

	// VMM interfaces

	prop_bool = secprop.Add_bool("vmware_mouse", only_at_start, true);
	prop_bool->Set_help(
	        "VMware mouse interface (enabled by default).\n"
	        "Fully compatible only with experimental 3rd party Windows 3.1x driver.\n"
	        "Note: Requires PS/2 mouse to be enabled.");

	prop_bool = secprop.Add_bool("virtualbox_mouse", only_at_start, true);
	prop_bool->Set_help(
	        "VirtualBox mouse interface (enabled by default).\n"
	        "Fully compatible only with 3rd party Windows 3.1x driver.\n"
	        "Note: Requires PS/2 mouse to be enabled.");
}

void MOUSE_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	Section_prop* sec = conf->AddSection_prop("mouse",
	                                          &config_read,
	                                          changeable_at_runtime);
	assert(sec);
	config_init(*sec);
}
