// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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

constexpr auto CaptureTypeSeamless = "seamless";
constexpr auto CaptureTypeOnClick  = "onclick";
constexpr auto CaptureTypeOnStart  = "onstart";
constexpr auto CaptureTypeNoMouse  = "nomouse";

constexpr auto ModelDos2Button = "2button";
constexpr auto ModelDos3Button = "3button";
constexpr auto ModelDosWheel   = "wheel";

constexpr auto ModelPs2Standard     = "standard";
constexpr auto ModelPs2Intellimouse = "intellimouse";
constexpr auto ModelPs2Explorer     = "explorer";
constexpr auto ModelPs2NoMouse      = "none";

constexpr auto ModelCom2Button    = "2button";
constexpr auto ModelCom3Button    = "3button";
constexpr auto ModelComWheel      = "wheel";
constexpr auto ModelComMsm        = "msm";
constexpr auto ModelCom2ButtonMsm = "2button+msm";
constexpr auto ModelCom3ButtonMsm = "3button+msm";
constexpr auto ModelComWheelMsm   = "wheel+msm";

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

constexpr auto DefaultBuiltinDosMouseDriverOptions = "";

const std::vector<uint16_t>& MouseConfig::GetValidMinRateList()
{
	return list_rates;
}

bool MouseConfig::ParseComModel(const std::string_view model_str,
                                MouseModelCOM& model, bool& auto_msm)
{
	using enum MouseModelCOM;

	if (iequals(model_str, ModelCom2Button)) {
		model    = Microsoft;
		auto_msm = false;
	} else if (iequals(model_str, ModelCom3Button)) {
		model    = Logitech;
		auto_msm = false;
	} else if (iequals(model_str, ModelComWheel)) {
		model    = Wheel;
		auto_msm = false;
	} else if (iequals(model_str, ModelComMsm)) {
		model    = MouseSystems;
		auto_msm = false;
	} else if (iequals(model_str, ModelCom2ButtonMsm)) {
		model    = Microsoft;
		auto_msm = true;
	} else if (iequals(model_str, ModelCom3ButtonMsm)) {
		model    = Logitech;
		auto_msm = true;
	} else if (iequals(model_str, ModelComWheelMsm)) {
		model    = Wheel;
		auto_msm = true;
	} else {
		return false;
	}

	return true;
}

static void set_capture_type(const std::string_view capture_str)
{
	using enum MouseCapture;

	if (iequals(capture_str, CaptureTypeSeamless)) {
		mouse_config.capture = Seamless;
	} else if (iequals(capture_str, CaptureTypeOnClick)) {
		mouse_config.capture = OnClick;
	} else if (iequals(capture_str, CaptureTypeOnStart)) {
		mouse_config.capture = OnStart;
	} else if (iequals(capture_str, CaptureTypeNoMouse)) {
		mouse_config.capture = NoMouse;
	} else {
		assertm(false, "Invalid mouse capture value");
	}
}

static void set_dos_driver_model(const std::string_view model_str)
{
	using enum MouseModelDos;

	auto new_model = mouse_config.model_dos;

	if (iequals(model_str, ModelDos2Button)) {
		new_model = TwoButton;
	} else if (iequals(model_str, ModelDos3Button)) {
		new_model = ThreeButton;
	} else if (iequals(model_str, ModelDosWheel)) {
		new_model = Wheel;
	} else {
		assertm(false, "Invalid DOS driver mouse model value");
	}

	if (new_model != mouse_config.model_dos) {
		mouse_config.model_dos = new_model;
		MOUSEDOS_NotifyModelChanged();
	}
}

static void set_ps2_mouse_model(const std::string_view model_str)
{
	using enum MouseModelPS2;

	if (iequals(model_str, ModelPs2Standard)) {
		mouse_config.model_ps2 = Standard;
	} else if (iequals(model_str, ModelPs2Intellimouse)) {
		mouse_config.model_ps2 = IntelliMouse;
	} else if (iequals(model_str, ModelPs2Explorer)) {
		mouse_config.model_ps2 = Explorer;
	} else if (iequals(model_str, ModelPs2NoMouse)) {
		mouse_config.model_ps2 = NoMouse;
	} else {
		assertm(false, "Invalid PS/2 mouse model value");
	}
}

