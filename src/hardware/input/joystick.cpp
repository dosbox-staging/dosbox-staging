// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "joystick.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <memory>

#include "config/config.h"
#include "config/setup.h"
#include "gui/mapper.h"
#include "hardware/input/mouse.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "ints/bios.h"
#include "ints/int10.h"
#include "utils/math_utils.h"

//TODO: higher axis can't be mapped. Find out why again

//Set to true, to enable automated switching back to square on circle mode if the inputs are outside the cirle.
#define SUPPORT_MAP_AUTO 0

constexpr int RANGE = 64;
constexpr int TIMEOUT = 10;

enum MovementType {
	JOYMAP_SQUARE,
	JOYMAP_CIRCLE,
	JOYMAP_INBETWEEN,
};

struct AxisRateConstants {
	char axis     = '\0';
	double scalar = 0.0;
	double offset = 0.0;
};

struct JoystickAxisRates {
	AxisRateConstants x = {};
	AxisRateConstants y = {};
};

constexpr JoystickAxisRates default_axis_rates = {
        {'x', 1.112 / 2, 0.02},
        {'y', 1.110 / 2, 0.02},
};

struct JoyStick {
	double xpos = 0.0;
	double ypos = 0.0; // position as set by SDL

	double xtick = 0.0;
	double ytick = 0.0;

	double xfinal = 0.0;
	double yfinal = 0.0; // position returned to the game for stick 0

	uint32_t xcount = 0;
	uint32_t ycount = 0;

	int deadzone = 0; // Deadzone (value between 0 and 100) interpreted as percentage
	enum MovementType mapstate = JOYMAP_SQUARE;

	bool button[2] = {false, false};
	bool transformed = false; // Whether xpos,ypos have been converted to xfinal and yfinal
	                          // Cleared when new xpos orypos have been set
	bool enabled = false;
	bool is_visible_to_dos = false;

	void clip() {
		xfinal = clamp(xfinal, -1.0, 1.0);
		yfinal = clamp(yfinal, -1.0, 1.0);
	}

	void fake_digital() {
		if (xpos > 0.5)
			xfinal = 1.0;
		else if (xpos < -0.5)
			xfinal = -1.0;
		else
			xfinal = 0.0;
		if (ypos > 0.5)
			yfinal = 1.0;
		else if (ypos < -0.5)
			yfinal = -1.0;
		else
			yfinal = 0.0;
	}

	void transform_circular(){
		const auto r = sqrt(xpos * xpos + ypos * ypos);
		if (fabs(r) < DBL_EPSILON) {
			xfinal = xpos;
			yfinal = ypos;
			return;
		}
		const auto deadzone_f = static_cast<double>(deadzone) / 100.0;
		const auto s = 1.0 - deadzone_f;
		if (r < deadzone_f) {
			xfinal = yfinal = 0.0;
			return;
		}

		const auto deadzonescale = (r - deadzone_f) / s; // r if deadzone=0;
		const auto xa = fabs(xpos);
		const auto ya = fabs(ypos);
		const auto maxpos = std::max(ya, xa);
		xfinal = xpos * deadzonescale / maxpos;
		yfinal = ypos * deadzonescale / maxpos;
	}

	void transform_square() {
		const auto deadzone_f = static_cast<double>(deadzone) / 100.0;
		const auto s = 1.0 - deadzone_f;

		if (xpos > deadzone_f) {
			xfinal = (xpos - deadzone_f)/ s;
		} else if ( xpos < -deadzone_f) {
			xfinal = (xpos + deadzone_f) / s;
		} else
			xfinal = 0.0;
		if (ypos > deadzone_f) {
			yfinal = (ypos - deadzone_f)/ s;
		} else if (ypos < -deadzone_f) {
			yfinal = (ypos + deadzone_f) / s;
		} else
			yfinal = 0.0;
	}

#if SUPPORT_MAP_AUTO
	void transform_inbetween(){
		//First transform to a circle and crop the values to -1.0 -> 1.0
		//then keep on doing this in future calls until it is safe to switch square mapping
		// safe = 0.95 as ratio  for both axis, or in deadzone
		transform_circular();
		clip();

		const auto xrate = xpos / xfinal;
		const auto yrate = ypos / yfinal;
		if (xrate > 0.95 && yrate > 0.95) {
			mapstate = JOYMAP_SQUARE; //TODO misschien xfinal=xpos...
			//LOG_MSG("switched to square %f %f",xrate,yrate);
		}
	}
#endif
	void transform_input(){
		if (transformed) return;
		transformed = true;
		if (deadzone == 100) fake_digital();
		else {
			if (mapstate == JOYMAP_SQUARE) transform_square();
			else if (mapstate == JOYMAP_CIRCLE) transform_circular();
#if SUPPORT_MAP_AUTO
			if (mapstate ==  JOYMAP_INBETWEEN) transform_inbetween(); //No else here
#endif
			clip();
		}
	}
};

