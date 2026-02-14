// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/mouse_config.h"
#include "private/mouse_common.h"
#include "private/mouse_interfaces.h"

#include "config/config.h"
#include "config/setup.h"
#include "gui/common.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

#include <cmath>

CHECK_NARROWING();

static const std::string SectionName = "mouse";

MouseConfig mouse_config;

static bool is_serial_mouse_model_read = false;

namespace OptionBuiltInDosDriver {

constexpr auto NoTsr = "no-tsr";

} // namespace OptionBuiltInDosDriver

namespace OptionCaptureType {

constexpr auto Seamless = "seamless";
constexpr auto OnClick  = "onclick";
constexpr auto OnStart  = "onstart";
constexpr auto NoMouse  = "nomouse";

} // namespace OptionCaptureType

namespace OptionModelDos {

constexpr auto TwoButton   = "2button";
constexpr auto ThreeButton = "3button";
constexpr auto Wheel       = "wheel";

} // namespace OptionModelDos

namespace OptionModelPs2 {

constexpr auto Standard     = "standard";
constexpr auto Intellimouse = "intellimouse";
constexpr auto Explorer     = "explorer";
constexpr auto NoMouse      = "none";

} // namespace OptionModelPs2

namespace OptionModelCom {

constexpr auto TwoButton      = "2button";
constexpr auto ThreeButton    = "3button";
constexpr auto Wheel          = "wheel";
constexpr auto Msm            = "msm";
constexpr auto TwoButtonMsm   = "2button+msm";
constexpr auto ThreeButtonMsm = "3button+msm";
constexpr auto WheelMsm       = "wheel+msm";

} // namespace OptionModelCom

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

bool MOUSECOM_ParseComModel(const std::string& model_str, MouseModelCom& model,
                            bool& auto_msm)
{
	using enum MouseModelCom;

	std::string model_lowcase = model_str;
	lowcase(model_lowcase);

	if (model_lowcase == OptionModelCom::TwoButton) {
		model    = Microsoft;
		auto_msm = false;
	} else if (model_lowcase == OptionModelCom::ThreeButton) {
		model    = Logitech;
		auto_msm = false;
	} else if (model_lowcase == OptionModelCom::Wheel) {
		model    = Wheel;
		auto_msm = false;
	} else if (model_lowcase == OptionModelCom::Msm) {
		model    = MouseSystems;
		auto_msm = false;
	} else if (model_lowcase == OptionModelCom::TwoButtonMsm) {
		model    = Microsoft;
		auto_msm = true;
	} else if (model_lowcase == OptionModelCom::ThreeButtonMsm) {
		model    = Logitech;
		auto_msm = true;
	} else if (model_lowcase == OptionModelCom::WheelMsm) {
		model    = Wheel;
		auto_msm = true;
	} else {
		return false;
	}

	return true;
}

static void log_invalid_parameter(const std::string& setting_name,
                                  const std::string_view option_str,
                                  const std::string& adapted_value)
{
	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "MOUSE",
	                      "PROGRAM_CONFIG_INVALID_SETTING",
	                      setting_name.c_str(),
	                      std::string(option_str).c_str(),
	                      adapted_value.c_str());
}

static void set_capture_type(const SectionProp& section)
{
	const std::string SettingName = "mouse_capture";

	const auto option_str = section.GetStringLowCase(SettingName);

	using enum MouseCapture;

	if (option_str == OptionCaptureType::Seamless) {
		mouse_config.capture = Seamless;
	} else if (option_str == OptionCaptureType::OnClick) {
		mouse_config.capture = OnClick;
	} else if (option_str == OptionCaptureType::OnStart) {
		mouse_config.capture = OnStart;
	} else if (option_str == OptionCaptureType::NoMouse) {
		mouse_config.capture = NoMouse;
	} else {
		assertm(false, "Invalid mouse capture value");
	}
}

