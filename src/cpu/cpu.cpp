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


#include "dosbox.h"
#include "cpu.h"
#include "memory.h"
#include "debug.h"
#include "keyboard.h"
#include "setup.h"

#if 1
#undef LOG_MSG
#define LOG_MSG
#endif

Flag_Info flags;
CPU_Regs cpu_regs;
CPUBlock cpu;
Segments Segs;

Bits CPU_Cycles=0;
Bits CPU_CycleLeft=0;
Bits CPU_CycleMax=1500;

CPU_Decoder * cpudecoder;

void CPU_Real_16_Slow_Start(void);
void CPU_Core_Full_Start(void);

typedef void DecoderStart(void);

//Determine correct core, to start from cpu state.
DecoderStart * CPU_DecorderStarts[8]={
	CPU_Real_16_Slow_Start,				//16-bit real,16-bit stack
//	CPU_Core_Full_Start,				//16-bit prot,16-bit stack
	CPU_Core_Full_Start,				//16-bit prot,16-bit stack
	0,									//32-bit real,16-bit stack		ILLEGAL
	CPU_Core_Full_Start,				//32-bit prot,16-bit stack
	0,									//16-bit real,32-bit stack		ILLEGAL
	CPU_Core_Full_Start,				//16-bit prot,32-bit stack
	0,									//32-bit real,32-bit stack		ILLEGAL
	CPU_Core_Full_Start,				//32-bit prot,32-bit stack
};

INLINE void CPU_Push16(Bitu value) {
	if (cpu.state & STATE_STACK32) {
		reg_esp-=2;
		mem_writew(SegPhys(ss)+reg_esp,value);
	} else {
		reg_sp-=2;
		mem_writew(SegPhys(ss)+reg_sp,value);
	}
}

INLINE void CPU_Push32(Bitu value) {
	if (cpu.state & STATE_STACK32) {
		reg_esp-=4;
		mem_writed(SegPhys(ss)+reg_esp,value);
	} else {
		reg_sp-=4;
		mem_writed(SegPhys(ss)+reg_sp,value);
	}
}

INLINE Bitu CPU_Pop16(void) {
	if (cpu.state & STATE_STACK32) {
		Bitu val=mem_readw(SegPhys(ss)+reg_esp);
		reg_esp+=2;
		return val;
	} else {
		Bitu val=mem_readw(SegPhys(ss)+reg_sp);
		reg_sp+=2;
		return val;
	}
}

INLINE Bitu CPU_Pop32(void) {
	if (cpu.state & STATE_STACK32) {
		Bitu val=mem_readd(SegPhys(ss)+reg_esp);
		reg_esp+=4;
		return val;
	} else {
		Bitu val=mem_readd(SegPhys(ss)+reg_sp);
		reg_sp+=4;
		return val;
	}
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

bool CPU_CheckState(void) {
	Bitu old_state=cpu.state;
	cpu.state=0;	
	if (!(cpu.cr0 & CR0_PROTECTION)) {
		cpu.full.entry=cpu.full.prefix=0;
	} else {
		cpu.state|=STATE_PROTECTED;
		if (Segs.big[cs]) {
			cpu.state|=STATE_USE32;
			cpu.full.entry=0x200;
			cpu.full.prefix=0x02;		/* PREFIX_ADDR */
		} else {
			cpu.full.entry=cpu.full.prefix=0;
		}
		if (Segs.big[ss]) cpu.state|=STATE_STACK32;
		LOG_MSG("CPL Level %x at %X:%X",cpu.cpl,SegValue(cs),reg_eip);
	}
	if (old_state==cpu.state) return true;
	(*CPU_DecorderStarts[cpu.state])();
	return false;
}

Bit8u lastint;
bool Interrupt(Bitu num) {
	lastint=num;
//DEBUG THINGIE to check fucked ints

#if C_DEBUG
	switch (num) {
	case 0x00:
 		LOG(LOG_CPU,LOG_NORMAL)("Divide Error");
		break;
	case 0x06:
		break;
	case 0x07:
		LOG(LOG_FPU,LOG_NORMAL)("Co Processor Exception");
		break;
	case 0x08:
	case 0x09:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x1A:
	case 0x1C:
	case 0x21:
	case 0x2a:
	case 0x2f:
	case 0x33:
	case 0x67:
	case 0x74:
		break;
	case 0xcd:
#if C_HEAVY_DEBUG
 		LOG(LOG_CPU,LOG_ERROR)("Call to interrupt 0xCD this is BAD");
		DEBUG_HeavyWriteLogInstruction();
#endif
 		E_Exit("Call to interrupt 0xCD this is BAD");
	case 0x03:
		if (DEBUG_Breakpoint()) return true;
		break;
	case 0x05:
		LOG(LOG_CPU,LOG_NORMAL)("CPU:Out Of Bounds interrupt");
		break;
	default:
//		LOG_WARN("Call to unsupported INT %02X call %02X",num,reg_ah);
		break;
	};
#endif
	FILLFLAGS;
	
	if (!(cpu.state & STATE_PROTECTED)) { /* RealMode Interrupt */	
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
		return true;
	} else { /* Protected Mode Interrupt */
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
				Segs.big[cs]=desc.Big();
				LOG_MSG("INT:Gate to %X:%X big %d %s",selector,reg_eip,desc.Big(),gate.Type() & 0x8 ? "386" : "286");
				reg_eip=offset;
				return CPU_CheckState();
			}
		default:
			E_Exit("Illegal descriptor type %X for int %X",gate.Type(),num);
		}
	}
	return true;
}

