// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "dosbox.h"
#include "utils/math_utils.h"
#include "hardware/memory.h"

/* Callback are located at 0xF000:0x1000  (see CB_SEG and CB_SOFFSET in callback.h)
   And they are 16 bytes each and you can define them to behave in certain ways like a
   far return or and IRET
*/

Callback_Handler Callback_Handlers[CB_MAX];
std::string Callback_Description[CB_MAX];

static callback_number_t call_stop    = 0;
static callback_number_t call_idle    = 0;
static callback_number_t call_default = 0;

callback_number_t call_priv_io = 0;

static Bitu illegal_handler()
{
	E_Exit("CALLBACK: Illegal Callback called");
}

callback_number_t CALLBACK_Allocate()
{
	// Ensure our returned type can handle the maximum callback
	static_assert(CB_MAX < std::numeric_limits<callback_number_t>::max());

	for (callback_number_t i = 1; i < CB_MAX; ++i) {
		if (Callback_Handlers[i] == &illegal_handler) {
			Callback_Handlers[i] = nullptr;
			return i;
		}
	}
	E_Exit("CALLBACK: Can't allocate handler.");
	return 0;
}

void CALLBACK_DeAllocate(callback_number_t cb_num)
{
	Callback_Handlers[cb_num] = &illegal_handler;
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
		Callback_Description[cb_num] = descr;
	} else
		Callback_Description[cb_num].clear();
}

const char* CALLBACK_GetDescription(callback_number_t cb_num)
{
	if (cb_num >= CB_MAX)
		return nullptr;
	return Callback_Description[cb_num].c_str();
}

