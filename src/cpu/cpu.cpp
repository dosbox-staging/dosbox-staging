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

/* $Id: cpu.cpp,v 1.34 2003-09-30 13:48:20 finsterr Exp $ */

#include <assert.h>
#include "dosbox.h"
#include "cpu.h"
#include "memory.h"
#include "debug.h"
#include "keyboard.h"
#include "setup.h"
#include "paging.h"

#if 1
#undef LOG
#define LOG(X,Y)
#endif

Flag_Info flags;
CPU_Regs cpu_regs;
CPUBlock cpu;
Segments Segs;

Bits CPU_Cycles=0;
Bits CPU_CycleLeft=0;
Bits CPU_CycleMax=1500;

CPU_Decoder * cpudecoder;

void CPU_Real_16_Slow_Start(bool big);
void CPU_Core_Full_Start(bool big);
void CPU_Core_Normal_Start(bool big);

#if 1

#define realcore_start CPU_Core_Normal_Start
#define pmodecore_start CPU_Core_Normal_Start

#else 

#define realcore_start CPU_Core_Full_Start
#define pmodecore_start CPU_Core_Full_Start

#endif

void CPU_Push16(Bitu value) {
	reg_esp-=2;
	mem_writew(SegPhys(ss) + (reg_esp & cpu.stack.mask) ,value);
}

void CPU_Push32(Bitu value) {
	reg_esp-=4;
	mem_writed(SegPhys(ss) + (reg_esp & cpu.stack.mask) ,value);
}

Bitu CPU_Pop16(void) {
	Bitu val=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp+=2;
	return val;
}

Bitu CPU_Pop32(void) {
	Bitu val=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp+=4;
	return val;
}

PhysPt SelBase(Bitu sel) {
	if (cpu.cr0 & CR0_PROTECTION) {
		Descriptor desc;
		cpu.gdt.GetDescriptor(sel,desc);
		return desc.GetBase();
	} else {
		return sel<<4;
	}
}

void CPU_SetFlags(Bitu word) {
	flags.word=(word|2)&~0x28;
}

bool CPU_CheckCodeType(CODE_TYPE type) {
	if (cpu.code.type==type) return true;
	cpu.code.type=type;
	switch (cpu.code.type) {
	case CODE_REAL:
		realcore_start(false);
		cpu.code.big = false;
		break;
	case CODE_PMODE16:
		pmodecore_start(false);
		break;
	case CODE_PMODE32:
		pmodecore_start(true);
		break;
	}
	return false;
}

Bit8u lastint;
bool Interrupt(Bitu num) {
	lastint=num;
#if C_DEBUG
	switch (num) {
	case 0xcd:
#if C_HEAVY_DEBUG
 		LOG(LOG_CPU,LOG_ERROR)("Call to interrupt 0xCD this is BAD");
		DEBUG_HeavyWriteLogInstruction();
#endif
 		E_Exit("Call to interrupt 0xCD this is BAD");
	case 0x03:
		if (DEBUG_Breakpoint()) return true;
	};
#endif

	if (!cpu.pmode) {
		/* Save everything on a 16-bit stack */
		CPU_Push16(flags.word & 0xffff);
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		SETFLAGBIT(IF,false);
		SETFLAGBIT(TF,false);
		/* Get the new CS:IP from vector table */
		reg_eip=mem_readw(num << 2);
		Segs.val[cs]=mem_readw((num << 2)+2);
		Segs.phys[cs]=Segs.val[cs]<<4;
		cpu.code.big=false;
		return CPU_CheckCodeType(CODE_REAL);
	} else {
		/* Protected Mode Interrupt */
		Descriptor gate;
//TODO Check for software interrupt and check gate's dpl<cpl
		cpu.idt.GetDescriptor(num<<3,gate);
		if (gate.DPL()<cpu.cpl) E_Exit("INT:Gate DPL<CPL");
		switch (gate.Type()) {
		case DESC_286_INT_GATE:
		case DESC_386_INT_GATE:
		case DESC_286_TRAP_GATE:
		case DESC_386_TRAP_GATE:
			{
				Descriptor desc;
				Bitu selector=gate.GetSelector();
				Bitu offset=gate.GetOffset();
				cpu.gdt.GetDescriptor(selector,desc);
				Bitu dpl=desc.DPL();
				if (dpl>cpu.cpl) E_Exit("Interrupt to higher privilege");
				switch (desc.Type()) {
				case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
					if (dpl<cpu.cpl) {
						/* Prepare for gate to lower privilege */
						E_Exit("Interrupt to lower privilege");
						break;
					} 
				case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
				/* Prepare stack for gate to same priviledge */
					if (gate.Type() & 0x8) {	/* 32-bit Gate */
						CPU_Push32(flags.word);
						CPU_Push32(SegValue(cs));
						CPU_Push32(reg_eip);
					} else {					/* 16-bit gate */
						CPU_Push16(flags.word & 0xffff);
						CPU_Push16(SegValue(cs));
						CPU_Push16(reg_ip);
					}
			
					break;		
				default:
					E_Exit("INT:Gate Selector points to illegal descriptor with type %x",desc.Type());
				}
				if (!(gate.Type()&1)) SETFLAGBIT(IF,false);			
				SETFLAGBIT(TF,false);
				SETFLAGBIT(NT,false);
				Segs.val[cs]=(selector&0xfffc) | cpu.cpl;
				Segs.phys[cs]=desc.GetBase();
				cpu.code.big=desc.Big()>0;
				LOG(LOG_CPU,LOG_NORMAL)("INT:Gate to %X:%X big %d %s",selector,reg_eip,desc.Big(),gate.Type() & 0x8 ? "386" : "286");
				reg_eip=offset;
				return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
			}
		default:
			E_Exit("Illegal descriptor type %X for int %X",gate.Type(),num);
		}
	}
	assert(1);
	return false;
}


