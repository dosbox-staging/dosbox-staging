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
#include <time.h>
#include "dosbox.h"
#include "bios.h"
#include "regs.h"
#include "callback.h"
#include "inout.h"
#include "mem.h"
#include "pic.h"
#include "joystick.h"

static Bitu call_int1a,call_int11,call_int8,call_int17,call_int12,call_int15,call_int1c;
static Bitu call_int1,call_int70;

static Bitu INT70_Handler(void) {
	/* Acknowledge irq with cmos */
	IO_Write(0x70,0xc);
	IO_Read(0x71);
	if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
		Bits count=mem_readd(BIOS_WAIT_FLAG_COUNT);
		if (count>997) {
			mem_writed(BIOS_WAIT_FLAG_COUNT,count-997);
		} else {
			mem_writed(BIOS_WAIT_FLAG_COUNT,0);
			PhysPt where=Real2Phys(mem_readd(BIOS_WAIT_FLAG_POINTER));
			mem_writeb(where,mem_readb(where)|0x80);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,0);
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)&~0x40);
		}
	} 
	/* Signal EOI to both pics */
	IO_Write(0xa0,0x20);
	IO_Write(0x20,0x20);
	return 0;
}

static Bitu INT1A_Handler(void) {
	switch (reg_ah) {
	case 0x00:	/* Get System time */
		{
			Bit32u ticks=mem_readd(BIOS_TIMER);
			reg_al=0;		/* Midnight never passes :) */
			reg_cx=(Bit16u)(ticks >> 16);
			reg_dx=(Bit16u)(ticks & 0xffff);
			break;
		}
	case 0x01:	/* Set System time */
		mem_writed(BIOS_TIMER,(reg_cx<<16)|reg_dx);
		break;
	case 0x02:	/* GET REAL-TIME CLOCK TIME (AT,XT286,PS) */
		IO_Write(0x70,0x04);		//Hours
		reg_ch=IO_Read(0x71);
		IO_Write(0x70,0x02);		//Minutes
		reg_cl=IO_Read(0x71);
		IO_Write(0x70,0x00);		//Seconds
		reg_dh=IO_Read(0x71);
		reg_dl=0;					//Daylight saving disabled
		CALLBACK_SCF(false);
		break;
	case 0x04:	/* GET REAL-TIME ClOCK DATA  (AT,XT286,PS) */
		reg_dx=reg_cx=0;
		CALLBACK_SCF(false);
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:04:Faked RTC get date call");
		break;
	case 0x80:	/* Pcjr Setup Sound Multiplexer */
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:80:Setup tandy sound multiplexer to %d",reg_al);
		break;
	case 0x81:	/* Tandy sound system checks */
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:81:Tandy DAC Check failing");
		break;
/*
	INT 1A - Tandy 2500, Tandy 1000L series - DIGITAL SOUND - INSTALLATION CHECK
	AX = 8100h
	Return: AL > 80h if supported
	AX = 00C4h if supported (1000SL/TL)
	    CF set if sound chip is busy
	    CF clear  if sound chip is free
	Note:	the value of CF is not definitive; call this function until CF is
			clear on return, then call AH=84h"Tandy"
		*/
	case 0xb1:		/* PCI Bios Calls */
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:PCI bios call %2X",reg_al);
		CALLBACK_SCF(true);
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:Undefined call %2X",reg_ah);
	}
	return CBRET_NONE;
}	

static Bitu INT11_Handler(void) {
	reg_ax=mem_readw(BIOS_CONFIGURATION);
	return CBRET_NONE;
}

static Bitu INT8_Handler(void) {
	/* Increase the bios tick counter */
	mem_writed(BIOS_TIMER,mem_readd(BIOS_TIMER)+1);
	/* decrease floppy motor timer */
	Bit8u val = mem_readb(BIOS_DISK_MOTOR_TIMEOUT);
	if (val>0) mem_writeb(BIOS_DISK_MOTOR_TIMEOUT,val-1);
	/* and running drive */
	mem_writeb(BIOS_DRIVE_RUNNING,mem_readb(BIOS_DRIVE_RUNNING) & 0xF0);
	// Save ds,dx,ax
	Bit16u oldds = SegValue(ds);
	Bit16u olddx = reg_dx;
	Bit16u oldax = reg_ax;
	// run int 1c	
	CALLBACK_RunRealInt(0x1c);
	IO_Write(0x20,0x20);
	// restore old values
	SegSet16(ds,oldds);
	reg_dx = olddx;
	reg_ax = oldax;
	return CBRET_NONE;
};