JoystickType joytype = JOY_UNSET;
static JoyStick stick[2];
static auto calibrated_axis_rates = default_axis_rates;

static uint32_t last_write = 0;
static bool write_active = false;
static bool swap34 = false;
bool button_wrapping_enabled = true;

extern bool autofire; //mapper.cpp

static uint8_t read_p201(io_port_t, io_width_t)
{
	/* Reset Joystick to 0 after TIMEOUT ms */
	if(write_active && ((PIC_Ticks - last_write) > TIMEOUT)) {
		write_active = false;
		stick[0].xcount = 0;
		stick[1].xcount = 0;
		stick[0].ycount = 0;
		stick[1].ycount = 0;
//		LOG_MSG("reset by time %d %d",PIC_Ticks,last_write);
	}

	/**  Format of the byte to be returned:
	**                        | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	**                        +-------------------------------+
	**                          |   |   |   |   |   |   |   |
	**  Joystick B, Button 2 ---+   |   |   |   |   |   |   +--- Joystick A, X Axis
	**  Joystick B, Button 1 -------+   |   |   |   |   +------- Joystick A, Y Axis
	**  Joystick A, Button 2 -----------+   |   |   +----------- Joystick B, X Axis
	**  Joystick A, Button 1 ---------------+   +--------------- Joystick B, Y Axis
	**/
	uint8_t ret = 0xff;
	if (stick[0].enabled) {
		if (stick[0].xcount) stick[0].xcount--; else ret&=~1;
		if (stick[0].ycount) stick[0].ycount--; else ret&=~2;
		if (stick[0].button[0]) ret&=~16;
		if (stick[0].button[1]) ret&=~32;
	}
	if (stick[1].enabled) {
		if (stick[1].xcount) stick[1].xcount--; else ret&=~4;
		if (stick[1].ycount) stick[1].ycount--; else ret&=~8;
		if (stick[1].button[0]) ret&=~64;
		if (stick[1].button[1]) ret&=~128;
	}
	return ret;
}

static uint8_t read_p201_timed(io_port_t, io_width_t)
{
	uint8_t ret = 0xff;
	const auto currentTick = PIC_FullIndex();
	if( stick[0].enabled ){
		if( stick[0].xtick < currentTick ) ret &=~1;
		if( stick[0].ytick < currentTick ) ret &=~2;
	}
	if( stick[1].enabled ){
		if( stick[1].xtick < currentTick ) ret &=~4;
		if( stick[1].ytick < currentTick ) ret &=~8;
	}

	if (stick[0].enabled) {
		if (stick[0].button[0]) ret&=~16;
		if (stick[0].button[1]) ret&=~32;
	}
	if (stick[1].enabled) {
		if (stick[1].button[0]) ret&=~64;
		if (stick[1].button[1]) ret&=~128;
	}
	return ret;
}