bool CPU_IRET(bool use32) {
	if (!(cpu.state & STATE_PROTECTED)) {		/*RealMode IRET */
		if (use32) {
			reg_eip=CPU_Pop32();
			SegSet16(cs,CPU_Pop32());
			SETFLAGSw(CPU_Pop32());
		} else {
			reg_eip=CPU_Pop16();
			SegSet16(cs,CPU_Pop16());
			SETFLAGSw(CPU_Pop16());
		}
		return true;
	} else {	/* Protected mode IRET */
		if (GETFLAG(NT)) E_Exit("No task support");
		Bitu selector,offset,old_flags;
		if (use32) {
			offset=CPU_Pop32();
			selector=CPU_Pop32() & 0xffff;
			old_flags=CPU_Pop32();
			if (old_flags & FLAG_VM) E_Exit("No vm86 support");
		} else {
			offset=CPU_Pop16();
			selector=CPU_Pop16();
			old_flags=CPU_Pop16();
		}
		Bitu rpl=selector & 3;
		Descriptor desc;
		cpu.gdt.GetDescriptor(selector,desc);
		if (rpl<cpu.cpl) E_Exit("IRET to lower privilege");
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
			Segs.big[cs]=desc.Big();
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;;
			reg_eip=offset;
			SETFLAGSw(old_flags);
			LOG_MSG("IRET:Same level return to %X:%X",selector,offset);
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
			Segs.big[cs]=desc.Big();
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
			LOG_MSG("IRET:Outer level return to %X:%X",selector,offset);
		}
		return CPU_CheckState();
	}
	return false;
}

bool CPU_JMP(bool use32,Bitu selector,Bitu offset) {
	if (!(cpu.state & STATE_PROTECTED)) {
		if (!use32) {
			reg_eip=offset&0xffff;
		} else {
			reg_eip=offset;
		}
		SegSet16(cs,selector);
		return true;
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
			LOG_MSG("JMP:Code:NC to %X:%X",selector,offset);
			goto CODE_jmp;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			LOG_MSG("JMP:Code:C to %X:%X",selector,offset);
CODE_jmp:
			/* Normal jump to another selector:offset */
			Segs.phys[cs]=desc.GetBase();
			Segs.big[cs]=desc.Big();
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			reg_eip=offset;
			return CPU_CheckState();
		default:
			E_Exit("JMP Illegal descriptor type %X",desc.Type());

		}

	}
	return false;
}


bool CPU_CALL(bool use32,Bitu selector,Bitu offset) {
	if (!(cpu.state & STATE_PROTECTED)) {
		if (!use32) {
			CPU_Push16(SegValue(cs));
			CPU_Push16(reg_ip);
			reg_eip=offset&0xffff;
		} else {
			CPU_Push32(SegValue(cs));
			CPU_Push32(reg_eip);
			reg_eip=offset;
		}
		SegSet16(cs,selector);
		return true;
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
			LOG_MSG("CALL:CODE:NC to %X:%X",selector,offset);
			goto call_code;	
		case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
			if (call.DPL()>cpu.cpl) E_Exit("CALL:CODE:C:DPL>CPL");
			LOG_MSG("CALL:CODE:C to %X:%X",selector,offset);
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
			Segs.big[cs]=call.Big();
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			reg_eip=offset;
			return CPU_CheckState();
		default:
			E_Exit("CALL:Descriptor type %x unsupported",call.Type());

		};
		return CPU_CheckState();
	}
	return false;
}


