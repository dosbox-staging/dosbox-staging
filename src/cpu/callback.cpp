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

#include <stdlib.h>
#include <stdio.h>

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
static Bitu call_stop,call_idle,call_default;

static Bitu illegal_handler(void) {
	E_Exit("Illegal CallBack Called");
	return 1;
}

Bitu CALLBACK_Allocate(void) {
	for (Bitu i=0;(i<CB_MAX);i++) {
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
	bool oldintf=flags.intf;
	flags.intf=true;
	Bit16u oldcs=SegValue(cs);
	Bit32u oldeip=reg_eip;
	SegSet16(cs,CB_SEG);
	reg_eip=call_idle<<4;
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
	flags.intf=oldintf;
	if (CPU_CycleLeft<300) CPU_CycleLeft=1;
	else CPU_CycleLeft-=300;
}

static Bitu default_handler(void) {
	LOG_WARN("Illegal Unhandled Interrupt Called %d",lastint);
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



bool CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type) {
	if (callback>=CB_MAX) return false;
	switch (type) {
	case CB_RETF:
		real_writeb((Bit16u)CB_SEG,(callback<<4)+0,(Bit8u)0xFE);	//GRP 4
		real_writeb((Bit16u)CB_SEG,(callback<<4)+1,(Bit8u)0x38);	//Extra Callback instruction
		real_writew((Bit16u)CB_SEG,(callback<<4)+2,callback);		//The immediate word
		real_writeb((Bit16u)CB_SEG,(callback<<4)+4,(Bit8u)0xCB);	//A RETF Instruction
		break;
	case CB_IRET:
		real_writeb((Bit16u)CB_SEG,(callback<<4)+0,(Bit8u)0xFE);	//GRP 4
		real_writeb((Bit16u)CB_SEG,(callback<<4)+1,(Bit8u)0x38);	//Extra Callback instruction
		real_writew((Bit16u)CB_SEG,(callback<<4)+2,callback);		//The immediate word
		real_writeb((Bit16u)CB_SEG,(callback<<4)+4,(Bit8u)0xCF);	//An IRET Instruction
		break;
	case CB_IRET_STI:
		real_writeb((Bit16u)CB_SEG,(callback<<4)+0,(Bit8u)0xFB);	//STI
		real_writeb((Bit16u)CB_SEG,(callback<<4)+1,(Bit8u)0xFE);	//GRP 4
		real_writeb((Bit16u)CB_SEG,(callback<<4)+2,(Bit8u)0x38);	//Extra Callback instruction
		real_writew((Bit16u)CB_SEG,(callback<<4)+3,callback);		//The immediate word
		real_writeb((Bit16u)CB_SEG,(callback<<4)+5,(Bit8u)0xCF);	//An IRET Instruction
		break;

	default:
		E_Exit("CALLBACK:Setup:Illegal type %d",type);

	}
	CallBack_Handlers[callback]=handler;
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
	real_writeb((Bit16u)CB_SEG,(call_stop<<4)+0,0xFE);
	real_writeb((Bit16u)CB_SEG,(call_stop<<4)+1,0x38);
	real_writew((Bit16u)CB_SEG,(call_stop<<4)+2,call_stop);
	/* Setup the idle handler */
	call_idle=CALLBACK_Allocate();
	CallBack_Handlers[call_idle]=stop_handler;
	for (i=0;i<=11;i++) real_writeb((Bit16u)CB_SEG,(call_idle<<4)+i,0x90);
	real_writeb((Bit16u)CB_SEG,(call_idle<<4)+12,0xFE);
	real_writeb((Bit16u)CB_SEG,(call_idle<<4)+13,0x38);
	real_writew((Bit16u)CB_SEG,(call_idle<<4)+14,call_idle);

#if C_DEBUG	
	/* Setup all Interrupt to point to the default handler */
	call_default=CALLBACK_Allocate();
	CALLBACK_Setup(call_default,&default_handler,CB_IRET);
	for (i=0;i<256;i++) {
		real_writed(0,i*4,CALLBACK_RealPointer(call_default));
	}
#endif
}