static uint8_t callback_setup_extra(const callback_number_t callback_number,
                                    const uint8_t callback_type,
                                    const PhysPt start_address,
                                    const bool use_callback = true)
{
	if (callback_number >= CB_MAX) {
		LOG_ERR("CPU: Unknown callback number 0x%04x", callback_number);
		return 0;
	}

	auto current_address = start_address;

	auto advance = [&](const uint8_t bytes) {
		current_address += bytes;
	};

	auto add_instruction_1 = [&](const uint8_t byte) {
		phys_writeb(current_address, byte);
		++current_address;
	};

	auto add_instruction_2 = [&](const uint8_t byte1, const uint8_t byte2) {
		add_instruction_1(byte1);
		add_instruction_1(byte2);
	};

	auto add_instruction_3 = [&](const uint8_t byte1,
	                             const uint8_t byte2,
	                             const uint8_t byte3) {
		add_instruction_1(byte1);
		add_instruction_1(byte2);
		add_instruction_1(byte3);
	};

	auto add_instruction_4 = [&](const uint8_t byte1,
	                             const uint8_t byte2,
	                             const uint8_t byte3,
	                             const uint8_t byte4) {
		add_instruction_1(byte1);
		add_instruction_1(byte2);
		add_instruction_1(byte3);
		add_instruction_1(byte4);
	};

	// Effectively 4-byte long - reflect it in the lambda name, to make
	// it easier to calculate relative jump addresses
	auto add_native_call_4 = [&](const callback_number_t cb_num) {
		add_instruction_1(0xfe); // GRP 4
		add_instruction_1(0x38); // extra callback instruction
		add_instruction_2(low_byte(cb_num), high_byte(cb_num));
	};

	switch (callback_type) {
	case CB_RETN:
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xc3);                   // retn
		break;
	case CB_RETF:
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcb);                   // retf
		break;
	case CB_RETF8:
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_3(0xca, 0x08, 0x00);       // retf 8
		break;
	case CB_RETF_STI:
		add_instruction_1(0xfb);                   // sti
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcb);                   // retf
		break;
	case CB_RETF_CLI:
		add_instruction_1(0xfa);                   // cli
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcb);                   // retf
		break;
	case CB_IRET:
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_IRETD:
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_2(0x66, 0xcf);             // iretd
		break;
	case CB_IRET_STI:
		add_instruction_1(0xfb);                   // sti
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_IRET_EOI_PIC1:
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0x50);                   // push ax
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_IRQ0: // timer INT8
		add_instruction_1(0xfb);                   // sti
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0x1e);                   // push ds
		add_instruction_1(0x50);                   // push ax
		add_instruction_1(0x52);                   // push dx
		add_instruction_2(0xcd, 0x1c);             // int 0x1c
		add_instruction_1(0xfa);                   // cli
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x5a);                   // pop dx
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0x1f);                   // pop ds
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_IRQ1: // keyboard INT9
		add_instruction_1(0x50);                   // push ax
		// Disable keyboard port
		add_instruction_2(0xb0, 0xad);             // mov al, 0xad
		add_instruction_2(0xe6, 0x64);             // out 0x64, al
		// Read the keyboard input
		add_instruction_2(0xe4, 0x60);             // in al, 0x60
		// Re-enable keyboard port
		add_instruction_1(0x50);                   // push ax
		add_instruction_2(0xb0, 0xae);             // mov al, 0xae
		add_instruction_2(0xe6, 0x64);             // out 0x64, al
		add_instruction_1(0x58);                   // pop ax
		// Handle keyboard interception via INT 15h
		add_instruction_2(0xb4, 0x4f);             // mov ah, 0x4f
		add_instruction_1(0xf9);                   // stc
		add_instruction_2(0xcd, 0x15);             // int 0x15
		if (use_callback) {
			add_instruction_2(0x73, 0x04);     // jc skip
			add_native_call_4(callback_number);
			// jump here to (skip):
		}
		// Process the key, handle PIC
		add_instruction_1(0xfa);                   // cli
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcf);                   // iret
		add_instruction_1(0xfa);                   // cli
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x55);                   // push bp
		add_instruction_2(0xcd, 0x05);             // int 0x05
		add_instruction_1(0x5d);                   // pop bp
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_IRQ9: // PIC cascade interrupt
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0x50);                   // push ax
		add_instruction_2(0xb0, 0x61);             // mov al, 0x61
		add_instruction_2(0xe6, 0xa0);             // out 0xa0, al
		add_instruction_2(0xcd, 0x0a);             // int 0x0a
		add_instruction_1(0xfa);                   // cli
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_IRQ12: // PS/2 mouse INT7
		if (!use_callback) {
			LOG_ERR("CPU: INT74 callback must implement a callback handler!");
			return 0;
		}
		add_instruction_1(0xfb);                   // sti
		add_instruction_1(0x1e);                   // push ds
		add_instruction_1(0x06);                   // push es
		add_instruction_2(0x66, 0x60);             // pushad
		add_native_call_4(callback_number);
		add_instruction_1(0x50);                   // push ax
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0xa0);             // out 0xa0, al
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xfc);                   // cld
		add_instruction_1(0xcb);                   // retf
		break;
	case CB_IRQ12_RET: // PS/2 mouse INT74 return
		add_instruction_1(0xfa);                   // cli
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0xa0);             // out 0xa0, al
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_2(0x66, 0x61);             // popad
		add_instruction_1(0x07);                   // pop es
		add_instruction_1(0x1f);                   // pop ds
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_IRQ6_PCJR: // PCjr keyboard interrupt
		add_instruction_1(0x50);                   // push ax
		add_instruction_2(0xe4, 0x60);             // in al, 0x60
		add_instruction_2(0x3c, 0xe0);             // cmp al, 0xe0
		if (use_callback) {
			add_instruction_2(0x74, 0x0b);     // je skip
			add_native_call_4(callback_number);
		} else {
			add_instruction_2(0x74, 0x07);     // je skip
		}
		add_instruction_1(0x1e);                   // push ds
		add_instruction_2(0x6a, 0x40);             // push 0x0040
		add_instruction_1(0x1f);                   // pop ds
		add_instruction_2(0xcd, 0x09);             // int 0x09
		add_instruction_1(0x1f);                   // pop ds
		// label: skip
		add_instruction_1(0xfa);                   // cli
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_MOUSE:
		add_instruction_2(0xeb, 0x07);             // jmp i33hd
		advance(7);
		// label: i33hd
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_INT16:
		add_instruction_1(0xfb);                   // sti
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcf);                   // iret
		for (auto i = 0; i <= 0x0b; ++i) {
			add_instruction_1(0x90);           // nop
		}
		add_instruction_2(0xeb, 0xed);             // jmp callback
		break;
	case CB_INT29: // fast console output
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0x50);                   // push ax
		add_instruction_1(0x53);                   // push bx
		add_instruction_2(0xb4, 0x0e);             // mov ah, 0x0e
		add_instruction_3(0xbb, 0x07, 0x00);       // mov bx, 0x0007
		add_instruction_2(0xcd, 0x10);             // int 0x10
		add_instruction_1(0x5b);                   // pop bx
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_HOOKABLE:
		add_instruction_2(0xeb, 0x03);             // jmp after nops
		add_instruction_1(0x90);                   // nop
		add_instruction_1(0x90);                   // nop
		add_instruction_1(0x90);                   // nop
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcb);                   // retf
		break;
	case CB_TDE_IRET: // Tandy DAC end transfer
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0x50);                   // push ax
		add_instruction_3(0xb8, 0xfb, 0x91);       // mov ax, 0x91fb
		add_instruction_2(0xcd, 0x15);             // int 0x15
		add_instruction_1(0xfa);                   // cli
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcf);                   // iret
		break;
