/*
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "cpu.h"

/* CallBack are located at 0xF000:0x1000  (see CB_SEG and CB_SOFFSET in callback.h)
   And they are 16 bytes each and you can define them to behave in certain ways like a
   far return or and IRET
*/

CallBack_Handler CallBack_Handlers[CB_MAX];
std::string CallBack_Description[CB_MAX];

static callback_number_t call_stop    = 0;
static callback_number_t call_idle    = 0;
static callback_number_t call_default = 0;

callback_number_t call_priv_io = 0;

static Bitu illegal_handler()
{
	E_Exit("CALLBACK: Illegal CallBack called");
}

callback_number_t CALLBACK_Allocate()
{
	// Ensure our returned type can handle the maximum callback
	static_assert(CB_MAX < std::numeric_limits<callback_number_t>::max());

	for (callback_number_t i = 1; i < CB_MAX; ++i) {
		if (CallBack_Handlers[i] == &illegal_handler) {
			CallBack_Handlers[i] = nullptr;
			return i;
		}
	}
	E_Exit("CALLBACK: Can't allocate handler.");
	return 0;
}

void CALLBACK_DeAllocate(callback_number_t cb_num)
{
	CallBack_Handlers[cb_num] = &illegal_handler;
}

void CALLBACK_Idle() {
/* this makes the cpu execute instructions to handle irq's and then come back */
	const auto oldIF = GETFLAG(IF);
	SETFLAGBIT(IF,true);
	uint16_t oldcs=SegValue(cs);
	uint32_t oldeip=reg_eip;
	SegSet16(cs,CB_SEG);
	reg_eip=CB_SOFFSET+call_idle*CB_SIZE;
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
	SETFLAGBIT(IF,oldIF);
	if (!CPU_CycleAutoAdjust && CPU_Cycles>0)
		CPU_Cycles=0;
}

static Bitu default_handler()
{
	LOG(LOG_CPU, LOG_ERROR)
	("Illegal Unhandled Interrupt Called %X", CPU_GetLastInterrupt());
	return CBRET_NONE;
}

static Bitu stop_handler() {
	return CBRET_STOP;
}