static void write_p201(io_port_t, io_val_t, io_width_t)
{
	/* Store writetime index */
	write_active = true;
	last_write = PIC_Ticks;

	auto percent_to_count = [](auto percent) -> uint8_t {
		const auto scaled = static_cast<int>(round(percent * RANGE));
		const auto shifted = check_cast<uint8_t>(scaled + RANGE);
		return shifted;
	};

	if (stick[0].enabled) {
		stick[0].transform_input();
		stick[0].xcount = percent_to_count(stick[0].xfinal);
		stick[0].ycount = percent_to_count(stick[0].yfinal);
	}
	if (stick[1].enabled) {
		stick[1].xcount = percent_to_count(swap34 ? stick[1].ypos
		                                          : stick[1].xpos);
		stick[1].ycount = percent_to_count(swap34 ? stick[1].xpos
		                                          : stick[1].ypos);
	}
}
static void write_p201_timed(io_port_t, io_val_t, io_width_t)
{
	// Convert the joystick's instantaneous position to activation duration in ticks

	/*
	// Original calculation
	auto position_to_ticks = [](auto position) {
	        constexpr auto joystick_s_constant = 0.0000242;
	        constexpr auto ohms = 120000.0 / 2.0;
	        constexpr auto s_per_ohm = 0.000000011;
	        const auto resistance = position + 1.0;

	        // Axes take time = 24.2 microseconds + ( 0.011 microsecons/ohm
	* resistance) to reset to 0 const auto axis_time_us =
	joystick_s_constant + s_per_ohm * resistance * ohms; const auto
	axis_time_ms = 1000.0 * axis_time_us;

	        // finally, return the current tick plus the axis_time in
	milliseconds return PIC_FullIndex() + axis_time_ms;
	};
	*/

	const auto now = PIC_FullIndex();
	// Newer calculation, derived from joycheck measurements
	auto position_to_ticks = [&](const auto position,
	                             const AxisRateConstants &axis_rate) {
		return now + (position + 1.0) * axis_rate.scalar + axis_rate.offset;
	};

	if (stick[0].enabled) {
		stick[0].transform_input();
		stick[0].xtick = position_to_ticks(stick[0].xfinal,
		                                   calibrated_axis_rates.x);
		stick[0].ytick = position_to_ticks(stick[0].yfinal,
		                                   calibrated_axis_rates.y);
	}
	if (stick[1].enabled) {
		stick[1].xtick = position_to_ticks(swap34 ? stick[1].ypos
		                                          : stick[1].xpos,
		                                   calibrated_axis_rates.x);
		stick[1].ytick = position_to_ticks(swap34 ? stick[1].xpos
		                                          : stick[1].ypos,
		                                   calibrated_axis_rates.y);
	}
}

void JOYSTICK_Enable(uint8_t which, bool enabled)
{
	assert(which < 2);
	stick[which].enabled = enabled;
}

void JOYSTICK_Button(uint8_t which, int num, bool pressed)
{
	assert(which < 2);
	assert(num < 2);
	stick[which].button[num] = pressed;
}

constexpr double position_to_percent(int16_t val)
{
	// SDL's joystick axis value ranges from -32768 to 32767
	return val / (val > 0 ? 32767.0 : 32768.0);
}

void JOYSTICK_Move_X(uint8_t which, int16_t x_val)
{
	assert(which < 2);

	const auto x = position_to_percent(x_val);
	if (stick[which].xpos == x)
		return;
	stick[which].xpos = x;
	stick[which].transformed = false;
//	if( which == 0 || joytype != JOY_FCS)
//		stick[which].applied_conversion; //todo
}

void JOYSTICK_Move_Y(uint8_t which, int16_t y_val)
{
	assert(which < 2);
	const auto y = position_to_percent(y_val);
	if (stick[which].ypos == y)
		return;
	stick[which].ypos = y;
	stick[which].transformed = false;
}

bool JOYSTICK_IsAccessible(uint8_t which)
{
	assert(which < 2);
	const auto &s = stick[which];
	return s.is_visible_to_dos && s.enabled;
}

bool JOYSTICK_GetButton(uint8_t which, int num)
{
	assert(which < 2);
	assert(num < 2);
	return stick[which].button[num];
}

double JOYSTICK_GetMove_X(uint8_t which)
{
	assert(which < 2);
	if (which == 0) {
		stick[0].transform_input();
		return stick[0].xfinal;
	}
	return stick[1].xpos;
}

double JOYSTICK_GetMove_Y(uint8_t which)
{
	assert(which < 2);
	if (which == 0) {
		stick[0].transform_input();
		return stick[0].yfinal;
	}
	return stick[1].ypos;
}