/*	
	case CB_IPXESR: // IPX ESR
		if (!use_callback) {
			LOG_ERR("CPU: IPX esr must implement a callback handler!");
			return 0;
		}
		add_instruction_1(0x1e);                   // push ds
		add_instruction_1(0x06);                   // push es
		add_instruction_2(0x0f, 0xa0);             // push fs
		add_instruction_2(0x0f, 0xa8);             // push gs		
		add_instruction_1(0x60);                   // pusha
		add_native_call_4(callback_number);
		add_instruction_1(0xcb);                   // retf
		break;
	case CB_IPXESR_RET: // IPX ESR return
		if (use_callback) {
			LOG_ERR("CPU: IPX esr return must not implement a callback handler!");
			return 0;
		}
		add_instruction_1(0xfa);                   // cli
		add_instruction_2(0xb0, 0x20);             // mov al, 0x20
		add_instruction_2(0xe6, 0xa0);             // out 0xa0, al
		add_instruction_2(0xe6, 0x20);             // out 0x20, al
		add_instruction_1(0x61);                   // popa
		add_instruction_2(0x0f, 0xa9);             // pop gs
		add_instruction_2(0x0f, 0xa1);             // pop fs
		add_instruction_1(0x07);                   // pop es
		add_instruction_1(0x1f);                   // pop ds
		add_instruction_1(0xcf);                   // iret
		break;
*/
	case CB_INT21:
		add_instruction_1(0xfb);                   // sti
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcf);                   // iret
		add_instruction_1(0xcb);                   // retf
		add_instruction_1(0x51);                   // push cx
		add_instruction_3(0xb9, 0x40, 0x01);       // mov cx, 0x0140		
		add_instruction_2(0xe2, 0xfe);             // loop $-2
		add_instruction_1(0x59);                   // pop cx
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_INT13:
		add_instruction_1(0xfb);                   // sti
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_1(0xcf);                   // iret
		add_instruction_2(0xcd, 0x0e);             // int 0x0e
		add_instruction_1(0xcf);                   // iret
		break;
	case CB_VESA_WAIT:
		if (use_callback) {
			LOG_ERR("CPU: VESA wait must not implement a callback handler!");
			return 0;
		}
		add_instruction_1(0xfb);                   // sti
		add_instruction_1(0x50);                   // push ax
		add_instruction_1(0x52);                   // push dx
		add_instruction_3(0xba, 0xda, 0x03);       // mov dx, 0x03da
		add_instruction_1(0xec);                   // in al, dx
		add_instruction_2(0xa8, 0x08);             // test al, 0x08
		add_instruction_2(0x75, 0xfb);             // jne $-5
		add_instruction_1(0xec);                   // in al, dx
		add_instruction_2(0xa8, 0x08);             // test al, 0x08
		add_instruction_2(0x74, 0xfb);             // je $-5
		add_instruction_1(0x5a);                   // pop dx
		add_instruction_1(0x58);                   // pop ax
		add_instruction_1(0xcb);                   // retf
		break;
	case CB_VESA_PM:
		if (use_callback) {
			add_native_call_4(callback_number);
		}
		add_instruction_3(0xf6, 0xc3, 0x80);       // test bl, 0x80
		add_instruction_2(0x74, 0x16);             // je $+22
		add_instruction_2(0x66, 0x50);             // push ax
		add_instruction_2(0x66, 0x52);             // push dx
		add_instruction_4(0x66, 0xba, 0xda, 0x03); // mov dx, 0x03da
		add_instruction_1(0xec);                   // in al, dx
		add_instruction_2(0xa8, 0x08);             // test al, 0x08
		add_instruction_2(0x75, 0xfb);             // jne $-5
		add_instruction_1(0xec);                   // in al, dx
		add_instruction_2(0xa8, 0x08);             // test al, 0x08
		add_instruction_2(0x74, 0xfb);             // je $-5
		add_instruction_2(0x66, 0x5a);             // pop dx
		add_instruction_2(0x66, 0x58);             // pop ax
		if (use_callback) {
			add_instruction_1(0xc3);           // retn
		}
		break;
	default:
		LOG_ERR("CPU: Unknown callback type 0x%04x", callback_type);
		return 0;
	}

	return check_cast<uint8_t>(current_address - start_address);
}