void CALLBACK_RunRealFar(uint16_t seg,uint16_t off) {
	reg_sp-=4;
	real_writew(SegValue(ss),reg_sp+0,RealOffset(CALLBACK_RealPointer(call_stop)));
	real_writew(SegValue(ss),reg_sp+2,RealSegment(CALLBACK_RealPointer(call_stop)));
	auto oldeip=reg_eip;
	auto oldcs=SegValue(cs);
	reg_eip=off;
	SegSet16(cs,seg);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_RunRealInt(uint8_t intnum) {
	uint32_t oldeip=reg_eip;
	uint16_t oldcs=SegValue(cs);
	reg_eip=CB_SOFFSET+(CB_MAX*CB_SIZE)+(intnum*6);
	SegSet16(cs,CB_SEG);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_SZF(bool val) {
	auto tempf = real_readw(SegValue(ss),reg_sp+4);
	if (val) tempf |= FLAG_ZF;
	else tempf &= ~FLAG_ZF;
	real_writew(SegValue(ss),reg_sp+4,tempf);
}

void CALLBACK_SCF(bool val) {
	auto tempf = real_readw(SegValue(ss),reg_sp+4);
	if (val) tempf |= FLAG_CF;
	else tempf &= ~FLAG_CF;
	real_writew(SegValue(ss),reg_sp+4,tempf);
}

void CALLBACK_SIF(bool val) {
	auto tempf = real_readw(SegValue(ss),reg_sp+4);
	if (val) tempf |= FLAG_IF;
	else tempf &= ~FLAG_IF;
	real_writew(SegValue(ss),reg_sp+4,tempf);
}

void CALLBACK_SetDescription(callback_number_t cb_num, const char* descr)
{
	if (descr) {
		CallBack_Description[cb_num] = descr;
	} else
		CallBack_Description[cb_num].clear();
}

const char* CALLBACK_GetDescription(callback_number_t cb_num)
{
	if (cb_num >= CB_MAX)
		return nullptr;
	return CallBack_Description[cb_num].c_str();
}

callback_number_t CALLBACK_SetupExtra(callback_number_t cb_num, Bitu type, PhysPt physAddress, bool use_cb = true)
{
	if (cb_num >= CB_MAX)
		return 0;
	switch (type) {
	case CB_RETN:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xC3);		//A RETN Instruction
		return (use_cb?5:1);
	case CB_RETF:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?5:1);
	case CB_RETF8:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCA);		//A RETF 8 Instruction
		phys_writew(physAddress+0x01,(uint16_t)0x0008);
		return (use_cb?7:3);
	case CB_RETF_STI:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x03, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?6:2);
	case CB_RETF_CLI:
		phys_writeb(physAddress+0x00,(uint8_t)0xFA);		//CLI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x03, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?6:2);
	case CB_IRET:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02,
			            cb_num); // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?5:1);
	case CB_IRETD:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x66);		//An IRETD Instruction
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);
		return (use_cb?6:2);
	case CB_IRET_STI:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x03, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?6:2);
	case CB_IRET_EOI_PIC1:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x01,(uint8_t)0xb0);		// mov al, 0x20
		phys_writeb(physAddress+0x02,(uint8_t)0x20);
		phys_writeb(physAddress+0x03,(uint8_t)0xe6);		// out 0x20, al
		phys_writeb(physAddress+0x04,(uint8_t)0x20);
		phys_writeb(physAddress+0x05,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x06,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x0b:0x07);
	case CB_IRQ0:	// timer int8
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x03, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0x1e);		// push ds
		phys_writeb(physAddress+0x02,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x03,(uint8_t)0x52);		// push dx
		phys_writew(physAddress+0x04,(uint16_t)0x1ccd);	// int 1c
		phys_writeb(physAddress+0x06,(uint8_t)0xfa);		// cli
		phys_writew(physAddress+0x07,(uint16_t)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x09,(uint16_t)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x0b,(uint8_t)0x5a);		// pop dx
		phys_writeb(physAddress+0x0c,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x0d,(uint8_t)0x1f);		// pop ds
		phys_writeb(physAddress+0x0e,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x13:0x0f);
	case CB_IRQ1:	// keyboard int9
		phys_writeb(physAddress+0x00,(uint8_t)0x50);			// push ax
		phys_writew(physAddress+0x01,(uint16_t)0x60e4);		// in al, 0x60
		phys_writew(physAddress+0x03,(uint16_t)0x4fb4);		// mov ah, 0x4f
		phys_writeb(physAddress+0x05,(uint8_t)0xf9);			// stc
		phys_writew(physAddress+0x06,(uint16_t)0x15cd);		// int 15
		if (use_cb) {
			phys_writew(physAddress+0x08,(uint16_t)0x0473);	// jc skip
			phys_writeb(physAddress+0x0a,(uint8_t)0xFE);		//GRP 4
			phys_writeb(physAddress+0x0b,(uint8_t)0x38);		//Extra Callback instruction
			phys_writew(physAddress + 0x0c, cb_num);                // The immediate word
			// jump here to (skip):
			physAddress+=6;
		}
		phys_writeb(physAddress+0x08,(uint8_t)0xfa);			// cli
		phys_writew(physAddress+0x09,(uint16_t)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x0b,(uint16_t)0x20e6);		// out 0x20, al
		phys_writeb(physAddress+0x0d,(uint8_t)0x58);			// pop ax
		phys_writeb(physAddress+0x0e,(uint8_t)0xcf);			//An IRET Instruction
		phys_writeb(physAddress+0x0f,(uint8_t)0xfa);			// cli
		phys_writew(physAddress+0x10,(uint16_t)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x12,(uint16_t)0x20e6);		// out 0x20, al
		phys_writeb(physAddress+0x14,(uint8_t)0x55);			// push bp
		phys_writew(physAddress+0x15,(uint16_t)0x05cd);		// int 5
		phys_writeb(physAddress+0x17,(uint8_t)0x5d);			// pop bp
		phys_writeb(physAddress+0x18,(uint8_t)0x58);			// pop ax
		phys_writeb(physAddress+0x19,(uint8_t)0xcf);			//An IRET Instruction
		return (use_cb?0x20:0x1a);
	case CB_IRQ9:	// pic cascade interrupt
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);		// push ax
		phys_writew(physAddress+0x01,(uint16_t)0x61b0);	// mov al, 0x61
		phys_writew(physAddress+0x03,(uint16_t)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(uint16_t)0x0acd);	// int a
		phys_writeb(physAddress+0x07,(uint8_t)0xfa);		// cli
		phys_writeb(physAddress+0x08,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x09,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x0e:0x0a);
	case CB_IRQ12:	// ps2 mouse int74
		if (!use_cb) E_Exit("int74 callback must implement a callback handler!");
		phys_writeb(physAddress+0x00,(uint8_t)0xfb);		// sti
		phys_writeb(physAddress+0x01,(uint8_t)0x1e);		// push ds
		phys_writeb(physAddress+0x02,(uint8_t)0x06);		// push es
		phys_writew(physAddress+0x03,(uint16_t)0x6066);	// pushad
		phys_writeb(physAddress+0x05,(uint8_t)0xFE);		//GRP 4
		phys_writeb(physAddress+0x06,(uint8_t)0x38);		//Extra Callback instruction
		phys_writew(physAddress + 0x07, cb_num);                // The immediate word
		phys_writeb(physAddress + 0x09, (uint8_t)0x50);         // push ax
		phys_writew(physAddress + 0x0a, (uint16_t)0x20b0);      // mov al, 0x20
		phys_writew(physAddress + 0x0c, (uint16_t)0xa0e6);      // out 0xa0, al
		phys_writew(physAddress + 0x0e, (uint16_t)0x20e6);      // out 0x20, al
		phys_writeb(physAddress + 0x10, (uint8_t)0x58);         // pop ax
		phys_writeb(physAddress + 0x11, (uint8_t)0xfc);         // cld
		phys_writeb(physAddress + 0x12, (uint8_t)0xCB);         // A RETF Instruction
		return 0x13;
	case CB_IRQ12_RET:	// ps2 mouse int74 return
		phys_writeb(physAddress+0x00,(uint8_t)0xfa);		// cli
		phys_writew(physAddress+0x01,(uint16_t)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x03,(uint16_t)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(uint16_t)0x20e6);	// out 0x20, al
		if (use_cb) {
			phys_writeb(physAddress+0x07,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x08,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x09, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writew(physAddress+0x07,(uint16_t)0x6166);	// popad
		phys_writeb(physAddress+0x09,(uint8_t)0x07);		// pop es
		phys_writeb(physAddress+0x0a,(uint8_t)0x1f);		// pop ds
		phys_writeb(physAddress+0x0b,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x10:0x0c);
	case CB_IRQ6_PCJR:	// pcjr keyboard interrupt
		phys_writeb(physAddress+0x00,(uint8_t)0x50);			// push ax
		phys_writew(physAddress+0x01,(uint16_t)0x60e4);		// in al, 0x60
		phys_writew(physAddress+0x03,(uint16_t)0xe03c);		// cmp al, 0xe0
		if (use_cb) {
			phys_writew(physAddress+0x05,(uint16_t)0x0b74);	// je skip
			phys_writeb(physAddress+0x07,(uint8_t)0xFE);		//GRP 4
			phys_writeb(physAddress+0x08,(uint8_t)0x38);		//Extra Callback instruction
			phys_writew(physAddress + 0x09, cb_num);                // The immediate word
			physAddress += 4;
		} else {
			phys_writew(physAddress+0x05,(uint16_t)0x0774);	// je skip
		}
		phys_writeb(physAddress+0x07,(uint8_t)0x1e);			// push ds
		phys_writew(physAddress+0x08,(uint16_t)0x406a);		// push 0x0040
		phys_writeb(physAddress+0x0a,(uint8_t)0x1f);			// pop ds
		phys_writew(physAddress+0x0b,(uint16_t)0x09cd);		// int 9
		phys_writeb(physAddress+0x0d,(uint8_t)0x1f);			// pop ds
		// jump here to (skip):
		phys_writeb(physAddress+0x0e,(uint8_t)0xfa);			// cli
		phys_writew(physAddress+0x0f,(uint16_t)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x11,(uint16_t)0x20e6);		// out 0x20, al
		phys_writeb(physAddress+0x13,(uint8_t)0x58);			// pop ax
		phys_writeb(physAddress+0x14,(uint8_t)0xcf);			//An IRET Instruction
		return (use_cb?0x19:0x15);
	case CB_MOUSE:
		phys_writew(physAddress+0x00,(uint16_t)0x07eb);		// jmp i33hd
		physAddress+=9;
		// jump here to (i33hd):
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?0x0e:0x0a);
	case CB_INT16:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x03, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		for (uint8_t i = 0; i <= 0x0b; ++i)
			phys_writeb(physAddress + 0x02 + i, 0x90);
		phys_writew(physAddress + 0x0e, (uint16_t)0xedeb); // jmp callback
		return (use_cb ? 0x10 : 0x0c);
	case CB_INT29: // fast console output
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);	// push ax
		phys_writeb(physAddress+0x01,(uint8_t)0x53);	// push bx
		phys_writew(physAddress+0x02,(uint16_t)0x0eb4);	// mov ah, 0x0e
		phys_writeb(physAddress+0x04,(uint8_t)0xbb);	// mov bx,
		phys_writew(physAddress+0x05,(uint16_t)0x0007);	// 0x0007
		phys_writew(physAddress+0x07,(uint16_t)0x10cd);	// int 10
		phys_writeb(physAddress+0x09,(uint8_t)0x5b);	// pop bx
		phys_writeb(physAddress+0x0a,(uint8_t)0x58);	// pop ax
		phys_writeb(physAddress+0x0b,(uint8_t)0xcf);	//An IRET Instruction
		return (use_cb?0x10:0x0c);
	case CB_HOOKABLE:
		phys_writeb(physAddress+0x00,(uint8_t)0xEB);		//jump near
		phys_writeb(physAddress+0x01,(uint8_t)0x03);		//offset
		phys_writeb(physAddress+0x02,(uint8_t)0x90);		//NOP
		phys_writeb(physAddress+0x03,(uint8_t)0x90);		//NOP
		phys_writeb(physAddress+0x04,(uint8_t)0x90);		//NOP
		if (use_cb) {
			phys_writeb(physAddress+0x05,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x06,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x07, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x05,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?0x0a:0x06);
	case CB_TDE_IRET:	// TandyDAC end transfer
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x01,(uint8_t)0xb8);		// mov ax, 0x91fb
		phys_writew(physAddress+0x02,(uint16_t)0x91fb);
		phys_writew(physAddress+0x04,(uint16_t)0x15cd);	// int 15
		phys_writeb(physAddress+0x06,(uint8_t)0xfa);		// cli
		phys_writew(physAddress+0x07,(uint16_t)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x09,(uint16_t)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x0b,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x0c,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x11:0x0d);
