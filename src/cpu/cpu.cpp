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
#include "cpu.h"
#include "memory.h"
#include "debug.h"
#include "keyboard.h"
#include "setup.h"

//Regs regs;

Flag_Info flags;

CPU_Regs cpu_regs;

Segment Segs[6];
Bitu cpu_cycles;

CPU_Decoder * cpudecoder;

static void CPU_CycleIncrease(void) {
	Bitu old_cycles=cpu_cycles;
	cpu_cycles=(Bitu)(cpu_cycles*1.2);
	if (cpu_cycles==old_cycles) cpu_cycles++;
	LOG_MSG("CPU:%d cycles",cpu_cycles);
}

static void CPU_CycleDecrease(void) {
	cpu_cycles=(Bitu)(cpu_cycles/1.2);
	if (!cpu_cycles) cpu_cycles=1;
	LOG_MSG("CPU:%d cycles",cpu_cycles);
}

Bit8u lastint;
void Interrupt(Bit8u num) {
	lastint=num;
//DEBUG THINGIE to check fucked ints

	switch (num) {
	case 0x00:
 		LOG_WARN("Divide Error");
		break;
	case 0x06:
		break;
	case 0x07:
		LOG_WARN("Co Processor Exception");
		break;
	case 0x08:
	case 0x09:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x16:
	case 0x15:
	case 0x1A:
	case 0x17:
	case 0x1C:
	case 0x21:
	case 0x2a:
	case 0x2f:
	case 0x33:
	case 0x67:
	case 0x74:
		break;
	case 0xcd:
 		E_Exit("Call to interrupt 0xCD this is BAD");
	case 0x03:
#if C_DEBUG 
	if (DEBUG_Breakpoint()) return;
#endif
		break;
	case 0x05:
		LOG_MSG("CPU:Out Of Bounds interrupt");
		break;
	default:
//		LOG_WARN("Call to unsupported INT %02X call %02X",num,reg_ah);

		break;
	};
/* Check for 16-bit or 32-bit and then setup everything for the interrupt to start */
	Bit16u pflags;
	pflags=
		(get_CF() << 0) | 
		(get_PF() << 2) | 
		(get_AF() << 4) | 
		(get_ZF() << 6) | 
		(get_SF() << 7) | 
		(flags.tf << 8) |
		(flags.intf << 9) |
		(flags.df << 10) |
		(get_OF() << 11) |
		(flags.io << 12) | 
		(flags.nt <<14);
		
	flags.intf=false;
	flags.tf=false;
/* Save everything on a 16-bit stack */
	reg_sp-=2;
	mem_writew(SegPhys(ss)+reg_sp,pflags);
	reg_sp-=2;
	mem_writew(SegPhys(ss)+reg_sp,SegValue(cs));
	reg_sp-=2;
	mem_writew(SegPhys(ss)+reg_sp,reg_ip);
/* Get the new CS:IP from vector table */
	Bit16u newip=mem_readw(num << 2);
	Bit16u newcs=mem_readw((num <<2)+2);
	SegSet16(cs,newcs);
	reg_ip=newip;
}

void CPU_Real_16_Slow_Start(void);

void SetCPU16bit()
{	
	CPU_Real_16_Slow_Start();
}


void CPU_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	reg_eax=0;
	reg_ebx=0;
	reg_ecx=0;
	reg_edx=0;
	reg_edi=0;
	reg_esi=0;
	reg_ebp=0;
	reg_esp=0;

	SegSet16(cs,0);
	SegSet16(ds,0);
	SegSet16(es,0);
	SegSet16(fs,0);
	SegSet16(gs,0);
	SegSet16(ss,0);

	reg_eip=0;
	flags.type=t_UNKNOWN;
	flags.af=0;
	flags.cf=0;
	flags.cf=0;
	flags.sf=0;
	flags.zf=0;
	flags.intf=true;
	flags.nt=0;
	flags.io=0;

	SetCPU16bit();
	cpu_cycles=section->Get_int("cycles");
	if (!cpu_cycles) cpu_cycles=300;
	KEYBOARD_AddEvent(KBD_f11,CTRL_PRESSED,CPU_CycleDecrease);
	KEYBOARD_AddEvent(KBD_f12,CTRL_PRESSED,CPU_CycleIncrease);

	reg_al=0;
	reg_ah=0;
    MSG_Add("CPU_CONFIGFILE_HELP","The amount of cycles to execute each loop. Lowering this setting will slowdown dosbox\n");
}

