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

#define RANGE 64
#define TIMEOUT 10

struct JoyStick {
	enum {JOYMAP_SQUARE,JOYMAP_CIRCLE,JOYMAP_INBETWEEN} mapstate;
	bool enabled;
	float xpos;
	float ypos; // position as set by SDL.
	double xtick;
	double ytick;
	Bitu xcount, ycount;
	bool button[2];
	int deadzone; //Deadzone (value between 0 and 100) interpreted as percentage.
	bool transformed; //Whether xpos,ypos have been converted to xfinal and yfinal. Cleared when new xpos orypos have been set
	float xfinal, yfinal; //position returned to the game for stick 0. 

	void clip() {
		if (xfinal > 1.0f)
			xfinal = 1.0f;
		else if (xfinal < -1.0f)
			xfinal = -1.0f;
		if (yfinal > 1.0f)
			yfinal = 1.0f;
		else if (yfinal < -1.0f)
			yfinal = -1.0f;
	}

	void fake_digital() {
		if (xpos > 0.5f) xfinal = 1.0f;
		else if (xpos < -0.5f) xfinal = -1.0f;
		else xfinal = 0.0f;
		if (ypos > 0.5f) yfinal = 1.0f;
		else if (ypos < -0.5f) yfinal = -1.0f;
		else yfinal = 0.0f;
	}

	void transform_circular(){
		const float r = sqrtf(xpos * xpos + ypos * ypos);
		if (fabs(r) < FLT_EPSILON) {
			xfinal = xpos;
			yfinal = ypos;
			return;
		}
		float deadzone_f = static_cast<float>(deadzone) / 100.0f;
		float s = 1.0f - deadzone_f;
		if (r < deadzone_f) {
			xfinal = yfinal = 0.0f;
			return;
		}

		float deadzonescale = (r - deadzone_f) / s; //r if deadzone=0;
		float xa = fabsf(xpos);
		float ya = fabsf(ypos);
		float maxpos = (ya>xa?ya:xa);
		xfinal = xpos * deadzonescale/maxpos;
		yfinal = ypos * deadzonescale/maxpos;
	}

	void transform_square() {
		float deadzone_f = static_cast<float>(deadzone) / 100.0f;
		float s = 1.0f - deadzone_f;

		if (xpos > deadzone_f) {
			xfinal = (xpos - deadzone_f)/ s;
		} else if ( xpos < -deadzone_f) {
			xfinal = (xpos + deadzone_f) / s;
		} else xfinal = 0.0f;
		if (ypos > deadzone_f) {
			yfinal = (ypos - deadzone_f)/ s;
		} else if ( ypos < - deadzone_f) {
			yfinal = (ypos + deadzone_f) / s;
		} else yfinal = 0.0f;
	}

#if SUPPORT_MAP_AUTO
	void transform_inbetween(){
		//First transform to a circle and crop the values to -1.0 -> 1.0
		//then keep on doing this in future calls until it is safe to switch square mapping
		// safe = 0.95 as ratio  for both axis, or in deadzone
		transform_circular();
		clip();


		float xrate = xpos / xfinal;
		float yrate = ypos / yfinal;
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

static Bitu last_write = 0;
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
	if (stick[0].enabled) {
		stick[0].transform_input();
		stick[0].xcount=(Bitu)((stick[0].xfinal*RANGE)+RANGE);
		stick[0].ycount=(Bitu)((stick[0].yfinal*RANGE)+RANGE);
	}
	if (stick[1].enabled) {
		stick[1].xcount=(Bitu)(((swap34? stick[1].ypos : stick[1].xpos)*RANGE)+RANGE);
		stick[1].ycount=(Bitu)(((swap34? stick[1].xpos : stick[1].ypos)*RANGE)+RANGE);
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
		const auto resistance = static_cast<double>(position) + 1.0;

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

void JOYSTICK_Enable(Bitu which,bool enabled) {
	if (which<2) stick[which].enabled = enabled;
}

void JOYSTICK_Button(Bitu which,Bitu num,bool pressed) {
	if ((which<2) && (num<2)) stick[which].button[num] = pressed;
}

void JOYSTICK_Move_X(Bitu which,float x) {
	if (which > 1) return;
	if (stick[which].xpos == x) return;
	stick[which].xpos = x;
	stick[which].transformed = false;
//	if( which == 0 || joytype != JOY_FCS)  
//		stick[which].applied_conversion; //todo 
}

void JOYSTICK_Move_Y(Bitu which,float y) {
	if (which > 1) return;
	if (stick[which].ypos == y) return;
	stick[which].ypos = y;
	stick[which].transformed = false;
}

bool JOYSTICK_IsEnabled(Bitu which) {
	if (which<2) return stick[which].enabled;
	return false;
}

bool JOYSTICK_GetButton(Bitu which, Bitu num) {
	if ((which<2) && (num<2)) return stick[which].button[num];
	return false;
}

float JOYSTICK_GetMove_X(Bitu which) {
	if (which > 1) return 0.0f;
	if (which == 0) { stick[0].transform_input(); return stick[0].xfinal;}
	return stick[1].xpos;
}

float JOYSTICK_GetMove_Y(Bitu which) {
	if (which > 1) return 0.0f;
	if (which == 0) { stick[0].transform_input(); return stick[0].yfinal;}
	return stick[1].ypos;
}

void JOYSTICK_ParseConfiguredType()
{
	const auto conf = control->GetSection("joystick");
	const auto section = static_cast<Section_prop *>(conf);
	const char *type = section->Get_string("joysticktype");

	if (!strcasecmp(type, "disabled"))
		joytype = JOY_DISABLED;
	else if (!strcasecmp(type, "none"))
		joytype = JOY_NONE;
	else if (!strcasecmp(type, "auto"))
		joytype = JOY_AUTO;
	else if (!strcasecmp(type, "2axis"))
		joytype = JOY_2AXIS;
	else if (!strcasecmp(type, "4axis"))
		joytype = JOY_4AXIS;
	else if (!strcasecmp(type, "4axis_2"))
		joytype = JOY_4AXIS_2;
	else if (!strcasecmp(type, "fcs"))
		joytype = JOY_FCS;
	else if (!strcasecmp(type, "ch"))
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
		stick[0].mapstate = section->Get_bool("circularinput")
		                            ? JoyStick::JOYMAP_CIRCLE
		                            : JoyStick::JOYMAP_SQUARE;

		// Set initial time and position states
		const auto ticks = PIC_FullIndex();
		stick[0].xtick = ticks;
		stick[0].ytick = ticks;
		stick[1].xtick = ticks;
		stick[1].ytick = ticks;
		stick[0].xpos = 0.0f;
		stick[0].ypos = 0.0f;
		stick[1].xpos = 0.0f;
		stick[1].ypos = 0.0f;
		stick[0].transformed = false;

		// Does the user want joysticks to be available for mapping, but hidden in DOS?
		if (joytype == JOY_NONE)
			return;

		// Setup the joystick IO port handlers, which lets DOS games
		// detect and use them
		const bool wants_timed = section->Get_bool("timed");
		ReadHandler.Install(0x201, wants_timed ? read_p201_timed : read_p201, io_width_t::byte);
		WriteHandler.Install(0x201, wants_timed ? write_p201_timed : write_p201, io_width_t::byte);
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