/*	case CB_IPXESR:		// IPX ESR
		if (!use_cb) E_Exit("ipx esr must implement a callback handler!");
		phys_writeb(physAddress+0x00,(uint8_t)0x1e);		// push ds
		phys_writeb(physAddress+0x01,(uint8_t)0x06);		// push es
		phys_writew(physAddress+0x02,(uint16_t)0xa00f);	// push fs
		phys_writew(physAddress+0x04,(uint16_t)0xa80f);	// push gs
		phys_writeb(physAddress+0x06,(uint8_t)0x60);		// pusha
		phys_writeb(physAddress+0x07,(uint8_t)0xFE);		//GRP 4
		phys_writeb(physAddress+0x08,(uint8_t)0x38);		//Extra Callback instruction
		phys_writew(physAddress+0x09,(uint16_t)callback);	//The immediate word
		phys_writeb(physAddress+0x0b,(uint8_t)0xCB);		//A RETF Instruction
		return 0x0c;
	case CB_IPXESR_RET:		// IPX ESR return
		if (use_cb) E_Exit("ipx esr return must not implement a callback handler!");
		phys_writeb(physAddress+0x00,(uint8_t)0xfa);		// cli
		phys_writew(physAddress+0x01,(uint16_t)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x03,(uint16_t)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(uint16_t)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x07,(uint8_t)0x61);		// popa
		phys_writew(physAddress+0x08,(uint16_t)0xA90F);	// pop gs
		phys_writew(physAddress+0x0a,(uint16_t)0xA10F);	// pop fs
		phys_writeb(physAddress+0x0c,(uint8_t)0x07);		// pop es
		phys_writeb(physAddress+0x0d,(uint8_t)0x1f);		// pop ds
		phys_writeb(physAddress+0x0e,(uint8_t)0xcf);		//An IRET Instruction
		return 0x0f; */
	case CB_INT21:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x03, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		phys_writeb(physAddress+0x02,(uint8_t)0xCB);		//A RETF Instruction
		phys_writeb(physAddress+0x03,(uint8_t)0x51);		// push cx
		phys_writeb(physAddress+0x04,(uint8_t)0xB9);		// mov cx,
		phys_writew(physAddress+0x05,(uint16_t)0x0140);		// 0x140
		phys_writew(physAddress+0x07,(uint16_t)0xFEE2);		// loop $-2
		phys_writeb(physAddress+0x09,(uint8_t)0x59);		// pop cx
		phys_writeb(physAddress+0x0A,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?15:11);
	case CB_INT13:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x03, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		phys_writew(physAddress+0x02,(uint16_t)0x0ECD);		// int 0e
		phys_writeb(physAddress+0x04,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?9:5);
	case CB_VESA_WAIT:
		if (use_cb) E_Exit("VESA wait must not implement a callback handler!");
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		// sti
		phys_writeb(physAddress+0x01,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x02,(uint8_t)0x52);		// push dx
		phys_writeb(physAddress+0x03,(uint8_t)0xBA);		// mov dx,
		phys_writew(physAddress+0x04,(uint16_t)0x03DA);	// 0x3da
		phys_writeb(physAddress+0x06,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x07,(uint16_t)0x08A8);	// test al,8
		phys_writew(physAddress+0x09,(uint16_t)0xFB75);	// jne $-5
		phys_writeb(physAddress+0x0B,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x0C,(uint16_t)0x08A8);	// test al,8
		phys_writew(physAddress+0x0E,(uint16_t)0xFB74);	// je $-5
		phys_writeb(physAddress+0x10,(uint8_t)0x5A);		// pop dx
		phys_writeb(physAddress+0x11,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x12,(uint8_t)0xCB);		//A RETF Instruction
		return 19;
	case CB_VESA_PM:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress + 0x02, cb_num);        // The immediate word
			physAddress += 4;
		}
		phys_writew(physAddress+0x00,(uint16_t)0xC3F6);	// test bl,
		phys_writeb(physAddress+0x02,(uint8_t)0x80);		// 0x80
		phys_writew(physAddress+0x03,(uint16_t)0x1674);	// je $+22
		phys_writew(physAddress+0x05,(uint16_t)0x5066);	// push ax
		phys_writew(physAddress+0x07,(uint16_t)0x5266);	// push dx
		phys_writew(physAddress+0x09,(uint16_t)0xBA66);	// mov dx,
		phys_writew(physAddress+0x0B,(uint16_t)0x03DA);	// 0x3da
		phys_writeb(physAddress+0x0D,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x0E,(uint16_t)0x08A8);	// test al,8
		phys_writew(physAddress+0x10,(uint16_t)0xFB75);	// jne $-5
		phys_writeb(physAddress+0x12,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x13,(uint16_t)0x08A8);	// test al,8
		phys_writew(physAddress+0x15,(uint16_t)0xFB74);	// je $-5
		phys_writew(physAddress+0x17,(uint16_t)0x5A66);	// pop dx
		phys_writew(physAddress+0x19,(uint16_t)0x5866);	// pop ax
		if (use_cb)
			phys_writeb(physAddress+0x1B,(uint8_t)0xC3);	//A RETN Instruction
		return (use_cb?32:27);
	default:
		E_Exit("CALLBACK:Setup:Illegal type %" sBitfs(u),type);
	}
	return 0;
}