static void set_serial_mouse_model(const std::string_view model_str)
{
	[[maybe_unused]] const auto result = MouseConfig::ParseComModel(
	        model_str, mouse_config.model_com, mouse_config.model_com_auto_msm);
	assert(result);
}

static void set_dos_driver_options(const std::string_view options_str)
{
	const std::string OptionImmediate = "immediate";
	const std::string OptionModern    = "modern";

	auto& last_options_str = mouse_config.dos_driver_last_options_str;
	if (last_options_str == options_str) {
		return;
	}
	last_options_str = options_str;

	mouse_config.dos_driver_immediate = false;
	mouse_config.dos_driver_modern    = false;

	for (const auto& token : split(options_str, " ,")) {
		if (token == OptionImmediate) {
			mouse_config.dos_driver_immediate = true;
		} else if (token == OptionModern) {
			mouse_config.dos_driver_modern = true;
		} else {
			LOG_WARNING(
			        "MOUSE: Invalid 'builtin_dos_mouse_driver_options' "
			        "parameter: '%s', using defaults",
			        token.c_str());

			set_section_property_value("mouse",
			                           "builtin_dos_mouse_driver_options",
			                           DefaultBuiltinDosMouseDriverOptions);

			set_dos_driver_options(DefaultBuiltinDosMouseDriverOptions);
			return;
		}
	}

	// Log new config options
	static std::string last_logged_str = "";
	if (options_str == last_logged_str) {
		return;
	}

	std::string log_options = {};
	if (mouse_config.dos_driver_immediate) {
		log_options = OptionImmediate;
	}
	if (mouse_config.dos_driver_modern) {
		if (!log_options.empty()) {
			log_options += ", ";
		}
		log_options += OptionModern;
	}

	if (log_options.empty()) {
		log_options = "none";
	}

	LOG_INFO("MOUSE (DOS): Driver options: %s", log_options.c_str());
	last_logged_str = options_str;
}

