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


#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "mouse.h"
#include "pic.h"
#include "inout.h"


static Bitu call_int33,call_int74;

struct button_event {
	Bit16u type;
	Bit16u buttons;
};

#define QUEUE_SIZE 32
//TODO Maybe use :)
#define MOUSE_IRQ 12
#define POS_X (Bit16s)(mouse.x*mouse.range_x)
#define POS_Y (Bit16s)(mouse.y*mouse.range_y)


static struct {
	Bit16u buttons;
	Bit16u times_pressed[3];
	Bit16u times_released[3];
	Bit16u last_released_x[3];
	Bit16u last_released_y[3];
	Bit16u last_pressed_x[3];
	Bit16u last_pressed_y[3];
	Bit16s shown;
	float add_x,add_y;
	Bit16u min_x,max_x,min_y,max_y;
	float range_x,range_y;
	float mickey_x,mickey_y;
	float x,y;
	button_event event_queue[QUEUE_SIZE];
	Bit32u events;
	Bit16u sub_seg,sub_ofs;
	Bit16u sub_mask;
} mouse;

#define X_MICKEY 500
#define Y_MICKEY 500

#define MOUSE_MOVED 1
#define MOUSE_LEFT_PRESSED 2
#define MOUSE_LEFT_RELEASED 4
#define MOUSE_RIGHT_PRESSED 8
#define MOUSE_RIGHT_RELEASED 16
#define MOUSE_MIDDLE_PRESSED 32
#define MOUSE_MIDDLE_RELEASED 64

inline void Mouse_AddEvent(Bit16u type) {
	if (mouse.events<QUEUE_SIZE) {
		mouse.event_queue[mouse.events].type=type;
		mouse.event_queue[mouse.events].buttons=mouse.buttons;
		mouse.events++;
	}
	PIC_ActivateIRQ(12);
}

void Mouse_CursorMoved(float x,float y) {
	mouse.mickey_x+=x;
	mouse.mickey_y+=y;
	mouse.x+=x;
	if (mouse.x>1) mouse.x=1;
	if (mouse.x<0) mouse.x=0;
	mouse.y+=y;
	if (mouse.y>1) mouse.y=1;
	if (mouse.y<0) mouse.y=0;
	Mouse_AddEvent(MOUSE_MOVED);
}

void Mouse_CursorSet(float x,float y) {

	mouse.x=x;
	mouse.y=y;
}

void Mouse_ButtonPressed(Bit8u button) {
	switch (button) {
	case 0:
		mouse.buttons|=1;
		Mouse_AddEvent(MOUSE_LEFT_PRESSED);
		break;
	case 1:
		mouse.buttons|=2;
		Mouse_AddEvent(MOUSE_RIGHT_PRESSED);
		break;
	case 2:
		mouse.buttons|=4;
		Mouse_AddEvent(MOUSE_MIDDLE_PRESSED);
		break;
	}
	mouse.times_pressed[button]++;
	mouse.last_pressed_x[button]=POS_X;
	mouse.last_pressed_y[button]=POS_Y;
}

void Mouse_ButtonReleased(Bit8u button) {
	switch (button) {
	case 0:
		mouse.buttons&=~1;
		Mouse_AddEvent(MOUSE_LEFT_RELEASED);
		break;
	case 1:
		mouse.buttons&=~2;
		Mouse_AddEvent(MOUSE_RIGHT_RELEASED);
		break;
	case 2:
		mouse.buttons&=~4;
		Mouse_AddEvent(MOUSE_MIDDLE_RELEASED);
		break;
	}
	mouse.times_released[button]++;	
	mouse.last_released_x[button]=POS_X;
	mouse.last_released_y[button]=POS_Y;
}


static void  mouse_reset(void) {
	real_writed(0,(0x33<<2),CALLBACK_RealPointer(call_int33));
	real_writed(0,(0x74<<2),CALLBACK_RealPointer(call_int74));
	mouse.shown=-1;
	mouse.min_x=0;
	mouse.max_x=639;
	mouse.min_y=0;
	mouse.max_y=199;
	mouse.range_x=639;
	mouse.range_y=199;
	mouse.x=320;
	mouse.y=100;
	mouse.events=0;
	mouse.mickey_x=0;
	mouse.mickey_y=0;
	mouse.sub_mask=0;
	mouse.sub_seg=0;
	mouse.sub_ofs=0;
};