bool CALLBACK_Setup(callback_number_t cb_num, CallBack_Handler handler, Bitu type, const char* descr)
{
	if (cb_num >= CB_MAX)
		return false;
	const bool should_use_callback = (handler != nullptr);
	CALLBACK_SetupExtra(cb_num, type, CALLBACK_PhysPointer(cb_num) + 0, should_use_callback);
	CallBack_Handlers[cb_num] = handler;
	CALLBACK_SetDescription(cb_num, descr);
	return true;
}

callback_number_t CALLBACK_Setup(callback_number_t cb_num, CallBack_Handler handler, Bitu type, PhysPt addr, const char* descr)
{
	if (cb_num >= CB_MAX)
		return 0;

	const bool should_use_callback = (handler != nullptr);

	const auto csize = CALLBACK_SetupExtra(cb_num, type, addr, should_use_callback);
	if (csize > 0) {
		CallBack_Handlers[cb_num] = handler;
		CALLBACK_SetDescription(cb_num, descr);
	}
	return csize;
}

void CALLBACK_RemoveSetup(callback_number_t cb_num)
{
	for (callback_number_t i = 0; i < CB_SIZE; ++i) {
		phys_writeb(CALLBACK_PhysPointer(cb_num) + i, (uint8_t)0x00);
	}
}