bool CPU_Exception(Bitu exception,Bit32u error_code) {
	if (!cpu.pmode) { /* RealMode Interrupt */	
		/* Save everything on a 16-bit stack */
		CPU_Push16(flags.word & 0xffff);
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		SETFLAGBIT(IF,false);
		SETFLAGBIT(TF,false);
		/* Get the new CS:IP from vector table */
		reg_eip=mem_readw(exception << 2);
		Segs.val[cs]=mem_readw((exception << 2)+2);
		Segs.phys[cs]=Segs.val[cs]<<4;
		cpu.code.big=false;
		return CPU_CheckCodeType(CODE_REAL);
	} else { /* Protected Mode Exception */
	


	}
	return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
}

bool CPU_IRET(bool use32) {
	if (!cpu.pmode || cpu.v86) {		/* RealMode IRET */
		if (use32) {
			reg_eip=CPU_Pop32();
			SegSet16(cs,CPU_Pop32());
			CPU_SetFlags(CPU_Pop32());
		} else {
			reg_eip=CPU_Pop16();
			SegSet16(cs,CPU_Pop16());
			CPU_SetFlagsw(CPU_Pop16());
		}
		cpu.code.big=false;
		return CPU_CheckCodeType(CODE_REAL);
	} else {	/* Protected mode IRET */
		/* Check if this is task IRET */
		if (GETFLAG(NT)) {
		if (GETFLAG(VM)) E_Exit("Pmode IRET with VM bit set");
			E_Exit("Task IRET");


		}
		Bitu selector,offset,old_flags;
		if (use32) {
			offset=CPU_Pop32();
			selector=CPU_Pop32() & 0xffff;
			old_flags=CPU_Pop32();
			if (old_flags & FLAG_VM) E_Exit("No vm86 support");
		} else {
			offset=CPU_Pop16();
			selector=CPU_Pop16();
			old_flags=(flags.word & 0xffff0000) | CPU_Pop16();
		}
		Bitu rpl=selector & 3;
		Descriptor desc;
		cpu.gdt.GetDescriptor(selector,desc);
		if (rpl<cpu.cpl)
			E_Exit("IRET to lower privilege");
		if (cpu.cpl==rpl) {	
			/* Return to same level */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (!(cpu.cpl==desc.DPL())) E_Exit("IRET:Same Level:NC:DPL!=CPL");
				break;
			case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
				if (!(desc.DPL()>=cpu.cpl)) E_Exit("IRET:Same level:C:DPL<CPL");
				break;
			default:
				E_Exit("IRET from illegal descriptor type %X",desc.Type());
			}
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;;
			reg_eip=offset;
			CPU_SetFlags(old_flags);
			LOG(LOG_CPU,LOG_NORMAL)("IRET:Same level return to %X:%X big %d",selector,offset,cpu.code.big);
		} else {
			/* Return to higher privilege */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (!(cpu.cpl==rpl)) E_Exit("IRET:Outer level:NC:RPL != DPL");
				break;
			case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
				if (!(desc.DPL()>cpu.cpl)) E_Exit("IRET:Outer level:C:DPL <= CPL");
				break;
			default:
				E_Exit("IRET from illegal descriptor type %X",desc.Type());
			}
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=selector;
			cpu.cpl=rpl;
			reg_eip=offset;
			Bitu new_ss,new_esp;
			if (use32) {
				new_esp=CPU_Pop32();
				new_ss=CPU_Pop32() & 0xffff;
			} else {
				new_esp=CPU_Pop16();
				new_ss=CPU_Pop16();
			}
			reg_esp=new_esp;
			CPU_SetSegGeneral(ss,new_ss);
			//TODO Maybe validate other segments, but why would anyone use them?
			LOG(LOG_CPU,LOG_NORMAL)("IRET:Outer level return to %X:X big %d",selector,offset,cpu.code.big);
		}
		return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
	}
	return false;
}

