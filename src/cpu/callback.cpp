/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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

/* $Id: callback.cpp,v 1.31 2006-02-12 23:28:21 harekiet Exp $ */

#include <stdlib.h>
#include <string.h>

#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "cpu.h"

/* CallBack are located at 0xC800:0 
   And they are 16 bytes each and you can define them to behave in certain ways like a
   far return or and IRET
*/

CallBack_Handler CallBack_Handlers[CB_MAX];
char* CallBack_Description[CB_MAX];

static Bitu call_stop,call_idle,call_default;
Bitu call_priv_io;

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

void CALLBACK_DeAllocate(Bitu in) {
	CallBack_Handlers[in]=&illegal_handler;
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
	if (!CPU_CycleAuto && CPU_Cycles>0) 
		CPU_Cycles=0;
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
	reg_eip=(CB_MAX*16)+(intnum*6);
	SegSet16(cs,CB_SEG);
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

void CALLBACK_SetDescription(Bitu nr, const char* descr) {
	if (descr) {
		CallBack_Description[nr] = new char[strlen(descr)+1];
		strcpy(CallBack_Description[nr],descr);
	} else
		CallBack_Description[nr] = 0;
};

const char* CALLBACK_GetDescription(Bitu nr) {
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

void CALLBACK_RemoveSetup(Bitu callback) {
	for (Bitu i = 0;i < 16;i++) {
		phys_writeb(CB_BASE+(callback<<4)+i ,(Bit8u) 0x00);
	}
}

Bitu CALLBACK_SetupExtra(Bitu callback, Bitu type, PhysPt physAddress) {
	if (callback>=CB_MAX) 
		return 0;
	switch (type) {
	case CB_RETN:
		phys_writeb(physAddress+0,(Bit8u)0xFE);	//GRP 4
		phys_writeb(physAddress+1,(Bit8u)0x38);	//Extra Callback instruction
		phys_writew(physAddress+2, callback);	//The immediate word
		phys_writeb(physAddress+4,(Bit8u)0xC3);	//A RETN Instruction
		return 5;
	case CB_RETF:
		phys_writeb(physAddress+0,(Bit8u)0xFE);	//GRP 4
		phys_writeb(physAddress+1,(Bit8u)0x38);	//Extra Callback instruction
		phys_writew(physAddress+2, callback);	//The immediate word
		phys_writeb(physAddress+4,(Bit8u)0xCB);	//A RETF Instruction
		return 5;
	case CB_IRET:
		phys_writeb(physAddress+0,(Bit8u)0xFE);	//GRP 4
		phys_writeb(physAddress+1,(Bit8u)0x38);	//Extra Callback instruction
		phys_writew(physAddress+2,callback);	//The immediate word
		phys_writeb(physAddress+4,(Bit8u)0xCF);	//An IRET Instruction
		return 5;
	case CB_IRET_STI:
		phys_writeb(physAddress+0,(Bit8u)0xFB);	//STI
		phys_writeb(physAddress+1,(Bit8u)0xFE);	//GRP 4
		phys_writeb(physAddress+2,(Bit8u)0x38);	//Extra Callback instruction
		phys_writew(physAddress+3, callback);	//The immediate word
		phys_writeb(physAddress+5,(Bit8u)0xCF);	//An IRET Instruction
		return 6;
	default:
		E_Exit("CALLBACK:Setup:Illegal type %d",type);
	}
	return 0;
}

CALLBACK_HandlerObject::~CALLBACK_HandlerObject(){
	if(!installed) return;
	if(m_type == CALLBACK_HandlerObject::SETUP) {
		if(vectorhandler.installed){
			//See if we are the current handler. if so restore the old one
			if(RealGetVec(vectorhandler.interrupt) == Get_RealPointer()) {
				RealSetVec(vectorhandler.interrupt,vectorhandler.old_vector);
			} else 
				LOG(LOG_MISC,LOG_WARN)("Interrupt vector changed on %X %s",vectorhandler.interrupt,CALLBACK_GetDescription(m_callback));
		}
		CALLBACK_RemoveSetup(m_callback);
	} else if(m_type == CALLBACK_HandlerObject::SETUPAT){
		E_Exit("Callback:SETUP at not handled yet.");
	} else if(m_type == CALLBACK_HandlerObject::NONE){
		//Do nothing. Merely DeAllocate the callback
	} else E_Exit("what kind of callback is this!");
	if(CallBack_Description[m_callback]) delete [] CallBack_Description[m_callback];
	CallBack_Description[m_callback] = 0;
	CALLBACK_DeAllocate(m_callback);
}

void CALLBACK_HandlerObject::Install(CallBack_Handler handler,Bitu type,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_callback=CALLBACK_Allocate();
		CALLBACK_Setup(m_callback,handler,type,description);
	} else E_Exit("Allready installed");
}
void CALLBACK_HandlerObject::Allocate(CallBack_Handler handler,const char* description) {
	if(!installed) {
		installed=true;
		m_type=NONE;
		m_callback=CALLBACK_Allocate();
		CALLBACK_SetDescription(m_callback,description);
		CallBack_Handlers[m_callback]=handler;
	} else E_Exit("Allready installed");
}

void CALLBACK_HandlerObject::Set_RealVec(Bit8u vec){
	if(!vectorhandler.installed) {
		vectorhandler.installed=true;
		vectorhandler.interrupt=vec;
		RealSetVec(vec,Get_RealPointer(),vectorhandler.old_vector);
	} else E_Exit ("double usage of vector handler");
}

void CALLBACK_Init(Section* sec) {
	Bitu i;
	for (i=0;i<CB_MAX;i++) {
		CallBack_Handlers[i]=&illegal_handler;
	}
	/* Setup the Stop Handler */
	call_stop=CALLBACK_Allocate();
	CallBack_Handlers[call_stop]=stop_handler;
	CALLBACK_SetDescription(call_stop,"stop");
	phys_writeb(CB_BASE+(call_stop<<4)+0,0xFE);
	phys_writeb(CB_BASE+(call_stop<<4)+1,0x38);
	phys_writew(CB_BASE+(call_stop<<4)+2,call_stop);
	/* Setup the idle handler */
	call_idle=CALLBACK_Allocate();
	CallBack_Handlers[call_idle]=stop_handler;
	CALLBACK_SetDescription(call_idle,"idle");
	for (i=0;i<=11;i++) phys_writeb(CB_BASE+(call_idle<<4)+i,0x90);
	phys_writeb(CB_BASE+(call_idle<<4)+12,0xFE);
	phys_writeb(CB_BASE+(call_idle<<4)+13,0x38);
	phys_writew(CB_BASE+(call_idle<<4)+14,call_idle);

	/* Setup all Interrupt to point to the default handler */
	call_default=CALLBACK_Allocate();
	CALLBACK_Setup(call_default,&default_handler,CB_IRET,"default");
   
	/* Only setup default handler for first half of interrupt table */
	for (i=0;i<0x40;i++) {
		real_writed(0,i*4,CALLBACK_RealPointer(call_default));
	}
	/* Setup block of 0xCD 0xxx instructions */
	PhysPt rint_base=CB_BASE+CB_MAX*16;
	for (i=0;i<=0xff;i++) {
		phys_writeb(rint_base,0xCD);
		phys_writeb(rint_base+1,i);
		phys_writeb(rint_base+2,0xFE);
		phys_writeb(rint_base+3,0x38);
		phys_writew(rint_base+4,call_stop);
		rint_base+=6;

	}
	real_writed(0,0x67*4,CALLBACK_RealPointer(call_default));
	real_writed(0,0x5c*4,CALLBACK_RealPointer(call_default)); //Network stuff
	//real_writed(0,0xf*4,0); some games don't like it

	call_priv_io=CALLBACK_Allocate();

	phys_writeb(CB_BASE+(call_priv_io<<4)+0x00,(Bit8u)0xec);	// in al, dx
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x01,(Bit8u)0xcb);	// retf
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x02,(Bit8u)0xed);	// in ax, dx
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x03,(Bit8u)0xcb);	// retf
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x04,(Bit8u)0x66);	// in eax, dx
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x05,(Bit8u)0xed);
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x06,(Bit8u)0xcb);	// retf

	phys_writeb(CB_BASE+(call_priv_io<<4)+0x08,(Bit8u)0xee);	// out dx, al
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x09,(Bit8u)0xcb);	// retf
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x0a,(Bit8u)0xef);	// out dx, ax
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x0b,(Bit8u)0xcb);	// retf
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x0c,(Bit8u)0x66);	// out dx, eax
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x0d,(Bit8u)0xef);
	phys_writeb(CB_BASE+(call_priv_io<<4)+0x0e,(Bit8u)0xcb);	// retf
}