static Bitu INT1C_Handler(void) {
	return CBRET_NONE;
};

static Bitu INT12_Handler(void) {
	reg_ax=mem_readw(BIOS_MEMORY_SIZE);
	return CBRET_NONE;
};

static Bitu INT17_Handler(void) {
	LOG(LOG_BIOS,LOG_NORMAL)("INT17:Function %X",reg_ah);
	switch(reg_ah) {
	case 0x00:		/* PRINTER: Write Character */
		reg_ah=1;	/* Report a timeout */
		break;
	case 0x01:		/* PRINTER: Initialize port */
		break;
	case 0x02:		/* PRINTER: Get Status */
		reg_ah=0;	
		break;
	default:
		E_Exit("Unhandled INT 17 call %2X",reg_ah);
	};
	return CBRET_NONE;
}


static Bitu INT15_Handler(void) {
	switch (reg_ah) {
	case 0x06:
		LOG(LOG_BIOS,LOG_NORMAL)("INT15 Unkown Function 6");
		break;
	case 0xC0:	/* Get Configuration*/
		LOG(LOG_BIOS,LOG_ERROR)("Request BIOS Configuration INT 15 C0");
		CALLBACK_SCF(true);
		break;
	case 0x4f:	/* BIOS - Keyboard intercept */
		/* Carry should be set but let's just set it just in case */
		CALLBACK_SCF(true);
		break;
	case 0x83:	/* BIOS - SET EVENT WAIT INTERVAL */
		{
			if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
				reg_ah=0x80;
				CALLBACK_SCF(true);
				break;
			}
			Bit32u count=(reg_cx<<16)|reg_dx;
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(SegValue(es),reg_bx));
			mem_writed(BIOS_WAIT_FLAG_COUNT,count);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);
			/* Reprogram RTC to start */
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)|0x40);
			CALLBACK_SCF(false);
		}
		break;
	case 0x84:	/* BIOS - JOYSTICK SUPPORT (XT after 11/8/82,AT,XT286,PS) */
		if (reg_dx==0x0000) {
			// Get Joystick button status
			if (JOYSTICK_IsEnabled(0) || JOYSTICK_IsEnabled(1)) {
				reg_al  = (JOYSTICK_GetButton(0,0)<<7)|(JOYSTICK_GetButton(0,1)<<6);
				reg_al |= (JOYSTICK_GetButton(1,0)<<5)|(JOYSTICK_GetButton(1,1)<<4);
				CALLBACK_SCF(false);
			} else {
				// dos values
				reg_ax = 0x00f0; reg_dx = 0x0201;
				CALLBACK_SCF(true);
			}
		} else if (reg_dx==0x0001) {
			if (JOYSTICK_IsEnabled(0) || JOYSTICK_IsEnabled(1)) {
				reg_ax = (Bit16u)JOYSTICK_GetMove_X(0);
				reg_bx = (Bit16u)JOYSTICK_GetMove_Y(0);
				reg_cx = (Bit16u)JOYSTICK_GetMove_X(1);
				reg_dx = (Bit16u)JOYSTICK_GetMove_Y(1);
				CALLBACK_SCF(false);
			} else {			
				reg_ax=reg_bx=reg_cx=reg_dx=0;
				CALLBACK_SCF(true);
			}
		} else {
			LOG(LOG_BIOS,LOG_ERROR)("INT15:84:Unknown Bios Joystick functionality.");
		}
		break;
	case 0x86:	/* BIOS - WAIT (AT,PS) */
		{
			//TODO Perhaps really wait :)
			Bit32u micro=(reg_cx<<16)|reg_dx;
			CALLBACK_SCF(false);
		}
	case 0x88:	/* SYSTEM - GET EXTENDED MEMORY SIZE (286+) */
		IO_Write(0x70,0x30);
		reg_al=IO_Read(0x71);
		IO_Write(0x70,0x31);
		reg_ah=IO_Read(0x71);
		CALLBACK_SCF(false);
		break;
	case 0x90:	/* OS HOOK - DEVICE BUSY */
		CALLBACK_SCF(false);
		reg_ah=0;
		break;
	case 0x91:	/* OS HOOK - DEVICE POST */
		CALLBACK_SCF(false);
		reg_ah=0;
		break;
	case 0xc2:	/* BIOS PS2 Pointing Device Support */
	case 0xc4:	/* BIOS POS Programma option Select */
		/* 
			Damn programs should use the mouse drivers 
			So let's fail these calls 
		*/
		LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function %X called,bios mouse not supported",reg_ah);
		CALLBACK_SCF(true);
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call %2X",reg_ah);
		reg_ah=0x86;
		CALLBACK_SCF(false);
	}
	return CBRET_NONE;
}