static void set_dos_driver(const SectionProp& section)
{
	const std::string SettingName = "builtin_dos_mouse_driver";

	const auto option_str = section.GetStringLowCase(SettingName);

	if (has_false(option_str)) {
		mouse_config.dos_driver_autoexec = false;
		mouse_config.dos_driver_no_tsr   = false;

	} else if (has_true(option_str)) {
		if (is_machine_tandy() || is_machine_pcjr()) {
			// The mouse TSR simulation currently does not work
			// correctly with PCJr or Tandy memory layout - MCB
			// corruption occurs (TODO: to be fixed).
			LOG_INFO("MOUSE (DOS): Forced no-TSR mode due to Tandy/PCJr machine type");
			mouse_config.dos_driver_autoexec = false;
			mouse_config.dos_driver_no_tsr   = true;
		} else {
			mouse_config.dos_driver_autoexec = true;
			mouse_config.dos_driver_no_tsr   = false;
		}

	} else if (option_str == OptionBuiltInDosDriver::NoTsr) {
		mouse_config.dos_driver_autoexec = false;
		mouse_config.dos_driver_no_tsr   = true;

	} else {
		assertm(false, "Invalid mouse driver mode");
	}
}

static void set_dos_driver_model(const SectionProp& section)
{
	const std::string SettingName = "builtin_dos_mouse_driver_model";

	const auto option_str = section.GetStringLowCase(SettingName);

	using enum MouseModelDos;

	auto new_model = mouse_config.model_dos;

	if (option_str == OptionModelDos::TwoButton) {
		new_model = TwoButton;
	} else if (option_str == OptionModelDos::ThreeButton) {
		new_model = ThreeButton;
	} else if (option_str == OptionModelDos::Wheel) {
		new_model = Wheel;
	} else {
		assertm(false, "Invalid DOS driver mouse model value");
	}

	if (new_model != mouse_config.model_dos) {
		mouse_config.model_dos = new_model;
		MOUSEDOS_NotifyModelChanged();
	}
}

static void set_ps2_mouse_model(const SectionProp& section)
{
	const std::string SettingName = "ps2_mouse_model";

	const auto option_str = section.GetStringLowCase(SettingName);

	using enum MouseModelPs2;

	if (option_str == OptionModelPs2::Standard) {
		mouse_config.model_ps2 = Standard;
	} else if (option_str == OptionModelPs2::Intellimouse) {
		mouse_config.model_ps2 = IntelliMouse;
	} else if (option_str == OptionModelPs2::Explorer) {
		mouse_config.model_ps2 = Explorer;
	} else if (option_str == OptionModelPs2::NoMouse) {
		mouse_config.model_ps2 = NoMouse;
	} else {
		assertm(false, "Invalid PS/2 mouse model value");
	}
}

static void set_serial_mouse_model(const SectionProp& section)
{
	const std::string SettingName = "com_mouse_model";

	const auto option_str = section.GetString(SettingName);

	[[maybe_unused]] const auto result = MOUSECOM_ParseComModel(
	        option_str, mouse_config.model_com, mouse_config.model_com_auto_msm);
	assert(result);

	is_serial_mouse_model_read = true;
}

static void set_dos_driver_move_threshold(const SectionProp& section)
{
	const std::string SettingName = "builtin_dos_mouse_driver_move_threshold";

	const auto option_str = section.GetString(SettingName);
	const auto DefaultStr = std::to_string(Mouse::DefaultMoveThreshold);

	auto set_new_value = [&](const std::string& value) {
		log_invalid_parameter(SettingName, option_str, value);
		set_section_property_value(SectionName, SettingName, value);
		set_dos_driver_move_threshold(section);
	};

	const auto tokens = split(option_str, " \t,;");
	if (tokens.empty() || tokens.size() > 2) {
		set_new_value(DefaultStr);
		return;
	}

	const auto value_x = parse_int(tokens[0]);
	const auto value_y = (tokens.size() >= 2) ? parse_int(tokens[1]) : value_x;

	if (!value_x || !value_y) {
		set_new_value(DefaultStr);
		return;
	}

	const auto clamped_x = std::clamp(*value_x,
	                                  Mouse::MinMoveThreshold,
	                                  Mouse::MaxMoveThreshold);
	const auto clamped_y = std::clamp(*value_y,
	                                  Mouse::MinMoveThreshold,
	                                  Mouse::MaxMoveThreshold);

	if (*value_x != clamped_x || *value_y != clamped_y) {
		auto adapted = std::to_string(clamped_x);
		if (tokens.size() > 1) {
			adapted += ",";
			adapted += std::to_string(clamped_y);
		}

		set_new_value(adapted);
		return;
	}

	mouse_config.dos_driver_move_threshold_x = static_cast<float>(*value_x);
	mouse_config.dos_driver_move_threshold_y = static_cast<float>(*value_y);
}