bool CPU_JMP(bool use32,Bitu selector,Bitu offset) {
	if (!cpu.pmode || cpu.v86) {
		if (!use32) {
			reg_eip=offset&0xffff;
		} else {
			reg_eip=offset;
		}
		SegSet16(cs,selector);
		cpu.code.big=false;
		return CPU_CheckCodeType(CODE_REAL);
	} else {
		Bitu rpl=selector & 3;
		Descriptor desc;
		cpu.gdt.GetDescriptor(selector,desc);
		switch (desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			if (rpl>cpu.cpl) E_Exit("JMP:NC:RPL>CPL");
			if (rpl!=desc.DPL()) E_Exit("JMP:NC:RPL != DPL");
			cpu.cpl=desc.DPL();
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:NC to %X:%X big %d",selector,offset,desc.Big());
			goto CODE_jmp;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:C to %X:%X big %d",selector,offset,desc.Big());
CODE_jmp:
			/* Normal jump to another selector:offset */
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			reg_eip=offset;
			return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
		default:
			E_Exit("JMP Illegal descriptor type %X",desc.Type());
		}
	}
	assert(1);
	return false;
}



bool CPU_CALL(bool use32,Bitu selector,Bitu offset) {
	if (!cpu.pmode || cpu.v86) {
		if (!use32) {
			CPU_Push16(SegValue(cs));
			CPU_Push16(reg_ip);
			reg_eip=offset&0xffff;
		} else {
			CPU_Push32(SegValue(cs));
			CPU_Push32(reg_eip);
			reg_eip=offset;
		}
		cpu.code.big=false;
		SegSet16(cs,selector);
		return CPU_CheckCodeType(CODE_REAL);
	} else {
		Descriptor call;
		Bitu rpl=selector & 3;
		cpu.gdt.GetDescriptor(selector,call);
		/* Check for type of far call */
		switch (call.Type()) {
		case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
			if (rpl>cpu.cpl) E_Exit("CALL:CODE:NC:RPL>CPL");
			if (call.DPL()!=cpu.cpl) E_Exit("CALL:CODE:NC:DPL!=CPL");
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:NC to %X:%X",selector,offset);
			goto call_code;	
		case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
			if (call.DPL()>cpu.cpl) E_Exit("CALL:CODE:C:DPL>CPL");
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:C to %X:%X",selector,offset);
call_code:
			if (!use32) {
				CPU_Push16(SegValue(cs));
				CPU_Push16(reg_ip);
				reg_eip=offset&0xffff;
			} else {
				CPU_Push32(SegValue(cs));
				CPU_Push32(reg_eip);
				reg_eip=offset;
			}
			Segs.phys[cs]=call.GetBase();
			cpu.code.big=call.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			reg_eip=offset;
			return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
		case DESC_286_CALL_GATE: { 
			if (call.DPL()<cpu.cpl) E_Exit("286 Call Gate: Gate DPL<CPL");
			if (call.DPL()<rpl)		E_Exit("286 Call Gate: Gate DPL<RPL");			
			Descriptor code;
			if (!cpu.gdt.GetDescriptor(call.GetSelector(),code)) E_Exit("286 Call Gate: Invalid code segment.");
			Bitu seldpl = code.DPL();
			if (seldpl<cpu.cpl) {
				// higher privilidge level
				// Get new SS selector for new privilege level from TSS
				// We dont have a TSS now, so we ´simply use our existing stack...
				// 1. Load new SS:eSP value from TSS
				// 2. Load new CS:EIP value from gate
				Bitu newcs	= call.GetSelector();
				Bitu neweip	= call.GetOffset();
				Bitu newcpl = seldpl;
				Bitu oldcs	= SegValue(cs);
				Bitu oldip	= reg_ip;
				// 3. Load CS descriptor (Set RPL of CS to CPL)
				Segs.phys[cs]	= code.GetBase();
				cpu.code.big	= code.Big()>0;
				reg_eip			= neweip;
				// Set CPL to stack segment DPL
			    // Set RPL of CS to CPL
				cpu.cpl			= newcpl;
				Segs.val[cs]	= (newcs & 0xfffc) | newcpl;
				// 4. Load SS descriptor
				// 5. Push long pointer of old stack onto new stack
				Bitu oldsp = reg_sp;
				CPU_Push16(SegValue(ss));
				CPU_Push16(oldsp);
				// 6. Get word count from call gate, mask to 5 bits
				Bitu wordCount = call.saved.gate.paramcount;
				if (wordCount>0) LOG(LOG_CPU,LOG_NORMAL)("CPU: Callgate 286 wordcount : %d)",wordCount);
				// 7. Copy parameters from old stack onto new stack
				while (wordCount>0) {
					CPU_Push16(mem_readw(SegPhys(ss)+oldsp)); 
					oldsp += 2; wordCount--;
				}
				// Push return address onto new stack
				CPU_Push16(oldcs);
				CPU_Push16(oldip);
//				LOG(LOG_MISC,LOG_ERROR)("CPU: Callgate (Higher) %04X:%04X",newcs,neweip);
				return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
			} else {
				// same privilidge level
				Bitu oldcs	= SegValue(cs);
				Bitu oldip	= reg_ip;
				Bitu newcs	= call.GetSelector() | 3;
				Bitu neweip	= call.GetOffset();
				// 3. Load CS descriptor (Set RPL of CS to CPL)
				Descriptor code2;
				if (!cpu.gdt.GetDescriptor(newcs,code2)) E_Exit("286 Call Gate: Invalid code segment.");
				Segs.phys[cs]	= code.GetBase();
				cpu.code.big	= code.Big()>0;
			    // Set RPL of CS to CPL
				cpu.cpl			= seldpl;
				Segs.val[cs]	= (newcs & 0xfffc) | seldpl;
				reg_eip			= neweip;
				// Push return address onto new stack
				CPU_Push16(oldcs);
				CPU_Push16(oldip);
//				LOG(LOG_MISC,LOG_ERROR)("CPU: Callgate (Same) %04X:%04X",newcs,neweip);
				return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
			}; break;
			};
		default:
			E_Exit("CALL:Descriptor type %x unsupported",call.Type());

		}
	}
	assert(1);
}


