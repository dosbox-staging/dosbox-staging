/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "inout.h"

#define RANGE 64

struct JoyStick {
	bool enabled;
	float xpos,ypos;
	Bitu xcount,ycount;
	bool button[2];
};


static JoyStick stick[2];


static Bit8u read_p201(Bit32u port) {
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
		if (stick[0].button[0]) ret&=16;
		if (stick[0].button[1]) ret&=32;
	}
	if (stick[1].enabled) {
		if (stick[1].xcount) stick[0].xcount--; else ret&=~4;
		if (stick[1].ycount) stick[0].ycount--; else ret&=~8;
		if (stick[1].button[0]) ret&=64;
		if (stick[1].button[1]) ret&=128;
	}
	return ret;
}

static void write_p201(Bit32u port,Bit8u val) {
	if (stick[0].enabled) {
		stick[0].xcount=(Bitu)(stick[0].xpos*RANGE+RANGE*2);
		stick[0].ycount=(Bitu)(stick[0].ypos*RANGE+RANGE*2);
	}
	if (stick[1].enabled) {
		stick[1].xcount=(Bitu)(stick[1].xpos*RANGE+RANGE*2);
		stick[1].ycount=(Bitu)(stick[1].ypos*RANGE+RANGE*2);
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


void JOYSTICK_Init(void) {
	IO_RegisterReadHandler(0x201,read_p201,"JOYSTICK");
	IO_RegisterWriteHandler(0x201,write_p201,"JOYSTICK");
	stick[0].enabled=false;
	stick[1].enabled=false;
}