static void set_dos_driver_options(const SectionProp& section)
{
	const std::string SettingName = "builtin_dos_mouse_driver_options";

	const auto option_str = section.GetString(SettingName);

	constexpr auto DefaultValue = Mouse::DefaultDriverOptions;

	const std::string OptionImmediate     = "immediate";
	const std::string OptionModern        = "modern";
	const std::string OptionNoGranularity = "no-granularity";

	auto set_new_value = [&](const std::string& value) {
		log_invalid_parameter(SettingName, option_str, value);
		set_section_property_value(SectionName, SettingName, value);
		set_dos_driver_options(section);
	};

	mouse_config.dos_driver_immediate      = false;
	mouse_config.dos_driver_modern         = false;
	mouse_config.dos_driver_no_granularity = false;

	for (const auto& token : split(option_str, " \t,;")) {
		if (token == OptionImmediate) {
			mouse_config.dos_driver_immediate = true;
		} else if (token == OptionModern) {
			mouse_config.dos_driver_modern = true;
		} else if (token == OptionNoGranularity) {
			mouse_config.dos_driver_no_granularity = true;
		} else {
			set_new_value(DefaultValue);
			return;
		}
	}

	// Log new config options
	static std::string last_logged_str = "";
	if (option_str == last_logged_str) {
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
	last_logged_str = option_str;
}

static void set_mouse_sensitivity(const SectionProp& section)
{
	constexpr auto SettingName = "mouse_sensitivity";

	const auto option_str = section.GetString(SettingName);
	const auto DefaultStr = std::to_string(Mouse::DefaultSensitivity);

	auto set_value = [&](const std::string& value) {
		set_section_property_value(SectionName, SettingName, value);
		set_mouse_sensitivity(section);
	};

	auto tokens = split(option_str, " \t,;");
	if (tokens.empty() || tokens.size() > 2) {
		log_invalid_parameter(SettingName, option_str, DefaultStr);
		set_value(DefaultStr);
		return;
	}

	const auto value_x = parse_int(tokens[0]);
	const auto value_y = (tokens.size() >= 2) ? parse_int(tokens[1]) : value_x;

	if (!value_x || !value_y) {
		log_invalid_parameter(SettingName, option_str, DefaultStr);
		set_value(DefaultStr);
		return;
	}

	const auto clamped_x = std::clamp(*value_x,
	                                  Mouse::MinSensitivity,
	                                  Mouse::MaxSensitivity);
	const auto clamped_y = std::clamp(*value_y,
	                                  Mouse::MinSensitivity,
	                                  Mouse::MaxSensitivity);

	if (*value_x != clamped_x || *value_y != clamped_y) {
		auto adapted = std::to_string(clamped_x);
		if (tokens.size() > 1) {
			adapted += ",";
			adapted += std::to_string(clamped_y);
		}

		log_invalid_parameter(SettingName, option_str, adapted);
		set_value(adapted);
		return;
	}

	auto convert_value = [](const int value) {
		// Coefficient to convert percentage in integer to float
		constexpr float Coefficient = 0.01f;

		return Coefficient * static_cast<float>(value);
	};

	mouse_config.sensitivity_coeff_x = convert_value(*value_x);
	mouse_config.sensitivity_coeff_y = convert_value(*value_y);
}

static void set_multi_display_aware(const SectionProp& section)
{
	const std::string OptionStr = "mouse_multi_display_aware";

	mouse_config.multi_display_aware = section.GetBool(OptionStr);
}

static void set_middle_release(const SectionProp& section)
{
	const std::string OptionStr = "mouse_middle_release";

	mouse_config.middle_release = section.GetBool(OptionStr);
}

static void set_raw_input(const SectionProp& section)
{
	const std::string OptionStr = "mouse_raw_input";

	mouse_config.raw_input = section.GetBool(OptionStr);
}

static void set_vmware_mouse(const SectionProp& section)
{
	const std::string OptionStr = "vmware_mouse";

	mouse_config.is_vmware_mouse_enabled = section.GetBool(OptionStr);
}

static void set_virtualbox_mouse(const SectionProp& section)
{
	const std::string OptionStr = "virtualbox_mouse";

	mouse_config.is_virtualbox_mouse_enabled = section.GetBool(OptionStr);

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
}

void MOUSE_Init()
{
	const auto section = get_section(SectionName.c_str());
	assert(section);

	set_capture_type(*section);
	set_mouse_sensitivity(*section);
	set_multi_display_aware(*section);
	set_middle_release(*section);
	set_raw_input(*section);

	// Built-in DOS driver configuration
	set_dos_driver(*section);
	set_dos_driver_model(*section);
	set_dos_driver_move_threshold(*section);
	set_dos_driver_options(*section);

	// PS/2 AUX port mouse configuration
	set_ps2_mouse_model(*section);

	// COM port mouse configuration
	if (!is_serial_mouse_model_read) {
		set_serial_mouse_model(*section);
	}

	// Virtual Machine Manager (VMM) mouse interfaces
	set_vmware_mouse(*section);
	set_virtualbox_mouse(*section);

	// Start mouse emulation if everything is ready
	mouse_shared.ready_config = true;
	mouse_shared.ready_init   = true;
	MOUSE_StartupIfReady();
}

static void notify_mouse_setting_updated(SectionProp& section,
                                         const std::string& prop_name)
{
	if (prop_name == "builtin_dos_mouse_driver_model") {
		set_dos_driver_model(section);

	} else if (prop_name == "builtin_dos_mouse_driver_move_threshold") {
		set_dos_driver_move_threshold(section);

	} else if (prop_name == "builtin_dos_mouse_driver_options") {
		set_dos_driver_options(section);

	} else if (prop_name == "mouse_capture") {
		set_capture_type(section);

		if (mouse_config.capture == MouseCapture::NoMouse) {
			// If NoMouse got configured at runtime,
			// immediately clear all the mappings.
			MouseControlAPI mouse_config_api;
			mouse_config_api.UnMap(MouseControlAPI::ListIDs());
		}
		MOUSE_UpdateGFX();

	} else if (prop_name == "mouse_middle_release") {
		set_middle_release(section);

	} else if (prop_name == "mouse_multi_display_aware") {
		set_multi_display_aware(section);

	} else if (prop_name == "mouse_raw_input") {
		set_raw_input(section);

	} else if (prop_name == "mouse_sensitivity") {
		set_mouse_sensitivity(section);
	}
}

static void init_mouse_config_settings(SectionProp& secprop)
{
	using enum Property::Changeable::Value;

	// General configuration

	auto prop_str = secprop.AddString("mouse_capture",
	                                  Always,
	                                  OptionCaptureType::OnClick);
	assert(prop_str);
	prop_str->SetValues({OptionCaptureType::Seamless,
	                     OptionCaptureType::OnClick,
	                     OptionCaptureType::OnStart,
	                     OptionCaptureType::NoMouse});
	prop_str->SetHelp(
	        "Set the mouse capture behaviour ('onclick' by default). Possible values:\n"
	        "\n"
	        "  onclick:   Capture the mouse when clicking any mouse button in the window\n"
	        "             (default).\n"
	        "\n"
	        "  onstart:   Capture the mouse immediately on start. Might not work correctly\n"
	        "\n"
	        "             on some host operating systems.\n"
	        "  seamless:  Let the mouse move seamlessly between the DOSBox window and the\n"
	        "             rest of the desktop; captures only with middle-click or hotkey.\n"
	        "             Seamless mouse does not work correctly with all the games.\n"
	        "             Windows 3.1x can be made compatible with a custom mouse driver.\n"
	        "\n"
	        "  nomouse:   Hide the mouse and don't send mouse input to the game.\n"
	        "\n"
	        "Note: Use 'seamless' mode for touch screens.");

	auto prop_bool = secprop.AddBool("mouse_middle_release", Always, true);
	prop_bool->SetHelp(
	        "Release the captured mouse by middle-clicking, and also capture it in seamless\n"
	        "mode ('on' by default).");

	prop_bool = secprop.AddBool("mouse_multi_display_aware", Always, true);
	prop_bool->SetHelp(
	        "Allow seamless mouse behavior and mouse pointer release to work in fullscreen\n"
	        "mode on systems with more than one display ('on' by default).\n"
	        "\n"
	        "Note: You should disable this if it incorrectly detects multiple displays\n"
	        "      when only one should actually be used. This might happen if you are\n"
	        "      using mirrored display mode or using an AV receiver's HDMI input for\n"
	        "      audio-only listening.");

	prop_str = secprop.AddString("mouse_sensitivity",
	                             Always,
	                             std::to_string(Mouse::DefaultSensitivity).c_str());
	prop_str->SetHelp(
	        "Set global mouse sensitivity (100 by default). Possible values:\n"
	        "\n"
	        "  <value>:  Set sensitivity for both axes as a percentage (e.g. 150).\n"
	        "\n"
	        "  X,Y:      Set X and Y axis sensitivity separately as percentages (e.g.,\n"
	        "            100,150). The two values can be separated by a space or a semicolon\n"
	        "            as well.\n"
	        "\n"
	        "Notes:\n"
	        "  - Negative values invert an axis, zero disables it.\n"
	        "\n"
	        "  - Sensitivity can be fine-tuned futher per mouse interface with the internal\n"
	        "    MOUSECTL.COM command.");

	prop_bool = secprop.AddBool("mouse_raw_input", Always, true);
	prop_bool->SetHelp(
	        "Bypass the mouse acceleration and sensitivity settings of the host operating\n"
	        "system ('on' by default). Works in fullscreen or when the mouse is captured\n"
	        "in windowed mode.");

	// Built-in DOS driver configuration

	prop_str = secprop.AddString("builtin_dos_mouse_driver", OnlyAtStart, "on");
	assert(prop_str);
	prop_str->SetValues({"off", "on", OptionBuiltInDosDriver::NoTsr});
	prop_str->SetHelp(
	        "Built-in DOS mouse driver mode ('on' by default). It bypasses the PS/2 and\n"
	        "serial (COM) ports and communicates with the mouse directly. This results in\n"
	        "lower input lag, smoother movement, and increased mouse responsiveness, so only\n"
	        "disable it and load a real DOS mouse driver if it's really necessary (e.g., if a\n"
	        "game is not compatible with the built-in driver). Possible values:\n"
	        "\n"
	        "  on:      Simulate a mouse driver TSR program loaded from AUTOEXEC.BAT\n"
	        "           (default). This is the most compatible way to emulate the DOS mouse\n"
	        "           driver, but if it doesn't work with your game, try the 'no-tsr'\n"
	        "           setting.\n"
	        "\n"
	        "  no-tsr:  Enable the mouse driver without simulating the TSR program. Let us\n"
	        "           know if it fixes any software not working with the 'on' setting.\n"
	        "\n"
	        "  off:     Disable the built-in mouse driver. You can still start it at runtime\n"
	        "           by executing the bundled MOUSE.COM from drive Z.\n"
	        "\n"
	        "Notes:\n"
	        "  - The `ps2_mouse_model` and `com_mouse_model` settings have no effect on the\n"
	        "    built-in driver.\n"
	        "\n"
	        "  - The driver is auto-disabled if you boot into real MS-DOS or Windows 9x\n"
	        "    under DOSBox. Under Windows 3.x, the driver is not disabled, but the\n"
	        "    Windows 3.x mouse driver takes over.\n"
	        "\n"
	        "  - To use a real DOS mouse driver (e.g., MOUSE.COM or CTMOUSE.EXE), configure\n"
	        "    the mouse type with `ps2_mouse_model` or `com_mouse_model`, then load the\n"
	        "    driver.\n");

	prop_bool = secprop.AddBool("dos_mouse_driver", Deprecated, true);
	prop_bool->SetHelp(
	        "Renamed to [color=light-green]'builtin_dos_mouse_driver'[reset].");

	prop_str = secprop.AddString("builtin_dos_mouse_driver_model",
	                             Always,
	                             OptionModelDos::TwoButton);
	assert(prop_str);
	prop_str->SetValues({OptionModelDos::TwoButton,
	                     OptionModelDos::ThreeButton,
	                     OptionModelDos::Wheel});
	prop_str->SetHelp(
	        "Set the mouse model to be simulated by the built-in DOS mouse driver ('2button'\n"
	        "by default). Possible values:\n"
	        "\n"
	        "  2button:  2 buttons, the safest option for most games. The majority of DOS\n"
	        "            games only support 2 buttons, some might misbehave if the middle\n"
	        "            button is pressed.\n"
	        "\n"
	        "  3button:  3 buttons, only supported by very few DOS games. Only use this if\n"
	        "            the game is known to support a 3-button mouse.\n"
	        "\n"
	        "  wheel:    3 buttons + wheel, supports the CuteMouse WheelAPI version 1.0.\n"
	        "            No DOS game uses the mouse wheel, only a handful of DOS applications\n"
	        "            and Windows 3.x with special third-party drivers.");

	prop_str = secprop.AddString("builtin_dos_mouse_driver_move_threshold",
	                             Always,
	                             std::to_string(Mouse::DefaultMoveThreshold).c_str());
	assert(prop_str);
	prop_str->SetHelp(
	        "The smallest amount of mouse movement that will be reported to the guest\n"
	        "(1 by default). Some DOS games cannot properly respond to small movements, which\n"
	        "were hard to achieve using the imprecise ball mice of the era; in such case\n"
	        "increase the amount to the smallest value that results in a proper cursor\n"
	        "motion. Possible values:\n"
	        "\n"
	        "  1-9:  The smallest amount of movement to report, for both horizontal and\n"
	        "        vertical axes. 1 reports all the movements (default).\n"
	        "\n"
	        "  x,y:  Separate values for horizontal and vertical axes, can be separated by\n"
	        "        spaces, commas, or semicolons.\n"
	        "\n"
	        "List of known games requiring the threshold to be set to 2:\n"
	        "  - Ultima Underworld: The Stygian Abyss\n"
	        "  - Ultima Underworld II: Labyrinth of Worlds");

	prop_str = secprop.AddString("builtin_dos_mouse_driver_options",
	                             Always,
	                             Mouse::DefaultDriverOptions);
	assert(prop_str);
	prop_str->SetHelp(
	        "Additional built-in DOS mouse driver settings as a list of space or comma\n"
	        "separated options (unset by default). Possible values:\n"
	        "\n"
	        "  immediate:       Update mouse movement counters immediately, without waiting\n"
	        "                   for interrupt. May improve mouse latency in fast-paced games\n"
	        "                   (arcade, FPS, etc.), but might cause issues in some titles.\n"
	        "                   List of known incompatible games:\n"
	        "                     - Ultima Underworld: The Stygian Abyss\n"
	        "                     - Ultima Underworld II: Labyrinth of Worlds\n"
	        "                   Please report other incompatible games so we can update this\n"
	        "                   list.\n"
	        "\n"
	        "  modern:          If provided, v7.0+ Microsoft mouse driver behaviour is\n"
	        "                   emulated, otherwise the v6.0 and earlier behaviour (the two\n"
	        "                   are slightly incompatible). Only 'Descent II' with the\n"
	        "                   official Voodoo patch has been found to require the v7.0+\n"
	        "                   behaviour so far.\n"
	        "\n"
	        "  no-granularity:  Disables the mouse position granularity. Only enable if the\n"
	        "                   game needs it. Only 'Joan of Arc: Siege & the Sword' in\n"
	        "                   Hercules mode has been found to require disabled granularity\n"
	        "                   so far.");

	prop_bool = secprop.AddBool("dos_mouse_immediate", Deprecated, false);
	prop_bool->SetHelp(
	        "Configure using [color=light-green]'builtin_dos_mouse_driver_options'[reset].");

	// Physical mice configuration

	// TODO: PS/2 mouse might be hot-pluggable
	prop_str = secprop.AddString("ps2_mouse_model",
	                             OnlyAtStart,
	                             OptionModelPs2::Explorer);
	assert(prop_str);
	prop_str->SetValues({OptionModelPs2::Standard,
	                     OptionModelPs2::Intellimouse,
	                     OptionModelPs2::Explorer,
	                     OptionModelPs2::NoMouse});
	prop_str->SetHelp(
	        "Set the PS/2 AUX port mouse model, or in other words, the type of the virtual\n"
	        "mouse plugged into the emulated PS/2 mouse port ('explorer' by default). The\n"
	        "setting has no effect on the built-in mouse driver (see 'dos_mouse_driver').\n"
	        "Possible values:\n"
	        "\n"
	        "  standard:      3 buttons, standard PS/2 mouse.\n"
	        "  intellimouse:  3 buttons + wheel, Microsoft IntelliMouse.\n"
	        "  explorer:      5 buttons + wheel, Microsoft IntelliMouse Explorer (default).\n"
	        "  none:          no PS/2 mouse.");

	prop_str = secprop.AddString("com_mouse_model",
	                             OnlyAtStart,
	                             OptionModelCom::WheelMsm);
	assert(prop_str);
	prop_str->SetValues({OptionModelCom::TwoButton,
	                     OptionModelCom::ThreeButton,
	                     OptionModelCom::Wheel,
	                     OptionModelCom::Msm,
	                     OptionModelCom::TwoButtonMsm,
	                     OptionModelCom::ThreeButtonMsm,
	                     OptionModelCom::WheelMsm});
	prop_str->SetHelp(
	        "Set the default COM (serial) mouse model, or in other words, the type of the\n"
	        "virtual mouse plugged into the emulated COM ports ('wheel+msm' by default).\n"
	        "The setting has no effect on the built-in mouse driver (see 'dos_mouse_driver').\n"
	        "Possible values:\n"
	        "\n"
	        "  2button:      2 buttons, Microsoft mouse.\n"
	        "\n"
	        "  3button:      3 buttons, Logitech mouse; mostly compatible with Microsoft\n"
	        "                mouse.\n"
	        "\n"
	        "  wheel:        3 buttons + wheel; mostly compatible with Microsoft mouse.\n"
	        "\n"
	        "  msm:          3 buttons, Mouse Systems mouse; NOT compatible with Microsoft\n"
	        "                mouse.\n"
	        "\n"
	        "  2button+msm:  Automatic choice between '2button' and 'msm'.\n"
	        "  3button+msm:  Automatic choice between '3button' and 'msm'.\n"
	        "  wheel+msm:    Automatic choice between 'wheel' and 'msm' (default).\n"
	        "\n"
	        "Note: Enable COM port mice in the [serial] section.");

	// VMM interfaces

	prop_bool = secprop.AddBool("vmware_mouse", OnlyAtStart, true);
	prop_bool->SetHelp(
	        "VMware mouse interface ('on' by default). Fully compatible only with 3rd party\n"
	        "Windows 3.1x driver.\n"
	        "\n"
	        "Note: Requires PS/2 mouse to be enabled.");

	prop_bool = secprop.AddBool("virtualbox_mouse", OnlyAtStart, true);
	prop_bool->SetHelp(
	        "VirtualBox mouse interface ('on' by default). Fully compatible only with 3rd\n"
	        "party Windows 3.1x driver.\n"
	        "\n"
	        "Note: Requires PS/2 mouse to be enabled.");
}

void MOUSE_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection(SectionName.c_str());
	section->AddUpdateHandler(notify_mouse_setting_updated);

	init_mouse_config_settings(*section);
}

MouseModelCom MOUSECOM_GetConfiguredModel()
{
	if (!is_serial_mouse_model_read) {

		const auto section = get_section(SectionName.c_str());
		assert(section);

		set_serial_mouse_model(*section);
	}

	return mouse_config.model_com;
}

bool MOUSECOM_GetConfiguredAutoMsm()
{
	if (!is_serial_mouse_model_read) {

		const auto section = get_section(SectionName.c_str());
		assert(section);

		set_serial_mouse_model(*section);
	}

	return mouse_config.model_com_auto_msm;
}