static Bitu INT1_Single_Step(void) {
	static bool warned=false;
	if (!warned) {
		warned=true;
		LOG(LOG_CPU,LOG_NORMAL)("INT 1:Single Step called");
	}
	return CBRET_NONE;
}

void BIOS_SetupKeyboard(void);
void BIOS_SetupDisks(void);

void BIOS_Init(Section* sec) {
    MSG_Add("BIOS_CONFIGFILE_HELP","Nothing to setup yet!\n");
	/* Clear the Bios Data Area */
	for (Bit16u i=0;i<1024;i++) real_writeb(0x40,i,0);
	/* Setup all the interrupt handlers the bios controls */
	/* INT 8 Clock IRQ Handler */
	//TODO Maybe give this a special callback that will also call int 8 instead of starting 
	//a new system
	call_int8=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int8,&INT8_Handler,CB_IRET);
	mem_writed(BIOS_TIMER,0);			//Calculate the correct time
	RealSetVec(0x8,CALLBACK_RealPointer(call_int8));
	/* INT10 Video Bios */
	
	/* INT 11 Get equipment list */
	call_int11=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int11,&INT11_Handler,CB_IRET);
	RealSetVec(0x11,CALLBACK_RealPointer(call_int11));
	/* INT 12 Memory Size default at 640 kb */
	call_int12=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int12,&INT12_Handler,CB_IRET);
	RealSetVec(0x12,CALLBACK_RealPointer(call_int12));
	mem_writew(BIOS_MEMORY_SIZE,640);
	/* INT 13 Bios Disk Support */
	BIOS_SetupDisks();
	/* INT 15 Misc Calls */
	call_int15=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int15,&INT15_Handler,CB_IRET);
	RealSetVec(0x15,CALLBACK_RealPointer(call_int15));
	/* INT 16 Keyboard handled in another file */
	BIOS_SetupKeyboard();
	/* INT 16 Printer Routines */
	call_int17=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int17,&INT17_Handler,CB_IRET);
	RealSetVec(0x17,CALLBACK_RealPointer(call_int17));
	/* INT 1A TIME and some other functions */
	call_int1a=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int1a,&INT1A_Handler,CB_IRET_STI);
	RealSetVec(0x1A,CALLBACK_RealPointer(call_int1a));
	/* INT 1C System Timer tick called from INT 8 */
	call_int1c=CALLBACK_Allocate();
	CALLBACK_Setup(call_int1c,&INT1C_Handler,CB_IRET);
	RealSetVec(0x1C,CALLBACK_RealPointer(call_int1c));
	/* IRQ 8 RTC Handler */
	call_int70=CALLBACK_Allocate();
	CALLBACK_Setup(call_int70,&INT70_Handler,CB_IRET);
	RealSetVec(0x70,CALLBACK_RealPointer(call_int70));

	/* Some defeault CPU error interrupt handlers */
	call_int1=CALLBACK_Allocate();
	CALLBACK_Setup(call_int1,&INT1_Single_Step,CB_IRET);
	RealSetVec(0x1,CALLBACK_RealPointer(call_int1));

	/* Setup some stuff in 0x40 bios segment */
	/* Test for parallel port */
	if (IO_Read(0x378)!=0xff) real_writew(0x40,0x08,0x378);
	/* Test for serial port */
	Bitu index=0;
	if (IO_Read(0x3f8)!=0xff) real_writew(0x40,(index++)*2,0x3f8);
	if (IO_Read(0x2f8)!=0xff) real_writew(0x40,(index++)*2,0x2f8);
	/* Setup equipment list */
	mem_writew(BIOS_CONFIGURATION,0xc823);		//1 Floppy,FPU,2 serial, 1 parallel

}