bool CPU_RET(bool use32,Bitu bytes) {
	if (!cpu.pmode || cpu.v86) {
		Bitu new_ip,new_cs;
		if (!use32) {
			new_ip=CPU_Pop16();
			new_cs=CPU_Pop16();
		} else {
			new_ip=CPU_Pop32();
			new_cs=CPU_Pop32() & 0xffff;
		}
		reg_esp+=bytes;
		SegSet16(cs,new_cs);
		reg_eip=new_ip;
		cpu.code.big=false;
		return CPU_CheckCodeType(CODE_REAL);
	} else {
		Bitu offset,selector;
		if (!use32) {
			offset=CPU_Pop16();
			selector=CPU_Pop16();
		} else {
			offset=CPU_Pop32();
			selector=CPU_Pop32() & 0xffff;
		}
		if (cpu.stack.big) {
			reg_esp+=bytes;
		} else {
			reg_sp+=bytes;
		}
		Descriptor desc;
		Bitu rpl=selector & 3;
		if (rpl<cpu.cpl) E_Exit("RET to lower privilege");
		cpu.gdt.GetDescriptor(selector,desc);

		if (cpu.cpl==rpl) {	
			/* Return to same level */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
				if (!(cpu.cpl==desc.DPL())) E_Exit("RET to NC segment of other privilege");
				goto RET_same_level;
			case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
				if (!(desc.DPL()>=cpu.cpl)) E_Exit("RET to C segment of higher privilege");
				break;
			default:
				E_Exit("RET from illegal descriptor type %X",desc.Type());
			}
RET_same_level:	
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=selector;
			reg_eip=offset;
			LOG(LOG_CPU,LOG_NORMAL)("RET - Same level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
		} else {
			/* Return to higher level */
			Bitu newsp = CPU_Pop16();
			Bitu newss = CPU_Pop16();
			cpu.cpl = rpl;
			CPU_SetSegGeneral(ss,newss);
			reg_esp = newsp;
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=selector;
			reg_eip=offset;
//			LOG(LOG_MISC,LOG_ERROR)("RET - Higher level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
		}
		LOG(LOG_CPU,LOG_NORMAL)("Prot ret %X:%X",selector,offset);
		return CPU_CheckCodeType(cpu.code.big ? CODE_PMODE32 : CODE_PMODE16);
	}
	return false;
}