bool CPU_RET(bool use32,Bitu bytes) {
	if (!(cpu.state & STATE_PROTECTED)) {
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
		return true;
	} else {
		Bitu offset,selector;
		if (!use32) {
			offset=CPU_Pop16();
			selector=CPU_Pop16();
		} else {
			offset=CPU_Pop32();
			selector=CPU_Pop32() & 0xffff;
		}
		if (cpu.state & STATE_STACK32) {
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
			Segs.big[cs]=desc.Big();
			Segs.val[cs]=selector;
			reg_eip=offset;
			LOG_MSG("RET - Same level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return CPU_CheckState();
		} else {
			/* Return to higher level */
			E_Exit("REturn to higher priviledge");
		}
		LOG_MSG("Prot ret %X:%X",selector,offset);
		return CPU_CheckState();
	}
	return false;
}


void CPU_SLDT(Bitu & selector) {
	selector=cpu.gdt.SLDT();
}

Bitu tr=0;
void CPU_LLDT(Bitu selector) {
	cpu.gdt.LLDT(selector);
	LOG_MSG("LDT Set to %X",selector);
}

void CPU_STR(Bitu & selector) {
	selector=tr;
}

void CPU_LTR(Bitu selector) {
	tr=selector;
	LOG_MSG("TR Set to %X",selector);
}


void CPU_LGDT(Bitu limit,Bitu base) {
	LOG_MSG("GDT Set to base:%X limit:%X",base,limit);
	cpu.gdt.SetLimit(limit);
	cpu.gdt.SetBase(base);
}

void CPU_LIDT(Bitu limit,Bitu base) {
	LOG_MSG("IDT Set to base:%X limit:%X",base,limit);
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
			if (value & CR0_PAGING) LOG_MSG("Paging enabled");
			if (value & CR0_PROTECTION) {
				LOG_MSG("Protected mode");
			} else {
				LOG_MSG("Real mode");
			}
			return CPU_CheckState();
		}
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
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, CR%d",cr);
		break;
	}
	return 0;
}


void CPU_SMSW(Bitu & word) {
	word=cpu.cr0 & 0xffff;
}

bool CPU_LMSW(Bitu word) {
	word&=0xffff;
	word|=(cpu.cr0&0xffff0000);
	return CPU_SET_CRX(0,word);
}

void CPU_ARPL(Bitu & dest_sel,Bitu src_sel) {
	flags.type=t_UNKNOWN;	
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
	flags.type=t_UNKNOWN;
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
	flags.type=t_UNKNOWN;
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
	flags.type=t_UNKNOWN;
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
	flags.type=t_UNKNOWN;
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


bool CPU_SetSegGeneral(SegNames seg,Bitu value) {
	Segs.val[seg]=value;
	if (cpu.state & STATE_PROTECTED) {
		Descriptor desc;
		cpu.gdt.GetDescriptor(value,desc);
		Segs.phys[seg]=desc.GetBase();
//		LOG_MSG("Segment %d Set with base %X limit %X",seg,desc.GetBase(),desc.GetLimit());
		if (seg==ss) {
			Segs.big[ss]=desc.Big();
			return CPU_CheckState();
		} else return true;
	} else {
		Segs.phys[seg]=value << 4;
		return true;
	} 
	return false;
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

static Bitu HLT_Decode(void) {
	/* Stay here just as long until an external interrupt has changed CS:EIP */
	if ((reg_eip!=cpu.hlt.eip) || (SegValue(cs)!=cpu.hlt.cs)) {
		cpu.state=0xff;			//force a new state to set decoder
		CPU_CheckState();
		return 0x0;
	}
	CPU_Cycles=0;
	return 0x0;
}

void CPU_HLT(void) {
	cpu.hlt.cs=SegValue(cs);
	cpu.hlt.eip=reg_eip;
	CPU_Cycles=0;
	cpudecoder=&HLT_Decode;
}


static void CPU_CycleIncrease(void) {
	Bitu old_cycles=CPU_CycleMax;
	CPU_CycleMax=(Bitu)(CPU_CycleMax*1.2);
	CPU_CycleLeft=0;CPU_Cycles=0;
	if (CPU_CycleMax==old_cycles) CPU_CycleMax++;
	LOG_MSG("CPU:%d cycles",CPU_CycleMax);
}

static void CPU_CycleDecrease(void) {
	CPU_CycleMax=(Bitu)(CPU_CycleMax/1.2);
	CPU_CycleLeft=0;CPU_Cycles=0;
	if (!CPU_CycleMax) CPU_CycleMax=1;
	LOG_MSG("CPU:%d cycles",CPU_CycleMax);
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
	flags.word=FLAG_IF;
	flags.type=t_UNKNOWN;

	cpu.full.entry=cpu.full.prefix=0;
	cpu.state=0xff;			//To initialize it the first time
	cpu.cr0=false;

	CPU_CheckState();

	KEYBOARD_AddEvent(KBD_f11,KBD_MOD_CTRL,CPU_CycleDecrease);
	KEYBOARD_AddEvent(KBD_f12,KBD_MOD_CTRL,CPU_CycleIncrease);

	CPU_Cycles=0;
	CPU_CycleMax=section->Get_int("cycles");;
	if (!CPU_CycleMax) CPU_CycleMax=1500;
	CPU_CycleLeft=0;

	MSG_Add("CPU_CONFIGFILE_HELP","The amount of cycles to execute each loop. Lowering this setting will slowdown dosbox\n");

}

