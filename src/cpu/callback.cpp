/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

/* $Id: callback.cpp,v 1.17 2003-11-09 16:44:07 finsterr Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "cpu.h"
#include "pic.h"

/* CallBack are located at 0xC800:0 
   And they are 16 bytes each and you can define them to behave in certain ways like a
   far return or and IRET
*/


CallBack_Handler CallBack_Handlers[CB_MAX];
char* CallBack_Description[CB_MAX];

static Bitu call_stop,call_idle,call_default;

static Bitu illegal_handler(void) {
	E_Exit("Illegal CallBack Called");
	return 1;
}

Bitu CALLBACK_Allocate(void) {
	for (Bitu i=1;(i<CB_MAX);i++) {
		if (CallBack_Handlers[i]==&illegal_handler) {
			CallBack_Handlers[i]=0;
			return i;	
		}
	}
	E_Exit("CALLBACK:Can't allocate handler.");
	return 0;
}

void CALLBACK_Idle(void) {
/* this makes the cpu execute instructions to handle irq's and then come back */
	Bitu oldIF=GETFLAG(IF);
	SETFLAGBIT(IF,true);
	Bit16u oldcs=SegValue(cs);
	Bit32u oldeip=reg_eip;
	SegSet16(cs,CB_SEG);
	reg_eip=call_idle<<4;
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
	SETFLAGBIT(IF,oldIF);
	if (CPU_CycleLeft<300) CPU_CycleLeft=1;
	else CPU_CycleLeft-=300;
}

static Bitu default_handler(void) {
	LOG(LOG_CPU,LOG_ERROR)("Illegal Unhandled Interrupt Called %X",lastint);
	return CBRET_NONE;
};

static Bitu stop_handler(void) {
	return CBRET_STOP;
};



void CALLBACK_RunRealFar(Bit16u seg,Bit16u off) {
	reg_sp-=4;
	mem_writew(SegPhys(ss)+reg_sp,call_stop<<4);
	mem_writew(SegPhys(ss)+reg_sp+2,CB_SEG);
	Bit32u oldeip=reg_eip;
	Bit16u oldcs=SegValue(cs);
	reg_eip=off;
	SegSet16(cs,seg);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_RunRealInt(Bit8u intnum) {
	Bit32u oldeip=reg_eip;
	Bit16u oldcs=SegValue(cs);
	reg_eip=call_stop<<4;
	SegSet16(cs,CB_SEG);
	Interrupt(intnum);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}



void CALLBACK_SZF(bool val) {
	Bit16u tempf=mem_readw(SegPhys(ss)+reg_sp+4) & 0xFFBF;
	Bit16u newZF=(val==true) << 6;
	mem_writew(SegPhys(ss)+reg_sp+4,(tempf | newZF));
};

void CALLBACK_SCF(bool val) {
	Bit16u tempf=mem_readw(SegPhys(ss)+reg_sp+4) & 0xFFFE;
	Bit16u newCF=(val==true);
	mem_writew(SegPhys(ss)+reg_sp+4,(tempf | newCF));
};

void CALLBACK_SetDescription(Bitu nr, const char* descr)
{
	if (descr) {
		CallBack_Description[nr] = new char[strlen(descr)+1];
		strcpy(CallBack_Description[nr],descr);
	} else
		CallBack_Description[nr] = 0;
};

const char* CALLBACK_GetDescription(Bitu nr)
{
	if (nr>=CB_MAX) return 0;
	return CallBack_Description[nr];
};

bool CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,const char* descr) {
	if (callback>=CB_MAX) return false;
	switch (type) {
	case CB_RETF:
		phys_writeb(CB_BASE+(callback<<4)+0,(Bit8u)0xFE);	//GRP 4
		phys_writeb(CB_BASE+(callback<<4)+1,(Bit8u)0x38);	//Extra Callback instruction
		phys_writew(CB_BASE+(callback<<4)+2,callback);		//The immediate word
		phys_writeb(CB_BASE+(callback<<4)+4,(Bit8u)0xCB);	//A RETF Instruction
		break;
	case CB_IRET:
		phys_writeb(CB_BASE+(callback<<4)+0,(Bit8u)0xFE);	//GRP 4
		phys_writeb(CB_BASE+(callback<<4)+1,(Bit8u)0x38);	//Extra Callback instruction
		phys_writew(CB_BASE+(callback<<4)+2,callback);		//The immediate word
		phys_writeb(CB_BASE+(callback<<4)+4,(Bit8u)0xCF);	//An IRET Instruction
		break;
	case CB_IRET_STI:
		phys_writeb(CB_BASE+(callback<<4)+0,(Bit8u)0xFB);	//STI
		phys_writeb(CB_BASE+(callback<<4)+1,(Bit8u)0xFE);	//GRP 4
		phys_writeb(CB_BASE+(callback<<4)+2,(Bit8u)0x38);	//Extra Callback instruction
		phys_writew(CB_BASE+(callback<<4)+3,callback);		//The immediate word
		phys_writeb(CB_BASE+(callback<<4)+5,(Bit8u)0xCF);	//An IRET Instruction
		break;

	default:
		E_Exit("CALLBACK:Setup:Illegal type %d",type);

	}
	CallBack_Handlers[callback]=handler;
	CALLBACK_SetDescription(callback,descr);
	return true;
}

