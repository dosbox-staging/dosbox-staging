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

#include "mouse_config.h"
#include "mouse_common.h"
#include "mouse_interfaces.h"

#include "checks.h"
#include "control.h"
#include "math_utils.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#include <cmath>

CHECK_NARROWING();

MouseConfig     mouse_config;
MousePredefined mouse_predefined;

constexpr auto capture_type_seamless_str  = "seamless";
constexpr auto capture_type_onclick_str   = "onclick";
constexpr auto capture_type_onstart_str   = "onstart";
constexpr auto capture_type_nomouse_str   = "nomouse";

constexpr auto model_ps2_standard_str     = "standard";
constexpr auto model_ps2_intellimouse_str = "intellimouse";
constexpr auto model_ps2_explorer_str     = "explorer";
constexpr auto model_ps2_nomouse_str      = "none";

constexpr auto model_com_2button_str      = "2button";
constexpr auto model_com_3button_str      = "3button";
constexpr auto model_com_wheel_str        = "wheel";
constexpr auto model_com_msm_str          = "msm";
constexpr auto model_com_2button_msm_str  = "2button+msm";
constexpr auto model_com_3button_msm_str  = "3button+msm";
constexpr auto model_com_wheel_msm_str    = "wheel+msm";

static const char *list_capture_types[] = {
	capture_type_seamless_str,
	capture_type_onclick_str,
	capture_type_onstart_str,
	capture_type_nomouse_str,
	nullptr
};

static const char* list_models_ps2[] = {
	model_ps2_standard_str,
        model_ps2_intellimouse_str,
        model_ps2_explorer_str,
        model_ps2_nomouse_str,
        nullptr
};

static const char *list_models_com[] = {
	model_com_2button_str,
	model_com_3button_str,
	model_com_wheel_str,
	model_com_msm_str,
	model_com_2button_msm_str,
	model_com_3button_msm_str,
	model_com_wheel_msm_str,
	nullptr
};

