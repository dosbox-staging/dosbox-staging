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
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"


static Bitu call_int2f,call_int2a;
struct MultiplexBlock {
	MultiplexHandler * handler;
	MultiplexBlock * next;
};

static MultiplexBlock * first_multiplex;

void DOS_AddMultiplexHandler(MultiplexHandler * handler) {
	MultiplexBlock * new_multiplex=new(MultiplexBlock);
	new_multiplex->next=first_multiplex;
	new_multiplex->handler=handler;
	first_multiplex=new_multiplex;
}

static Bitu INT2F_Handler(void) {
	MultiplexBlock * loop_multiplex=first_multiplex;
	while (loop_multiplex) {
		if ((*loop_multiplex->handler)()) return CBRET_NONE;
		loop_multiplex=loop_multiplex->next;
	}
	LOG_WARN("DOS:Multiplex Unhandled call %4X",reg_ax);	
	return CBRET_NONE;
};


static Bitu INT2A_Handler(void) {

	return CBRET_NONE;
};


void DOS_SetupMisc(void) {
	/* Setup the dos multiplex interrupt */
	first_multiplex=0;
	call_int2f=CALLBACK_Allocate();
	CALLBACK_Setup(call_int2f,&INT2F_Handler,CB_IRET);
	RealSetVec(0x2f,CALLBACK_RealPointer(call_int2f));
	/* Setup the dos network interrupt */
	call_int2a=CALLBACK_Allocate();
	CALLBACK_Setup(call_int2a,&INT2A_Handler,CB_IRET);
	RealSetVec(0x2A,CALLBACK_RealPointer(call_int2a));
};

