/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "joystick.h"

#include <cfloat>
#include <string.h>
#include <math.h>

#include "control.h"
#include "inout.h"
#include "pic.h"
#include "support.h"

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

struct JoyStick {
	double xpos = 0.0;
	double ypos = 0.0; // position as set by SDL

	double xtick = 0.0;
	double ytick = 0.0;

	double xfinal = 0.0;
	double yfinal = 0.0; // position returned to the game for stick 0

	uint8_t xcount = 0;
	uint8_t ycount = 0;

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

static uint32_t last_write = 0;
static bool write_active = false;
static bool swap34 = false;
bool button_wrapping_enabled = true;

extern bool autofire; //sdl_mapper.cpp

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
	const auto current_tick = PIC_FullIndex();

	// Convert the the joystick's instantaneous position to activation duration in ticks
	auto position_to_ticks = [current_tick](auto position) {
		constexpr auto joystick_s_constant = 0.0000242;
		constexpr auto ohms = 120000.0 / 2.0;
		constexpr auto s_per_ohm = 0.000000011;
		const auto resistance = position + 1.0;

		// Axes take time = 24.2 microseconds + ( 0.011 microsecons/ohm * resistance) to reset to 0
		const auto axis_time_us = joystick_s_constant + s_per_ohm * resistance * ohms;
		const auto axis_time_ms = 1000.0 * axis_time_us;

		// finally, return the current tick plus the axis_time in milliseconds
		return current_tick + axis_time_ms;
	};

	if (stick[0].enabled) {
		stick[0].transform_input();
		stick[0].xtick = position_to_ticks(stick[0].xfinal);
		stick[0].ytick = position_to_ticks(stick[0].yfinal);
	}
	if (stick[1].enabled) {
		stick[1].xtick = position_to_ticks(swap34 ? stick[1].ypos : stick[1].xpos);
		stick[1].ytick = position_to_ticks(swap34 ? stick[1].xpos : stick[1].ypos);
	}
}

void JOYSTICK_Enable(uint8_t which, bool enabled)
{
	if (which < 2)
		stick[which].enabled = enabled;
}

void JOYSTICK_Button(uint8_t which, int num, bool pressed)
{
	if ((which < 2) && (num < 2))
		stick[which].button[num] = pressed;
}

constexpr double position_to_percent(int16_t val)
{
	// SDL's joystick axis value ranges from -32768 to 32767
	return val / (val > 0 ? 32767.0 : 32768.0);
}

void JOYSTICK_Move_X(uint8_t which, int16_t x_val)
{
	if (which > 1)
		return;

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
	if (which > 1)
		return;

	const auto y = position_to_percent(y_val);
	if (stick[which].ypos == y)
		return;
	stick[which].ypos = y;
	stick[which].transformed = false;
}

bool JOYSTICK_IsAccessible(uint8_t which)
{
	assert(which < 2);
	const auto& s = stick[which];
	return s.is_visible_to_dos && s.enabled;
}

bool JOYSTICK_GetButton(uint8_t which, int num)
{
	if ((which < 2) && (num < 2))
		return stick[which].button[num];
	return false;
}

double JOYSTICK_GetMove_X(uint8_t which)
{
	if (which > 1)
		return 0.0;
	if (which == 0) {
		stick[0].transform_input();
		return stick[0].xfinal;
	}
	return stick[1].xpos;
}

double JOYSTICK_GetMove_Y(uint8_t which)
{
	if (which > 1)
		return 0.0;
	if (which == 0) {
		stick[0].transform_input();
		return stick[0].yfinal;
	}
	return stick[1].ypos;
}

void JOYSTICK_ParseConfiguredType()
{
	const auto conf = control->GetSection("joystick");
	const auto section = static_cast<Section_prop *>(conf);
	const auto type = std::string(section->Get_string("joysticktype"));

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

class JOYSTICK final : public Module_base {
private:
	IO_ReadHandleObject ReadHandler = {};
	IO_WriteHandleObject WriteHandler = {};

public:
	JOYSTICK(Section *configuration) : Module_base(configuration)
	{
		JOYSTICK_ParseConfiguredType();

		// Does the user want joysticks to be entirely disabled, both in SDL and DOS?
		if (joytype == JOY_DISABLED)
			return;

		// Get the [joystock] conf section
		const auto section = static_cast<Section_prop *>(configuration);
		assert(section);

		// Get and apply configuration settings
		autofire = section->Get_bool("autofire");
		button_wrapping_enabled = section->Get_bool("buttonwrap");
		stick[0].deadzone = section->Get_int("deadzone");
		swap34 = section->Get_bool("swap34");
		stick[0].mapstate = section->Get_bool("circularinput") ? MovementType::JOYMAP_CIRCLE
		                                                       : MovementType::JOYMAP_SQUARE;

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
			const bool wants_timed = section->Get_bool("timed");
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

static JOYSTICK* test;

void JOYSTICK_Destroy(MAYBE_UNUSED Section *sec)
{
	delete test;
}

void JOYSTICK_Init(Section* sec) {
	test = new JOYSTICK(sec);
	sec->AddDestroyFunction(&JOYSTICK_Destroy,true); 
}