void CALLBACK_HandlerObject::Uninstall(){
	if(!installed) return;
	if(m_type == CALLBACK_HandlerObject::SETUP) {
		if(vectorhandler.installed){
			//See if we are the current handler. if so restore the old one
			if(RealGetVec(vectorhandler.interrupt) == Get_RealPointer()) {
				RealSetVec(vectorhandler.interrupt,vectorhandler.old_vector);
			} else
				LOG(LOG_MISC, LOG_WARN)
			("Interrupt vector changed on %X %s", vectorhandler.interrupt, CALLBACK_GetDescription(m_cb_number));
		}
		CALLBACK_RemoveSetup(m_cb_number);
	} else if(m_type == CALLBACK_HandlerObject::SETUPAT){
		E_Exit("Callback:SETUP at not handled yet.");
	} else if(m_type == CALLBACK_HandlerObject::NONE){
		//Do nothing. Merely DeAllocate the callback
	} else E_Exit("what kind of callback is this!");

	if (!CallBack_Description[m_cb_number].empty())
		CallBack_Description[m_cb_number].clear();

	CALLBACK_DeAllocate(m_cb_number);
	installed=false;
}

CALLBACK_HandlerObject::~CALLBACK_HandlerObject(){
	Uninstall();
}

void CALLBACK_HandlerObject::Install(CallBack_Handler handler,Bitu type,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_cb_number = CALLBACK_Allocate();
		CALLBACK_Setup(m_cb_number, handler, type, description);
	} else
		E_Exit("Callback handler object already installed");
}
void CALLBACK_HandlerObject::Install(CallBack_Handler handler,Bitu type,PhysPt addr,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_cb_number = CALLBACK_Allocate();
		CALLBACK_Setup(m_cb_number, handler, type, addr, description);
	} else
		E_Exit("Callback handler object already installed");
}

