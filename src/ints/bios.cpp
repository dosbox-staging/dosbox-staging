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
#include <time.h>
#include "dosbox.h"
#include "bios.h"
#include "regs.h"
#include "callback.h"
#include "inout.h"
#include "mem.h"
#include "timer.h"

static Bitu call_int1a,call_int11,call_int8,call_int17,call_int12,call_int15,call_int1c;

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
		reg_dx=reg_cx=0;
		CALLBACK_SCF(false);
		LOG_WARN("INT1A:02:Faked RTC get time call");
		break;
	case 0x04:	/* GET REAL-TIME ClOCK DATA  (AT,XT286,PS) */
		reg_dx=reg_cx=0;
		CALLBACK_SCF(false);
		LOG_WARN("INT1A:04:Faked RTC get date call");
		break;
	case 0x80:	/* Pcjr Setup Sound Multiplexer */
		LOG_WARN("INT1A:80:Setup tandy sound multiplexer to %d",reg_al);
		break;
	case 0x81:	/* Tandy sound system checks */
		LOG_WARN("INT1A:81:Tandy DAC Check failing");
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
	default:
		LOG_WARN("INT1A:Undefined call %2X",reg_ah);
	}
	return CBRET_NONE;
}	

static Bitu INT11_Handler(void) {
	/*
	AX = BIOS equipment list word
    bits
    0     floppy disk(s) installed (see bits 6-7)
    1     80x87 coprocessor installed
    2,3   number of 16K banks of RAM on motherboard (PC only)
          number of 64K banks of RAM on motherboard (XT only)
    2     pointing device installed (PS)
    3     unused (PS)
    4-5   initial video mode
          00 EGA, VGA, or PGA
          01 40x25 color
          10 80x25 color
          11 80x25 monochrome
    6-7   number of floppies installed less 1 (if bit 0 set)
    8     DMA support installed (PCjr, some Tandy 1000s, 1400LT)
    9-11  number of serial ports installed
    12    game port installed
    13    serial printer attached (PCjr)
          internal modem installed (PC/Convertible)
    14-15 number of parallel ports installed
	*/
	reg_eax=0x104D;
	return CBRET_NONE;
}

static Bitu INT8_Handler(void) {
	/* Increase the bios tick counter */
	mem_writed(BIOS_TIMER,mem_readd(BIOS_TIMER)+1);
	/* decrease floppy motor timer */
	Bit8u val = mem_readb(BIOS_DISK_MOTOR_TIMEOUT);
	if (val>0) mem_writeb(BIOS_DISK_MOTOR_TIMEOUT,val-1);
	
	CALLBACK_RunRealInt(0x1c);
	IO_Write(0x20,0x20);
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
	LOG_ERROR("INT17:Not supported call for bios printer support");
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
};

static void WaitFlagEvent(void) {
	PhysPt where=Real2Phys(mem_readd(BIOS_WAIT_FLAG_POINTER));
	mem_writeb(where,mem_readb(where)|0x80);
	mem_writeb(BIOS_WAIT_FLAG_ACTIVE,0);
}

static Bitu INT15_Handler(void) {
	switch (reg_ah) {
	case 0x06:
		LOG_WARN("Calling unkown int15 function 6");
		break;
	case 0xC0:	/* Get Configuration*/
		LOG_WARN("Request BIOS Configuration INT 15 C0");
		CALLBACK_SCF(true);
		break;
	case 0x4f:	/* BIOS - Keyboard intercept */
		/* Carry should be set but let's just set it just in case */
		CALLBACK_SCF(true);
		break;
	case 0x83:	/* BIOS - SET EVENT WAIT INTERVAL */
		mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(SegValue(es),reg_bx));
		mem_writed(BIOS_WAIT_FLAG_COUNT,reg_cx<<16|reg_dx);
		mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);
		TIMER_RegisterDelayHandler(&WaitFlagEvent,reg_cx<<16|reg_dx);
		break;
	case 0x84:	/* BIOS - JOYSTICK SUPPORT (XT after 11/8/82,AT,XT286,PS) */
		//Does anyone even use this?
		LOG_WARN("INT15:84:Bios Joystick functionality not done");
		reg_ax=reg_bx=reg_cx=reg_dx=0;
		break;
	case 0x86:	/* BIOS - WAIT (AT,PS) */
		{
			//TODO Perhaps really wait :)
			Bit32u micro=(reg_cx<<16)|reg_dx;
			CALLBACK_SCF(false);
		}
	case 0x88:	/* SYSTEM - GET EXTENDED MEMORY SIZE (286+) */
		reg_ax=0;
		CALLBACK_SCF(false);
		break;
	case 0x90:	/* OS HOOK - DEVICE BUSY */
		CALLBACK_SCF(true);
		reg_ah=0;
		break;
	case 0xc2:	/* BIOS PS2 Pointing Device Support */
		/* 
			Damn programs should use the mouse drivers 
			So let's fail these calls 
		*/
		CALLBACK_SCF(true);
		break;
	case 0xc4:	/* BIOS POS Programma option Select */
		LOG_WARN("INT15:C4:Call for POS Function %2x",reg_al);
		CALLBACK_SCF(true);
		break;
	default:
		LOG_WARN("INT15:Unknown call %2X",reg_ah);
		reg_ah=0x86;
		CALLBACK_SCF(false);
	}
	return CBRET_NONE;
};

static void INT15_StartUp(void) {
/* TODO Start the time correctly */
};


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
	CALLBACK_Setup(call_int1a,&INT1A_Handler,CB_IRET);
	RealSetVec(0x1A,CALLBACK_RealPointer(call_int1a));
	/* INT 1C System Timer tick called from INT 8 */
	call_int1c=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int1c,&INT1C_Handler,CB_IRET);
	RealSetVec(0x1C,CALLBACK_RealPointer(call_int1c));

}