void JOYSTICK_ParseConfiguredType()
{
	const auto conf    = get_joystick_section();
	const auto section = static_cast<SectionProp *>(conf);
	const auto type = section->GetString("joysticktype");

	if (type == "disabled")
		joytype = JOY_DISABLED;
	else if (type == "hidden")
		joytype = JOY_ONLY_FOR_MAPPING;
	else if (type == "auto")
		joytype = JOY_AUTO;
	else if (type == "2axis")
		joytype = JOY_2AXIS;
	else if (type == "4axis")
		joytype = JOY_4AXIS;
	else if (type == "4axis_2")
		joytype = JOY_4AXIS_2;
	else if (type == "fcs")
		joytype = JOY_FCS;
	else if (type == "ch")
		joytype = JOY_CH;
	else
		joytype = JOY_AUTO;

	assert(joytype != JOY_UNSET);
}

enum CalibrationType {
	NegativeScale,
	PositiveScale,
	NegativeOffset,
	PositiveOffset,
};

static void calibrate_axis(const bool pressed, AxisRateConstants &rates,
                           const CalibrationType calibration_type)
{
	if (!pressed)
		return;

	constexpr auto negative_scale = 0.95;
	constexpr auto positive_scale = 1.05;
	constexpr auto delta_offset   = 0.005;

	switch (calibration_type) {
	case NegativeScale: rates.scalar *= negative_scale; break;
	case PositiveScale: rates.scalar *= positive_scale; break;
	case NegativeOffset: rates.offset -= delta_offset; break;
	case PositiveOffset: rates.offset += delta_offset; break;
	}
	LOG_MSG("JOYSTICK: %c_calibration = %.6g,%.6g",
	        rates.axis,
	        rates.scalar,
	        rates.offset);
}

static void joy_x_scalar_left(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.x, NegativeScale);
}

static void joy_x_scalar_right(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.x, PositiveScale);
}

static void joy_x_offset_left(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.x, NegativeOffset);
}

static void joy_x_offset_right(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.x, PositiveOffset);
}
static void joy_x_reset(const bool pressed)
{
	if (pressed) {
		calibrated_axis_rates.x = default_axis_rates.x;
		LOG_MSG("JOYSTICK: joy_x_calibration = auto");
	}
}

// for the Y-axis, the increasing the timings moves the position down, and
// vice-versa

static void joy_y_scalar_down(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.y, PositiveScale);
}

static void joy_y_scalar_up(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.y, NegativeScale);
}

static void joy_y_offset_down(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.y, PositiveOffset);
}
static void joy_y_offset_up(const bool pressed)
{
	calibrate_axis(pressed, calibrated_axis_rates.y, NegativeOffset);
}
static void joy_y_reset(const bool pressed)
{
	if (pressed) {
		calibrated_axis_rates.y = default_axis_rates.y;
		LOG_MSG("JOYSTICK: joy_y_calibration = auto");
	}
}

static void activate_calibration_hotkeys()
{
	LOG_MSG("JOYSTICK: Activating joystick calibration hotkeys");
	// clang-format off
	MAPPER_AddHandler(&joy_x_scalar_left,  SDL_SCANCODE_LEFT,   PRIMARY_MOD, "jxsl", "JXSL");
	MAPPER_AddHandler(&joy_x_scalar_right, SDL_SCANCODE_RIGHT,  PRIMARY_MOD, "jxsr", "JXSR");
	MAPPER_AddHandler(&joy_x_offset_left,  SDL_SCANCODE_LEFT,   MMOD2,       "jxol", "JXOL");
	MAPPER_AddHandler(&joy_x_offset_right, SDL_SCANCODE_RIGHT,  MMOD2,       "jxor", "JXOR");
	MAPPER_AddHandler(&joy_x_reset,        SDL_SCANCODE_DELETE, PRIMARY_MOD, "jxrs", "JXRS");
	MAPPER_AddHandler(&joy_y_scalar_down,  SDL_SCANCODE_DOWN,   PRIMARY_MOD, "jysd", "JYSD");
	MAPPER_AddHandler(&joy_y_scalar_up,    SDL_SCANCODE_UP,     PRIMARY_MOD, "jysu", "JYSU");
	MAPPER_AddHandler(&joy_y_offset_down,  SDL_SCANCODE_DOWN,   MMOD2,       "jyod", "JYOD");
	MAPPER_AddHandler(&joy_y_offset_up,    SDL_SCANCODE_UP,     MMOD2,       "jyou", "JYOU");
	MAPPER_AddHandler(&joy_y_reset,        SDL_SCANCODE_HOME,   PRIMARY_MOD, "jyrs", "JYRS");
	// clang-format off
}