bool CALLBACK_SetupAt(Bitu callback,CallBack_Handler handler,Bitu type,Bitu linearAddress,const char* descr) {
	if (callback>=CB_MAX) return false;
	switch (type) {
	case CB_RETF:
		mem_writeb(linearAddress+0,(Bit8u)0xFE);	//GRP 4
		mem_writeb(linearAddress+1,(Bit8u)0x38);	//Extra Callback instruction
		mem_writew(linearAddress+2, callback);		//The immediate word
		mem_writeb(linearAddress+4,(Bit8u)0xCB);	//A RETF Instruction
		break;
	case CB_IRET:
		mem_writeb(linearAddress+0,(Bit8u)0xFE);	//GRP 4
		mem_writeb(linearAddress+1,(Bit8u)0x38);	//Extra Callback instruction
		mem_writew(linearAddress+2,callback);		//The immediate word
		mem_writeb(linearAddress+4,(Bit8u)0xCF);	//An IRET Instruction
		break;
	case CB_IRET_STI:
		mem_writeb(linearAddress+0,(Bit8u)0xFB);	//STI
		mem_writeb(linearAddress+1,(Bit8u)0xFE);	//GRP 4
		mem_writeb(linearAddress+2,(Bit8u)0x38);	//Extra Callback instruction
		mem_writew(linearAddress+3, callback);		//The immediate word
		mem_writeb(linearAddress+5,(Bit8u)0xCF);	//An IRET Instruction
		break;
	default:
		E_Exit("CALLBACK:Setup:Illegal type %d",type);
	}
	CallBack_Handlers[callback]=handler;
	CALLBACK_SetDescription(callback,descr);
	return true;
}

void CALLBACK_Init(Section* sec) {
	Bitu i;
	for (i=0;i<CB_MAX;i++) {
		CallBack_Handlers[i]=&illegal_handler;
	}
	/* Setup the Stop Handler */
	call_stop=CALLBACK_Allocate();
	CallBack_Handlers[call_stop]=stop_handler;
	phys_writeb(CB_BASE+(call_stop<<4)+0,0xFE);
	phys_writeb(CB_BASE+(call_stop<<4)+1,0x38);
	phys_writew(CB_BASE+(call_stop<<4)+2,call_stop);
	/* Setup the idle handler */
	call_idle=CALLBACK_Allocate();
	CallBack_Handlers[call_idle]=stop_handler;
	for (i=0;i<=11;i++) phys_writeb(CB_BASE+(call_idle<<4)+i,0x90);
	phys_writeb(CB_BASE+(call_idle<<4)+12,0xFE);
	phys_writeb(CB_BASE+(call_idle<<4)+13,0x38);
	phys_writew(CB_BASE+(call_idle<<4)+14,call_idle);

	/* Setup all Interrupt to point to the default handler */
	call_default=CALLBACK_Allocate();
	CALLBACK_Setup(call_default,&default_handler,CB_IRET);
	/* Only setup default handler for first half of interrupt table */
	for (i=0;i<0x40;i++) {
		real_writed(0,i*4,CALLBACK_RealPointer(call_default));
	}
	real_writed(0,0x67*4,CALLBACK_RealPointer(call_default));
	//real_writed(0,0xf*4,0); some games don't like it
}