static void set_sensitivity(const std::string_view sensitivity_str)
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

	set_capture_type(conf->Get_string("mouse_capture"));
	set_sensitivity(conf->Get_string("mouse_sensitivity"));

	mouse_config.multi_display_aware = conf->Get_bool("mouse_multi_display_aware");

	mouse_config.middle_release = conf->Get_bool("mouse_middle_release");
	mouse_config.raw_input      = conf->Get_bool("mouse_raw_input");

	set_dos_driver_model(conf->Get_string("builtin_dos_mouse_driver_model"));
	set_dos_driver_options(conf->Get_string("builtin_dos_mouse_driver_options"));

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

	// Built-in DOS driver configuration

	mouse_config.dos_driver_enabled = conf->Get_bool("builtin_dos_mouse_driver");

	// PS/2 AUX port mouse configuration

	set_ps2_mouse_model(conf->Get_string("ps2_mouse_model"));

	// COM port mouse configuration

	set_serial_mouse_model(conf->Get_string("com_mouse_model"));

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
	constexpr auto deprecated    = Property::Changeable::Deprecated;

	Prop_bool* prop_bool  = nullptr;
	Prop_string* prop_str = nullptr;

	// General configuration

	prop_str = secprop.Add_string("mouse_capture", always, CaptureTypeOnClick);
	assert(prop_str);
	prop_str->Set_values({CaptureTypeSeamless,
	                      CaptureTypeOnClick,
	                      CaptureTypeOnStart,
	                      CaptureTypeNoMouse});
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
	        "seamless mode ('on' by default).");

	prop_bool = secprop.Add_bool("mouse_multi_display_aware", always, true);
	prop_bool->Set_help(
	        "Allow seamless mouse behavior and mouse pointer release to work in fullscreen\n"
	        "mode on systems with more than one display ('on' by default).\n"
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
	        "settings ('on' by default). Works in fullscreen or when the mouse is captured\n"
	        "in windowed mode.");

	// Built-in DOS driver configuration

	prop_bool = secprop.Add_bool("builtin_dos_mouse_driver", only_at_start, true);
	assert(prop_bool);
	prop_bool->Set_help(
	        "Enable the built-in DOS mouse driver ('on' by default). This results in the\n"
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

	prop_bool = secprop.Add_bool("dos_mouse_driver", deprecated, true);
	prop_bool->Set_help("Renamed to 'builtin_dos_mouse_driver'.");

	prop_str = secprop.Add_string("builtin_dos_mouse_driver_model",
	                              always,
	                              ModelDos2Button);
	assert(prop_str);
	prop_str->Set_values(
	        {ModelDos2Button, ModelDos3Button, ModelDosWheel});
	prop_str->Set_help(
	        "Set the mouse model to be simulated by the built-in DOS mouse driver ('2button'\n"
	        "by default).\n"
	        "  2button:  2 buttons, the safest option for most games. The majority of DOS\n"
	        "            games onoly support 2 buttons, some might misbehave if the middle\n"
	        "            button is pressed.\n"
	        "  3button:  3 buttons, only supported by very few DOS games. Only use this if\n"
	        "            the game is known to support a 3-button mouse.\n"
	        "  wheel:    3 buttons + wheel, supports the CuteMouse WheelAPI version 1.0.\n"
	        "            No DOS game uses the mouse wheel, only a handful of DOS applications\n"
	        "            and Windows 3.x with special third-party drivers.");

	prop_str = secprop.Add_string("builtin_dos_mouse_driver_options",
	                              always,
	                              DefaultBuiltinDosMouseDriverOptions);
	assert(prop_str);
	prop_str->Set_help(
	        "Additional built-in DOS mouse driver settings as a list of space or comma\n"
	        "separated options (unset by default).\n"
	        "  immediate:  Update mouse movement counters immediately, without waiting for\n"
	        "              interrupt. May improve mouse latency in fast-paced games (arcade,\n"
	        "              FPS, etc.), but might cause issues in some titles.\n"
	        "              List of known incompatible games:\n"
	        "                - Ultima Underworld: The Stygian Abyss\n"
	        "                - Ultima Underworld II: Labyrinth of Worlds\n"
	        "              Please report other incompatible games so we can update this list.\n"
	        "  modern:     If provided, v7.0+ Microsoft mouse driver behaviour is emulated,\n"
	        "              otherwise the v6.0 and earlier behaviour (the two are slightly\n"
	        "              incompatible). Only Descent II with the official Voodoo patch has\n"
	        "              been found to require the v7.0+ behaviour so far.\n");

	prop_bool = secprop.Add_bool("dos_mouse_immediate", deprecated, false);
	prop_bool->Set_help("Configure using 'builtin_dos_mouse_driver_options'.");

	// Physical mice configuration

	// TODO: PS/2 mouse might be hot-pluggable
	prop_str = secprop.Add_string("ps2_mouse_model",
	                              only_at_start,
	                              ModelPs2Explorer);
	assert(prop_str);
	prop_str->Set_values({ModelPs2Standard,
	                      ModelPs2Intellimouse,
	                      ModelPs2Explorer,
	                      ModelPs2NoMouse});
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
	                              ModelComWheelMsm);
	assert(prop_str);
	prop_str->Set_values({ModelCom2Button,
	                      ModelCom3Button,
	                      ModelComWheel,
	                      ModelComMsm,
	                      ModelCom2ButtonMsm,
	                      ModelCom3ButtonMsm,
	                      ModelComWheelMsm});
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
	        "VMware mouse interface ('on' by default).\n"
	        "with experimental 3rd party Windows 3.1x driver.\n"
	        "Note: Requires PS/2 mouse to be enabled.");

	prop_bool = secprop.Add_bool("virtualbox_mouse", only_at_start, true);
	prop_bool->Set_help(
	        "VirtualBox mouse interface ('on' by default).\n"
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