static void configure_calibration(const SectionProp &settings)
{
	if (settings.GetBool("use_joy_calibration_hotkeys"))
		activate_calibration_hotkeys();

	auto axis_rates_from_pref = [](const std::string& pref,
	                               const AxisRateConstants &default_rates) {
		if (AxisRateConstants parsed_rates = {};
		    sscanf(pref.c_str(), "%lf,%lf", &parsed_rates.scalar, &parsed_rates.offset) == 2) {
			LOG_MSG("JOYSTICK: Loaded custom %c-axis calibration parameters (%.6g,%.6g)",
			        default_rates.axis,
			        parsed_rates.scalar,
			        parsed_rates.offset);
			return parsed_rates;
		}
		if (pref != "auto" && pref.length() != 0)
			LOG_WARNING("JOYSTICK: Invalid '%c_calibration' setting: '%s', "
			            "using 'auto'",
			            default_rates.axis,
			            pref.c_str());
		return default_rates;
	};
	const auto x_cal_pref   = settings.GetString("joy_x_calibration");
	calibrated_axis_rates.x = axis_rates_from_pref(x_cal_pref, default_axis_rates.x);

	const auto y_cal_pref   = settings.GetString("joy_y_calibration");
	calibrated_axis_rates.y = axis_rates_from_pref(y_cal_pref, default_axis_rates.y);
}

class JOYSTICK {
private:
	IO_ReadHandleObject ReadHandler = {};
	IO_WriteHandleObject WriteHandler = {};

public:
	JOYSTICK(SectionProp& section)
	{
		JOYSTICK_ParseConfiguredType();

		// Does the user want joysticks to be entirely disabled, both in SDL and DOS?
		if (joytype == JOY_DISABLED)
			return;

		// Get and apply configuration settings
		autofire = section.GetBool("autofire");
		button_wrapping_enabled = section.GetBool("buttonwrap");
		stick[0].deadzone = section.GetInt("deadzone");
		swap34 = section.GetBool("swap34");
		stick[0].mapstate = section.GetBool("circularinput") ? MovementType::JOYMAP_CIRCLE
		                                                       : MovementType::JOYMAP_SQUARE;
		configure_calibration(section);

		// Set initial time and position states
		const auto ticks = PIC_FullIndex();
		stick[0].xtick = ticks;
		stick[0].ytick = ticks;
		stick[1].xtick = ticks;
		stick[1].ytick = ticks;
		stick[0].xpos = 0.0;
		stick[0].ypos = 0.0;
		stick[1].xpos = 0.0;
		stick[1].ypos = 0.0;
		stick[0].transformed = false;

		// Does the user want joysticks to visible and usable in DOS?
		const bool is_visible = (joytype != JOY_ONLY_FOR_MAPPING &&
		                         joytype != JOY_DISABLED);
		stick[0].is_visible_to_dos = is_visible;
		stick[1].is_visible_to_dos = is_visible;

		// Setup the joystick IO port handlers, which lets DOS games
		// detect and use them
		if (is_visible) {
			const bool wants_timed = section.GetBool("timed");
			ReadHandler.Install(0x201,
			                    wants_timed ? read_p201_timed : read_p201,
			                    io_width_t::byte);
			WriteHandler.Install(0x201,
			                     wants_timed ? write_p201_timed : write_p201,
			                     io_width_t::byte);
		}
	}
	~JOYSTICK() {
		// No-op if IO handlers were not installed
		WriteHandler.Uninstall();
		ReadHandler.Uninstall();
	}
};

static std::unique_ptr<JOYSTICK> joystick = nullptr;

void JOYSTICK_Init()
{
	const auto section = get_section("joystick");
	assert(section);

	joystick = std::make_unique<JOYSTICK>(*section);
}

void JOYSTICK_Destroy()
{
	joystick = {};
}

static void notify_joystick_setting_updated(SectionProp& section, [[maybe_unused]] const std::string& prop_name)
{
	joystick = std::make_unique<JOYSTICK>(section);
}