bool CALLBACK_Setup(callback_number_t cb_num, Callback_Handler handler, Bitu type, const char* descr)
{
	if (cb_num >= CB_MAX)
		return false;
	const bool use_callback = (handler != nullptr);
	callback_setup_extra(cb_num, type, CALLBACK_PhysPointer(cb_num), use_callback);
	Callback_Handlers[cb_num] = handler;
	CALLBACK_SetDescription(cb_num, descr);
	return true;
}

callback_number_t CALLBACK_Setup(callback_number_t cb_num, Callback_Handler handler, Bitu type, PhysPt addr, const char* descr)
{
	if (cb_num >= CB_MAX)
		return 0;

	const bool use_callback = (handler != nullptr);

	const auto csize = callback_setup_extra(cb_num, type, addr, use_callback);
	if (csize > 0) {
		Callback_Handlers[cb_num] = handler;
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

	if (!Callback_Description[m_cb_number].empty())
		Callback_Description[m_cb_number].clear();

	CALLBACK_DeAllocate(m_cb_number);
	installed=false;
}

CALLBACK_HandlerObject::~CALLBACK_HandlerObject(){
	Uninstall();
}

void CALLBACK_HandlerObject::Install(Callback_Handler handler,Bitu type,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_cb_number = CALLBACK_Allocate();
		CALLBACK_Setup(m_cb_number, handler, type, description);
	} else
		E_Exit("Callback handler object already installed");
}
void CALLBACK_HandlerObject::Install(Callback_Handler handler,Bitu type,PhysPt addr,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_cb_number = CALLBACK_Allocate();
		CALLBACK_Setup(m_cb_number, handler, type, addr, description);
	} else
		E_Exit("Callback handler object already installed");
}

void CALLBACK_HandlerObject::Allocate(Callback_Handler handler,const char* description) {
	if(!installed) {
		installed=true;
		m_type=NONE;
		m_cb_number = CALLBACK_Allocate();
		CALLBACK_SetDescription(m_cb_number, description);
		Callback_Handlers[m_cb_number] = handler;
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
		Callback_Handlers[i]=&illegal_handler;
	}

	/* Setup the Stop Handler */
	call_stop=CALLBACK_Allocate();
	Callback_Handlers[call_stop]=stop_handler;
	CALLBACK_SetDescription(call_stop,"stop");
	phys_writeb(CALLBACK_PhysPointer(call_stop)+0,0xFE);
	phys_writeb(CALLBACK_PhysPointer(call_stop)+1,0x38);
	phys_writew(CALLBACK_PhysPointer(call_stop)+2,(uint16_t)call_stop);

	/* Setup the idle handler */
	call_idle=CALLBACK_Allocate();
	Callback_Handlers[call_idle]=stop_handler;
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
	real_writed(0, 0x66 * 4, CALLBACK_RealPointer(call_default));	//war2d
	real_writed(0, 0x67 * 4, CALLBACK_RealPointer(call_default));
	if (is_machine_cga()) {
		// Popcorn
		real_writed(0, 0x68*4,0);
	}
	real_writed(0, 0x5c * 4, CALLBACK_RealPointer(call_default));	//Network stuff
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