void CALLBACK_HandlerObject::Allocate(CallBack_Handler handler,const char* description) {
	if(!installed) {
		installed=true;
		m_type=NONE;
		m_cb_number = CALLBACK_Allocate();
		CALLBACK_SetDescription(m_cb_number, description);
		CallBack_Handlers[m_cb_number] = handler;
	} else
		E_Exit("Callback handler object already installed");
}

void CALLBACK_HandlerObject::Set_RealVec(uint8_t vec){
	if(!vectorhandler.installed) {
		vectorhandler.installed=true;
		vectorhandler.interrupt=vec;
		RealSetVec(vec,Get_RealPointer(),vectorhandler.old_vector);
	} else E_Exit ("double usage of vector handler");
}

void CALLBACK_Init(Section* /*sec*/) {
	for (callback_number_t i = 0; i < CB_MAX; ++i) {
		CallBack_Handlers[i]=&illegal_handler;
	}

	/* Setup the Stop Handler */
	call_stop=CALLBACK_Allocate();
	CallBack_Handlers[call_stop]=stop_handler;
	CALLBACK_SetDescription(call_stop,"stop");
	phys_writeb(CALLBACK_PhysPointer(call_stop)+0,0xFE);
	phys_writeb(CALLBACK_PhysPointer(call_stop)+1,0x38);
	phys_writew(CALLBACK_PhysPointer(call_stop)+2,(uint16_t)call_stop);

	/* Setup the idle handler */
	call_idle=CALLBACK_Allocate();
	CallBack_Handlers[call_idle]=stop_handler;
	CALLBACK_SetDescription(call_idle,"idle");
	for (uint8_t i = 0; i <= 11; ++i)
		phys_writeb(CALLBACK_PhysPointer(call_idle) + i, 0x90);
	phys_writeb(CALLBACK_PhysPointer(call_idle) + 12, 0xFE);
	phys_writeb(CALLBACK_PhysPointer(call_idle) + 13, 0x38);
	phys_writew(CALLBACK_PhysPointer(call_idle) + 14, (uint16_t)call_idle);

	/* Default handlers for unhandled interrupts that have to be non-null */
	call_default=CALLBACK_Allocate();
	CALLBACK_Setup(call_default,&default_handler,CB_IRET,"default");

	/* Only setup default handler for first part of interrupt table */
	for (uint16_t ct=0;ct<0x60;ct++) {
		real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
	}
	for (uint16_t ct=0x68;ct<0x70;ct++) {
		real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
	}
	/* Setup block of 0xCD 0xxx instructions */
	PhysPt rint_base=CALLBACK_GetBase()+CB_MAX*CB_SIZE;
	for (uint16_t i = 0; i <= 0xff; ++i) {
		phys_writeb(rint_base,0xCD);
		phys_writeb(rint_base+1,(uint8_t)i);
		phys_writeb(rint_base+2,0xFE);
		phys_writeb(rint_base+3,0x38);
		phys_writew(rint_base+4,(uint16_t)call_stop);
		rint_base += 6;
	}
	// setup a few interrupt handlers that point to bios IRETs by default
	real_writed(0,0x66*4,CALLBACK_RealPointer(call_default));	//war2d
	real_writed(0,0x67*4,CALLBACK_RealPointer(call_default));
	if (machine==MCH_CGA) real_writed(0,0x68*4,0);				//Popcorn
	real_writed(0,0x5c*4,CALLBACK_RealPointer(call_default));	//Network stuff
	//real_writed(0,0xf*4,0); some games don't like it

	call_priv_io=CALLBACK_Allocate();

	// virtualizable in-out opcodes
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x00,(uint8_t)0xec);	// in al, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x01,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x02,(uint8_t)0xed);	// in ax, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x03,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x04,(uint8_t)0x66);	// in eax, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x05,(uint8_t)0xed);
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x06,(uint8_t)0xcb);	// retf

	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x08,(uint8_t)0xee);	// out dx, al
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x09,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0a,(uint8_t)0xef);	// out dx, ax
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0b,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0c,(uint8_t)0x66);	// out dx, eax
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0d,(uint8_t)0xef);
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0e,(uint8_t)0xcb);	// retf
}