static Bitu INT33_Handler(void) {
	switch (reg_ax) {
	case 0x00:	/* Reset Driver and Read Status */
		reg_ax=0xffff;
		reg_bx=0;
		mouse_reset();
		break;
	case 0x01:	/* Show Mouse */
		mouse.shown++;
		if (mouse.shown>0) mouse.shown=0;
		break;
	case 0x02:	/* Hide Mouse */
		mouse.shown--;
		break;
	case 0x03:	/* Return position and Button Status */
		reg_bx=mouse.buttons;
		reg_cx=POS_X;
		reg_dx=POS_Y;
		break;
	case 0x04:	/* Position Mouse */
		mouse.x=((float)reg_cx)/mouse.range_x;
		mouse.y=((float)reg_dx)/mouse.range_y;
		break;
	case 0x05:	/* Return Button Press Data */
		{
			Bit16u but=reg_bx;
			reg_ax=mouse.buttons;
			reg_cx=mouse.last_pressed_x[but];
			mouse.last_pressed_x[but]=0;
			reg_dx=mouse.last_pressed_y[but];
			mouse.last_pressed_y[but]=0;
			reg_bx=mouse.times_pressed[but];
			mouse.times_pressed[but]=0;
			break;
		}
	case 0x07:	/* Define horizontal cursor range */
		mouse.min_x=reg_cx;
		mouse.max_x=reg_dx;
		mouse.range_x=mouse.max_x-mouse.min_x;
//TODO Check for range start 0
		break;
	case 0x08:	/* Define vertical cursor range */
		mouse.min_y=reg_cx;
		mouse.max_y=reg_dx;
		mouse.range_y=mouse.max_y-mouse.min_y;
		break;
	case 0x09:	/* Define GFX Cursor */
		LOG_WARN("MOUSE:Define gfx cursor not supported");
		break;
	case 0x0a:	/* Define Text Cursor */
		/* Don't see much need for supporting this */
		break;
	case 0x0c:	/* Define interrupt subroutine parameters */
		mouse.sub_mask=reg_cx;
		mouse.sub_seg=Segs[es].value;
		mouse.sub_ofs=reg_dx;
		break;
	case 0x0f:	/* Define mickey/pixel rate */
		//TODO Maybe dunno for sure might be possible */
		break;
	case 0x0B:	/* Read Motion Data */
		reg_cx=(Bit16s)(mouse.mickey_x*X_MICKEY);
		reg_dx=(Bit16s)(mouse.mickey_y*Y_MICKEY);
		mouse.mickey_x=0;
		mouse.mickey_y=0;
		break;
	case 0x1c:	/* Set interrupt rate */
		/* Can't really set a rate this is host determined */
		break;
	case 0x21:	/* Software Reset */
		//TODO reset internal mouse software variables likes mickeys
		reg_ax=0xffff;
		reg_bx=2;
		break;
	case 0x24:	/* Get Software version and mouse type */
		reg_bx=0x805;	//Version 8.05 woohoo 
		reg_ch=0xff;	/* Unkown type */
		reg_cl=0;		/* Hmm ps2 irq dunno */
		break;
	default:
		LOG_ERROR("Mouse Function %2X",reg_ax);
	};
	return CBRET_NONE;
};

static Bitu INT74_Handler(void) {
	if (mouse.events>0) {
		mouse.events--;
		/* Check for an active Interrupt Handler that will get called */
		if (mouse.sub_mask & mouse.event_queue[mouse.events].type) {
			/* Save lot's of registers */
			Bit16u oldax,oldbx,oldcx,olddx,oldsi,olddi;
			oldax=reg_ax;oldbx=reg_bx;oldcx=reg_cx;olddx=reg_dx;oldsi=reg_si;olddi=reg_di;
			reg_ax=mouse.event_queue[mouse.events].type;
			reg_bx=mouse.event_queue[mouse.events].buttons;
			reg_cx=POS_X;
			reg_dx=POS_Y;
			reg_si=(Bit16s)(mouse.mickey_x*X_MICKEY);
			reg_di=(Bit16s)(mouse.mickey_y*Y_MICKEY);
			if (mouse.event_queue[mouse.events].type==MOUSE_MOVED) {
				mouse.mickey_x=0;
				mouse.mickey_y=0;
			}
			CALLBACK_RunRealFar(mouse.sub_seg,mouse.sub_ofs);
			reg_ax=oldax;reg_bx=oldbx;reg_cx=oldcx;reg_dx=olddx;reg_si=oldsi;reg_di=olddi;
		}
	}
	IO_Write(0xa0,0x20);
	/* Check for more Events if so reactivate IRQ */

	if (mouse.events) {
		PIC_ActivateIRQ(12);
	}

	return CBRET_NONE;
};


void MOUSE_Init(void) {
	call_int33=CALLBACK_Allocate();
	CALLBACK_Setup(call_int33,&INT33_Handler,CB_IRET);
	real_writed(0,(0x33<<2),CALLBACK_RealPointer(call_int33));

	call_int74=CALLBACK_Allocate();
	CALLBACK_Setup(call_int74,&INT74_Handler,CB_IRET);
	real_writed(0,(0x74<<2),CALLBACK_RealPointer(call_int74));

	memset((void *)&mouse,0,sizeof(mouse));
	mouse_reset();
};