void CPU_SLDT(Bitu & selector) {
	selector=cpu.gdt.SLDT();
}

Bitu tr=0;
void CPU_LLDT(Bitu selector) {
	cpu.gdt.LLDT(selector);
	LOG(LOG_CPU,LOG_NORMAL)("LDT Set to %X",selector);
}

void CPU_STR(Bitu & selector) {
	selector=tr;
}

void CPU_LTR(Bitu selector) {
	tr=selector;
	LOG(LOG_CPU,LOG_NORMAL)("TR Set to %X",selector);
}


void CPU_LGDT(Bitu limit,Bitu base) {
	LOG(LOG_CPU,LOG_NORMAL)("GDT Set to base:%X limit:%X",base,limit);
	cpu.gdt.SetLimit(limit);
	cpu.gdt.SetBase(base);
}

void CPU_LIDT(Bitu limit,Bitu base) {
	LOG(LOG_CPU,LOG_NORMAL)("IDT Set to base:%X limit:%X",base,limit);
	cpu.idt.SetLimit(limit);
	cpu.idt.SetBase(base);
}

void CPU_SGDT(Bitu & limit,Bitu & base) {
	limit=cpu.gdt.GetLimit();
	base=cpu.gdt.GetBase();
}

void CPU_SIDT(Bitu & limit,Bitu & base) {
	limit=cpu.idt.GetLimit();
	base=cpu.idt.GetBase();
}


bool CPU_SET_CRX(Bitu cr,Bitu value) {
	switch (cr) {
	case 0:
		{
			Bitu changed=cpu.cr0 ^ value;		
			if (!changed) return true;
			cpu.cr0=value;
//TODO Maybe always first change to core_full for a change to cr0
			if (value & CR0_PROTECTION) {
				cpu.pmode=true;
				LOG(LOG_CPU,LOG_NORMAL)("Protected mode");
				PAGING_Enable((value & CR0_PAGING)>0);
			} else {
				cpu.pmode=false;
				PAGING_Enable(false);
				LOG(LOG_CPU,LOG_NORMAL)("Real mode");
			}
			return false;		//Only changes with next CS change
		}
	case 3:
		PAGING_SetDirBase(value);
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV CR%d,%X",cr,value);
		break;
	}
	return false;
}

Bitu CPU_GET_CRX(Bitu cr) {
	switch (cr) {
	case 0:
		return cpu.cr0;
	case 3:
		return PAGING_GetDirBase();
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, CR%d",cr);
		break;
	}
	return 0;
}


void CPU_SMSW(Bitu & word) {
	word=cpu.cr0;
}

