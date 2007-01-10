/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: joystick.cpp,v 1.16 2007-01-10 15:01:15 c2woody Exp $ */

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "setup.h"
#include "joystick.h"
#include "pic.h"
#include "support.h"

#define RANGE 64
#define TIMEOUT 10

struct JoyStick {
	bool enabled;
	float xpos,ypos;
	Bitu xcount,ycount;
	bool button[2];
};

JoystickType joytype;
static JoyStick stick[2];

static Bit32u last_write = 0;
static bool write_active = false;

static Bitu read_p201(Bitu port,Bitu iolen) {
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
	Bit8u ret=0xff;
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

static void write_p201(Bitu port,Bitu val,Bitu iolen) {
	/* Store writetime index */
	write_active = true;
	last_write = PIC_Ticks;
	if (stick[0].enabled) {
		stick[0].xcount=(Bitu)((stick[0].xpos*RANGE)+RANGE);
		stick[0].ycount=(Bitu)((stick[0].ypos*RANGE)+RANGE);
	}
	if (stick[1].enabled) {
		stick[1].xcount=(Bitu)((stick[1].xpos*RANGE)+RANGE);
		stick[1].ycount=(Bitu)((stick[1].ypos*RANGE)+RANGE);
	}

}

void JOYSTICK_Enable(Bitu which,bool enabled) {
	if (which<2) stick[which].enabled=enabled;
}

void JOYSTICK_Button(Bitu which,Bitu num,bool pressed) {
	if ((which<2) && (num<2)) stick[which].button[num]=pressed;
}

void JOYSTICK_Move_X(Bitu which,float x) {
	if (which<2) {
		stick[which].xpos=x;
	}
}

void JOYSTICK_Move_Y(Bitu which,float y) {
	if (which<2) {
		stick[which].ypos=y;
	}
}

bool JOYSTICK_IsEnabled(Bitu which)
{
	if (which<2) return stick[which].enabled;
	return false;
};

bool JOYSTICK_GetButton(Bitu which, Bitu num)
{
	if ((which<2) && (num<2)) return stick[which].button[num];
	return false;
}

float JOYSTICK_GetMove_X(Bitu which) 
{
	if (which<2) return stick[which].xpos;
	return 0.0f;
}

float JOYSTICK_GetMove_Y(Bitu which)
{	
	if (which<2) return stick[which].ypos;
	return 0.0f;
};

class JOYSTICK:public Module_base{
private:
	IO_ReadHandleObject ReadHandler;
	IO_WriteHandleObject WriteHandler;
public:
	JOYSTICK(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);
		const char * type=section->Get_string("joysticktype");
		if (!strcasecmp(type,"none")) joytype=JOY_NONE;
		else if (!strcasecmp(type,"false")) joytype=JOY_NONE;
		else if (!strcasecmp(type,"auto")) joytype=JOY_AUTO;
		else if (!strcasecmp(type,"2axis")) joytype=JOY_2AXIS;
		else if (!strcasecmp(type,"4axis")) joytype=JOY_4AXIS;
		else if (!strcasecmp(type,"fcs")) joytype=JOY_FCS;
		else if (!strcasecmp(type,"ch")) joytype=JOY_CH;
		else joytype=JOY_AUTO;
		ReadHandler.Install(0x201,read_p201,IO_MB);
		WriteHandler.Install(0x201,write_p201,IO_MB);
		stick[0].enabled=false;
		stick[1].enabled=false;	
	}
};
static JOYSTICK* test;

void JOYSTICK_Destroy(Section* sec) {
	delete test;
}

void JOYSTICK_Init(Section* sec) {
	test = new JOYSTICK(sec);
	sec->AddDestroyFunction(&JOYSTICK_Destroy,true); 
}