static const std::vector<uint16_t> list_rates = {
        // Commented out values are probably not interesting
        // for the end user as "boosted" sampling rate
        //  10",  // PS/2 mouse
        //  20",  // PS/2 mouse
        //  30",  // bus/InPort mouse
        40,  // PS/2 mouse, approx. limit for 1200 baud serial mouse
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

bool MouseConfig::ParseCaptureType(const std::string_view capture_str,
                                   MouseCapture& capture)
{
	if (capture_str == capture_type_seamless_str)
		capture = MouseCapture::Seamless;
	else if (capture_str == capture_type_onclick_str)
		capture = MouseCapture::OnClick;
	else if (capture_str == capture_type_onstart_str)
		capture = MouseCapture::OnStart;
	else if (capture_str == capture_type_nomouse_str)
		capture = MouseCapture::NoMouse;
	else
		return false;
	return true;
}

bool MouseConfig::ParseCOMModel(const std::string_view model_str,
                                MouseModelCOM& model, bool& auto_msm)
{
	if (model_str == model_com_2button_str) {
		model    = MouseModelCOM::Microsoft;
		auto_msm = false;
		return true;
	} else if (model_str == model_com_3button_str) {
		model    = MouseModelCOM::Logitech;
		auto_msm = false;
		return true;
	} else if (model_str == model_com_wheel_str) {
		model    = MouseModelCOM::Wheel;
		auto_msm = false;
		return true;
	} else if (model_str == model_com_msm_str) {
		model    = MouseModelCOM::MouseSystems;
		auto_msm = false;
		return true;
	} else if (model_str == model_com_2button_msm_str) {
		model    = MouseModelCOM::Microsoft;
		auto_msm = true;
		return true;
	} else if (model_str == model_com_3button_msm_str) {
		model    = MouseModelCOM::Logitech;
		auto_msm = true;
		return true;
	} else if (model_str == model_com_wheel_msm_str) {
		model    = MouseModelCOM::Wheel;
		auto_msm = true;
		return true;
	}

	return false;
}

bool MouseConfig::ParsePS2Model(const std::string_view model_str, MouseModelPS2& model)
{
	if (model_str == model_ps2_standard_str) {
		model = MouseModelPS2::Standard;
	} else if (model_str == model_ps2_intellimouse_str) {
		model = MouseModelPS2::IntelliMouse;
	} else if (model_str == model_ps2_explorer_str) {
		model = MouseModelPS2::Explorer;
	} else if (auto as_bool = parse_bool_setting(model_str);
	           as_bool && *as_bool == false) {
		model = MouseModelPS2::NoMouse;
	} else {
		return false;
	}
	return true;
}

const std::vector<uint16_t> &MouseConfig::GetValidMinRateList()
{
	return list_rates;
}

static void config_read(Section *section)
{
	assert(section);
	const Section_prop *conf = dynamic_cast<Section_prop *>(section);
	assert(conf);
	if (!conf)
		return;

	// Settings changeable during runtime

	std::string prop_str = conf->Get_string("mouse_capture");
	MouseConfig::ParseCaptureType(prop_str, mouse_config.capture);

	mouse_config.multi_display_aware =
		conf->Get_bool("mouse_multi_display_aware");

	mouse_config.middle_release = conf->Get_bool("mouse_middle_release");
	mouse_config.raw_input      = conf->Get_bool("mouse_raw_input");
	mouse_config.dos_immediate  = conf->Get_bool("dos_mouse_immediate");

	// Settings below should be read only once

	if (mouse_shared.ready_config) {

		if (mouse_config.capture == MouseCapture::NoMouse) {
			// If NoMouse got configured in runtime,
			// immediately clear all the mapping
			MouseControlAPI mouse_config_api;
			mouse_config_api.UnMap(MouseControlAPI::ListIDs());
		}

		MOUSE_UpdateGFX();
		return;
	}

	// Default mouse sensitivity

	PropMultiVal *prop_multi = conf->GetMultiVal("mouse_sensitivity");
	mouse_config.sensitivity_x = static_cast<int16_t>(
	        prop_multi->GetSection()->Get_int("xsens"));
	mouse_config.sensitivity_y = static_cast<int16_t>(
	        prop_multi->GetSection()->Get_int("ysens"));

	// DOS driver configuration

	mouse_config.dos_driver = conf->Get_bool("dos_mouse_driver");

	// PS/2 AUX port mouse configuration

	prop_str = conf->Get_string("ps2_mouse_model");
	MouseConfig::ParsePS2Model(prop_str, mouse_config.model_ps2);

	// COM port mouse configuration

	prop_str = conf->Get_string("com_mouse_model");
	MouseConfig::ParseCOMModel(prop_str,
	                           mouse_config.model_com,
	                           mouse_config.model_com_auto_msm);

	// VMM PCI interfaces

#ifdef EXPERIMENTAL_VIRTUALBOX_MOUSE

	mouse_config.is_vmware_mouse_enabled = conf->Get_bool("vmware_mouse");
	mouse_config.is_virtualbox_mouse_enabled = conf->Get_bool("virtualbox_mouse");

#endif

	// Start mouse emulation if everything is ready
	mouse_shared.ready_config = true;
	MOUSE_StartupIfReady();
}

static void config_init(Section_prop &secprop)
{
	constexpr auto always        = Property::Changeable::Always;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	Prop_bool    *prop_bool  = nullptr;
	Prop_int     *prop_int   = nullptr;
	Prop_string  *prop_str   = nullptr;
	PropMultiVal *prop_multi = nullptr;

	// General configuration

	prop_str = secprop.Add_string("mouse_capture", always,
	                              capture_type_onclick_str);
	assert(prop_str);
	prop_str->Set_values(list_capture_types);
	prop_str->Set_help(
	        "Set the mouse capture behaviour:\n"
	        "  onclick:   Capture the mouse when clicking any mouse button in the window \n"
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
	prop_bool->Set_help("If enabled, middle-click will release the captured mouse, and also capture\n"
	                    "when seamless (enabled by default).");

	prop_bool = secprop.Add_bool("mouse_multi_display_aware", always, true);
	prop_bool->Set_help("Allows mouse seamless behavior and mouse pointer release to work in fullscreen\n"
	                    "mode for systems with more than one display. (enabled by default).\n"
	                    "Note: You should disable this if it incorrectly detects multiple displays\n"
	                    "      when only one should actually be used. This might happen if you are\n"
	                    "      using mirrored display mode or using an AV receiver's HDMI input for\n"
	                    "      audio-only listening.");

	prop_multi = secprop.AddMultiVal("mouse_sensitivity", only_at_start, ",");
	prop_multi->Set_help(
	        "Mouse sensitivity for the horizontal and vertical axes, as a percentage\n"
	        "(100,100 by default). Negative values invert the axis, zero disables it.\n"
	        "Providing only one value sets sensitivity for both axes.\n"
	        "The setting can be adjusted in runtime (also per mouse interface) using\n"
	        "internal MOUSECTL.COM tool, available on drive Z.");
	prop_multi->SetValue("100");

	prop_int = prop_multi->GetSection()->Add_int("xsens", only_at_start, 100);
	prop_int->SetMinMax(-mouse_predefined.sensitivity_user_max,
	                    mouse_predefined.sensitivity_user_max);
	prop_int = prop_multi->GetSection()->Add_int("ysens", only_at_start, 100);
	prop_int->SetMinMax(-mouse_predefined.sensitivity_user_max,
	                    mouse_predefined.sensitivity_user_max);

	prop_bool = secprop.Add_bool("mouse_raw_input", always, true);
	prop_bool->Set_help(
	        "Enable to bypass your operating system's mouse acceleration and sensitivity\n"
	        "settings (enabled by default). Works in fullscreen or when the mouse is\n"
	        "captured in window mode.");

	// DOS driver configuration

	prop_bool = secprop.Add_bool("dos_mouse_driver", only_at_start, true);
	assert(prop_bool);
	prop_bool->Set_help(
	        "Enable built-in DOS mouse driver (enabled by default).\n"
	        "Notes:\n"
	        "  - Disable if you intend to use original MOUSE.COM driver in emulated DOS.\n"
	        "  - When guest OS is booted, built-in driver gets disabled automatically.");

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
	prop_str->Set_values(list_models_ps2);
	prop_str->Set_help(
	        "PS/2 AUX port mouse model:\n"
	        "  standard:      3 buttons, standard PS/2 mouse.\n"
	        "  intellimouse:  3 buttons + wheel, Microsoft IntelliMouse.\n"
	        "  explorer:      5 buttons + wheel, Microsoft IntelliMouse Explorer.\n"
	        "  none:          no PS/2 mouse emulated.\n"
	        "Default: explorer");

	prop_str = secprop.Add_string("com_mouse_model",
	                              only_at_start,
	                              model_com_wheel_msm_str);
	assert(prop_str);
	prop_str->Set_values(list_models_com);
	prop_str->Set_help(
	        "COM (serial) port default mouse model:\n"
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

#ifdef EXPERIMENTAL_VIRTUALBOX_MOUSE

	prop_bool = secprop.Add_bool("vmware_mouse", only_at_start, true);
	prop_bool->Set_help("VMware mouse interface (enabled by default).\n"
	                    "Note: This requires PS/2 mouse to be enabled.\n");
	prop_bool = secprop.Add_bool("virtualbox_mouse", only_at_start, true);
	prop_bool->Set_help("VirtualBox mouse interface (enabled by default).\n"
	                    "Note: This requires PS/2 mouse to be enabled.\n");

#endif
}

void MOUSE_AddConfigSection(Config* conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	Section_prop* sec = conf->AddSection_prop("mouse",
	                                          &config_read,
	                                          changeable_at_runtime);
	assert(sec);
	config_init(*sec);
}