bool CPU_LMSW(Bitu word) {
	word&=0xffff;
	word|=(cpu.cr0&0xffff0000);
	return CPU_SET_CRX(0,word);
}

void CPU_ARPL(Bitu & dest_sel,Bitu src_sel) {
	if ((dest_sel & 3) < (src_sel & 3)) {
		dest_sel=(dest_sel & 0xfffc) + (src_sel & 3);
//		dest_sel|=0xff3f0000;
		SETFLAGBIT(ZF,true);
	} else {
		SETFLAGBIT(ZF,false);
	} 
}
	
void CPU_LAR(Bitu selector,Bitu & ar) {
	Descriptor desc;Bitu rpl=selector & 3;
	ar=0;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	if (!desc.saved.seg.p) {
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
	case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
		break;
	
	case DESC_LDT:
	case DESC_TASK_GATE:

	case DESC_286_TSS_A:		case DESC_286_TSS_B:
	case DESC_286_INT_GATE:		case DESC_286_TRAP_GATE:	
	case DESC_286_CALL_GATE:

	case DESC_386_TSS_A:		case DESC_386_TSS_B:
	case DESC_386_INT_GATE:		case DESC_386_TRAP_GATE:
	case DESC_386_CALL_GATE:
	

	case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
	case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
	case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	/* Valid descriptor */
	ar=desc.saved.fill[1] & 0x00ffff00;
	SETFLAGBIT(ZF,true);
}

void CPU_LSL(Bitu selector,Bitu & limit) {
	Descriptor desc;Bitu rpl=selector & 3;
	limit=0;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	if (!desc.saved.seg.p) {
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
	case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
		break;
	
	case DESC_LDT:
	case DESC_286_TSS_A:
	case DESC_286_TSS_B:
	
	case DESC_386_TSS_A:
	case DESC_386_TSS_B:

	case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
	
	case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
	case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	limit=desc.GetLimit();
	SETFLAGBIT(ZF,true);
}

void CPU_VERR(Bitu selector) {
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	if (!desc.saved.seg.p) {
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:	
		//Conforming readable code segments can be always read 
		break;
	case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:

	case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	SETFLAGBIT(ZF,true);
}

void CPU_VERW(Bitu selector) {
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	if (!desc.saved.seg.p) {
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	SETFLAGBIT(ZF,true);
}


void CPU_SetSegGeneral(SegNames seg,Bitu value) {
	Segs.val[seg]=value;
	if (!cpu.pmode || cpu.v86) {
		Segs.phys[seg]=value << 4;
//TODO maybe just always do this when they enable/real/v86 mode
		if (seg==ss) {
			cpu.stack.big=false;
			cpu.stack.mask=0xffff;
		}
	} else {
		Descriptor desc;
		cpu.gdt.GetDescriptor(value,desc);
		Segs.phys[seg]=desc.GetBase();
		if (seg==ss) {
			if (desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
			}
		}
	}
}

void CPU_CPUID(void) {
	switch (reg_eax) {
	case 0:	/* Vendor ID String and maximum level? */
		reg_eax=0;
		reg_ebx=('G'<< 24) || ('e' << 16) || ('n' << 8) || 'u';
		reg_edx=('i'<< 24) || ('n' << 16) || ('e' << 8) || 'T';
		reg_ecx=('n'<< 24) || ('t' << 16) || ('e' << 8) || 'l';
		break;
	case 1:	/* get processor type/family/model/stepping and feature flags */
		reg_eax=0x402;		/* intel 486 sx? */
		reg_ebx=0;			/* Not Supported */
		reg_ecx=0;			/* No features */
		reg_edx=0;			/* Nothing either */
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled CPUID Function %x",reg_eax);
		break;
	}
}

void CPU_ReadTaskSeg32(PhysPt base,TaskSegment_32 * seg) {
	seg->back   =mem_readw(base+offsetof(TSS_386,back   ));
	seg->esp0   =mem_readd(base+offsetof(TSS_386,esp0   ));
	seg->ss0    =mem_readw(base+offsetof(TSS_386,ss0    ));
	seg->esp1   =mem_readd(base+offsetof(TSS_386,esp1   ));
	seg->ss1    =mem_readw(base+offsetof(TSS_386,ss1    ));
	seg->esp2   =mem_readd(base+offsetof(TSS_386,esp2   ));
	seg->ss2    =mem_readw(base+offsetof(TSS_386,ss2    ));
	
	seg->cr3    =mem_readd(base+offsetof(TSS_386,cr3    ));
	seg->eflags =mem_readd(base+offsetof(TSS_386,eflags ));
	seg->eip    =mem_readd(base+offsetof(TSS_386,eip    ));
	
	seg->eax    =mem_readd(base+offsetof(TSS_386,eax    ));
	seg->ecx    =mem_readd(base+offsetof(TSS_386,ecx    ));
	seg->edx    =mem_readd(base+offsetof(TSS_386,edx    ));
	seg->ebx    =mem_readd(base+offsetof(TSS_386,ebx    ));
	seg->esp    =mem_readd(base+offsetof(TSS_386,esp    ));
	seg->ebp    =mem_readd(base+offsetof(TSS_386,ebp    ));
	seg->esi    =mem_readd(base+offsetof(TSS_386,esi    ));
	seg->edi    =mem_readd(base+offsetof(TSS_386,edi    ));

	seg->es     =mem_readw(base+offsetof(TSS_386,es     ));
	seg->cs     =mem_readw(base+offsetof(TSS_386,cs     ));
	seg->ss     =mem_readw(base+offsetof(TSS_386,ss     ));
	seg->ds     =mem_readw(base+offsetof(TSS_386,ds     ));
	seg->fs     =mem_readw(base+offsetof(TSS_386,fs     ));
	seg->gs     =mem_readw(base+offsetof(TSS_386,gs     ));

	seg->ldt    =mem_readw(base+offsetof(TSS_386,ldt    ));
	seg->trap   =mem_readw(base+offsetof(TSS_386,trap   ));
	seg->io     =mem_readw(base+offsetof(TSS_386,io     ));

}

static Bits HLT_Decode(void) {
	/* Once an interrupt occurs, it should change cpu core */
	CPU_Cycles=0;
	return 0;
}

void CPU_HLT(void) {
	CPU_Cycles=0;
	cpu.code.type=CODE_INIT;
	cpudecoder=&HLT_Decode;
}


extern void GFX_SetTitle(Bits cycles ,Bits frameskip);
static void CPU_CycleIncrease(void) {
	Bits old_cycles=CPU_CycleMax;
	CPU_CycleMax=(Bits)(CPU_CycleMax*1.2);
	CPU_CycleLeft=0;CPU_Cycles=0;
	if (CPU_CycleMax==old_cycles) CPU_CycleMax++;
	LOG_MSG("CPU:%d cycles",CPU_CycleMax);
	GFX_SetTitle(CPU_CycleMax,-1);
}

static void CPU_CycleDecrease(void) {
	CPU_CycleMax=(Bits)(CPU_CycleMax/1.2);
	CPU_CycleLeft=0;CPU_Cycles=0;
	if (!CPU_CycleMax) CPU_CycleMax=1;
	LOG_MSG("CPU:%d cycles",CPU_CycleMax);
	GFX_SetTitle(CPU_CycleMax,-1);
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
	CPU_SetFlags(FLAG_IF);				//Enable interrupts
	cpu.cr0=0xffffffff;
	CPU_SET_CRX(0,0);					//Initialize
	cpu.v86=false;
	cpu.code.big=false;
	cpu.code.type=CODE_INIT;			//So a new cpu core will be started
	cpu.stack.mask=0xffff;
	cpu.stack.big=false;

	CPU_JMP(false,0,0);					//Setup the first cpu core

	KEYBOARD_AddEvent(KBD_f11,KBD_MOD_CTRL,CPU_CycleDecrease);
	KEYBOARD_AddEvent(KBD_f12,KBD_MOD_CTRL,CPU_CycleIncrease);

	CPU_Cycles=0;
	CPU_CycleMax=section->Get_int("cycles");;
	if (!CPU_CycleMax) CPU_CycleMax=1500;
	CPU_CycleLeft=0;
	GFX_SetTitle(CPU_CycleMax,-1);
	MSG_Add("CPU_CONFIGFILE_HELP","The amount of cycles to execute each loop. Lowering this setting will slowdown dosbox\n");

}