static void init_joystick_config_settings(SectionProp& secprop)
{
	using enum Property::Changeable::Value;

	auto pstring = secprop.AddString("joysticktype", WhenIdle, "auto");

	pstring->SetValues(
	        {"auto", "2axis", "4axis", "4axis_2", "fcs", "ch", "hidden", "disabled"});

	pstring->SetHelp(
	        "Type of joystick to emulate ('auto' by default). Possible values:\n"
			"\n"
	        "  auto:      Detect and use any joystick(s), if possible (default).\n"
	        "             Joystick emulation is disabled if no joystick is found.\n"
			"\n"
	        "  2axis:     Support up to two joysticks, each with 2 axis.\n"
			"\n"
	        "  4axis:     Support the first joystick only, as a 4-axis type.\n"
			"\n"
	        "  4axis_2:   Support the second joystick only, as a 4-axis type.\n"
			"\n"
	        "  fcs:       Emulate joystick as an original Thrustmaster FCS.\n"
			"\n"
	        "  ch:        Emulate joystick as an original CH Flightstick.\n"
			"\n"
	        "  hidden:    Prevent DOS from seeing the joystick(s), but enable them\n"
	        "             for mapping.\n"
			"\n"
	        "  disabled:  Fully disable joysticks: won't be polled, mapped,\n"
	        "             or visible in DOS.\n"
			"\n"
	        "Note: Remember to reset DOSBox's mapperfile if you saved it earlier.");

	auto pbool = secprop.AddBool("timed", WhenIdle, true);
	pbool->SetHelp(
	        "Enable timed intervals for axis ('on' by default). Experiment with this option\n"
			"if your joystick drifts away.");

	pbool = secprop.AddBool("autofire", WhenIdle, false);
	pbool->SetHelp("Fire continuously as long as the button is pressed ('off' by default)");

	pbool = secprop.AddBool("swap34", WhenIdle, false);
	pbool->SetHelp(
	        "Swap the 3rd and the 4th axis ('off' by default). Can be useful for certain\n"
	        "joysticks.");

	pbool = secprop.AddBool("buttonwrap", WhenIdle, false);
	pbool->SetHelp("Enable button wrapping at the number of emulated buttons ('off' by default).");

	pbool = secprop.AddBool("circularinput", WhenIdle, false);
	pbool->SetHelp(
	        "Enable translation of circular input to square output ('off' by default).\n"
	        "Try enabling this if your left analog stick can only move in a circle.");

	auto pint = secprop.AddInt("deadzone", WhenIdle, 10);
	pint->SetMinMax(0, 100);
	pint->SetHelp(
	        "Percentage of motion to ignore (10 by default). Valid range is 0 to 100.\n"
			"100 turns the stick into a digital one.");

	pbool = secprop.AddBool("use_joy_calibration_hotkeys", WhenIdle, false);
	pbool->SetHelp(
	        "Enable hotkeys to allow realtime calibration of the joystick's X and Y axes\n"
	        "('off' by default). Only consider this as a last resort if in-game calibration\n"
	        "doesn't work correctly. Instructions:\n"
			"\n"
	        "  - Press Ctrl/Cmd+Arrow-keys adjust the axis' scalar value:\n"
	        "      - Left and Right diminish or magnify the x-axis scalar, respectively.\n"
	        "      - Down and Up diminish or magnify the y-axis scalar, respectively.\n"
			"\n"
	        "  - Press Alt+Arrow-keys adjust the axis' offset position:\n"
	        "      - Left and Right shift X-axis offset in the given direction.\n"
	        "      - Down and Up shift the Y-axis offset in the given direction.\n"
			"\n"
	        "  - Reset the X and Y calibration using Ctrl+Delete and Ctrl+Home, respectively.\n"
			"\n"
	        "Each tap will report X or Y calibration values you can set below. When you find\n"
	        "parameters that work, quit the game, switch this setting back to disabled, and\n"
	        "populate the reported calibration parameters.");

	pstring = secprop.AddString("joy_x_calibration", WhenIdle, "auto");
	pstring->SetHelp(
	        "Apply X-axis calibration parameters from the hotkeys ('auto' by default).");

	pstring = secprop.AddString("joy_y_calibration", WhenIdle, "auto");
	pstring->SetHelp(
	        "Apply Y-axis calibration parameters from the hotkeys ('auto' by default).");
}

void JOYSTICK_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("joystick");
	section->AddUpdateHandler(notify_joystick_setting_updated);

	init_joystick_config_settings(*section);
}
