/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

/* $Id: cpu.cpp,v 1.48 2004-01-10 14:03:34 qbix79 Exp $ */

#include <assert.h>
#include "dosbox.h"
#include "cpu.h"
#include "memory.h"
#include "debug.h"
#include "keyboard.h"
#include "setup.h"
#include "paging.h"
#include "support.h"

Bitu DEBUG_EnableDebugger(void);

#if 1
#undef LOG
#define LOG(X,Y)
#endif

CPU_Regs cpu_regs;
CPUBlock cpu;
Segments Segs;

Bits CPU_Cycles = 0;
Bits CPU_CycleLeft = 0;
Bits CPU_CycleMax = 1800;
Bits CPU_CycleUp = 0;
Bits CPU_CycleDown = 0;
CPU_Decoder * cpudecoder;

static struct {
	Bitu which,errorcode;
} exception;

void CPU_Core_Full_Init(void);
void CPU_Core_Normal_Init(void);
void CPU_Core_Dyn_X86_Init(void);

#if (C_DYNAMIC_X86)

#define startcpu_core	CPU_Core_Dyn_X86_Run

#else 

#define startcpu_core	CPU_Core_Normal_Run
//#define startcpu_core	CPU_Core_Full_Run

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

void CPU_SetFlags(Bitu word,Bitu mask) {
	reg_flags=(reg_flags & ~mask)|(word & mask)|2;
}

class TaskStateSegment {
public:
	TaskStateSegment() {
		valid=false;
	}
	bool IsValid(void) {
		return valid;
	}
	Bitu Get_back(void) {
		return mem_readw(base);
	}
	void SaveSelector(void) {
		cpu.gdt.SetDescriptor(selector,desc);
	}
	void Get_SSx_ESPx(Bitu level,Bitu & _ss,Bitu & _esp) {
		if (is386) {
			PhysPt where=base+offsetof(TSS_32,esp0)+level*8;
			_esp=mem_readd(where);
			_ss=mem_readw(where+4);
		} else {
			PhysPt where=base+offsetof(TSS_16,sp0)+level*4;
			_esp=mem_readw(where);
			_ss=mem_readw(where+2);
		}
	}
	bool SetSelector(Bitu new_sel) {
		valid=false;
		selector=new_sel;
		if (!cpu.gdt.GetDescriptor(selector,desc)) return false;
		switch (desc.Type()) {
			case DESC_286_TSS_A:		case DESC_286_TSS_B:
			case DESC_386_TSS_A:		case DESC_386_TSS_B:
				break;
			default:
				valid=false;
				return false;
		}
		valid=true;
		base=desc.GetBase();
		limit=desc.GetLimit();
		is386=desc.Is386();
		return true;
	}
	TSS_Descriptor desc;
	Bitu selector;
	PhysPt base;
	Bitu limit;
	Bitu is386;
	bool valid;
};

TaskStateSegment cpu_tss;

enum TSwitchType {
	TSwitch_JMP,TSwitch_CALL_INT,TSwitch_IRET
};

bool CPU_SwitchTask(Bitu new_tss_selector,TSwitchType tstype) {
	TaskStateSegment new_tss;
	if (!new_tss.SetSelector(new_tss_selector)) 
		E_Exit("Illegal TSS for switch");
	/* Save current context in current TSS */
	/* Check if we need to clear busy bit of old TASK */
	if (tstype==TSwitch_JMP || tstype==TSwitch_IRET) {
		cpu_tss.desc.SetBusy(false);
		cpu_tss.SaveSelector();
	}
	Bitu new_cr3=0;
	Bitu new_es,new_cs,new_ss,new_ds,new_fs,new_gs;
	Bitu new_ldt;
	if (cpu_tss.is386) {
		mem_writed(cpu_tss.base+offsetof(TSS_32,eflags),reg_flags);
		mem_writed(cpu_tss.base+offsetof(TSS_32,eip),reg_eip);

		mem_writed(cpu_tss.base+offsetof(TSS_32,eax),reg_eax);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ecx),reg_ecx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,edx),reg_edx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ebx),reg_ebx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,esp),reg_esp);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ebp),reg_ebp);
		mem_writed(cpu_tss.base+offsetof(TSS_32,esi),reg_esi);
		mem_writed(cpu_tss.base+offsetof(TSS_32,edi),reg_edi);

		mem_writed(cpu_tss.base+offsetof(TSS_32,es),SegValue(es));
		mem_writed(cpu_tss.base+offsetof(TSS_32,cs),SegValue(cs));
		mem_writed(cpu_tss.base+offsetof(TSS_32,ss),SegValue(ss));
		mem_writed(cpu_tss.base+offsetof(TSS_32,ds),SegValue(ds));
		mem_writed(cpu_tss.base+offsetof(TSS_32,fs),SegValue(fs));
		mem_writed(cpu_tss.base+offsetof(TSS_32,gs),SegValue(gs));
	} else {
		E_Exit("286 task switch");
	}
	/* Load new context from new TSS */
	if (new_tss.is386) {
		new_cr3=mem_readd(new_tss.base+offsetof(TSS_32,cr3));
		reg_eip=mem_readd(new_tss.base+offsetof(TSS_32,eip));
		CPU_SetFlags(mem_readd(new_tss.base+offsetof(TSS_32,eflags)),FMASK_ALL | FLAG_VM);
		reg_eax=mem_readd(new_tss.base+offsetof(TSS_32,eax));
		reg_ecx=mem_readd(new_tss.base+offsetof(TSS_32,ecx));
		reg_edx=mem_readd(new_tss.base+offsetof(TSS_32,edx));
		reg_ebx=mem_readd(new_tss.base+offsetof(TSS_32,ebx));
		reg_esp=mem_readd(new_tss.base+offsetof(TSS_32,esp));
		reg_ebp=mem_readd(new_tss.base+offsetof(TSS_32,ebp));
		reg_edi=mem_readd(new_tss.base+offsetof(TSS_32,edi));
		reg_esi=mem_readd(new_tss.base+offsetof(TSS_32,esi));

		new_es=mem_readw(new_tss.base+offsetof(TSS_32,es));
		new_cs=mem_readw(new_tss.base+offsetof(TSS_32,cs));
		new_ss=mem_readw(new_tss.base+offsetof(TSS_32,ss));
		new_ds=mem_readw(new_tss.base+offsetof(TSS_32,ds));
		new_fs=mem_readw(new_tss.base+offsetof(TSS_32,fs));
		new_gs=mem_readw(new_tss.base+offsetof(TSS_32,gs));
		new_ldt=mem_readw(new_tss.base+offsetof(TSS_32,ldt));
	} else {
		E_Exit("286 task switch");
	}
	/* Setup a back link to the old TSS in new TSS */
	if (tstype==TSwitch_CALL_INT) {
		if (new_tss.is386) {
			mem_writed(new_tss.base+offsetof(TSS_32,back),cpu_tss.selector);
		} else {
			mem_writew(new_tss.base+offsetof(TSS_16,back),cpu_tss.selector);
		}
		/* And make the new task's eflag have the nested task bit */
		reg_flags|=FLAG_NT;
	}
	/* Set the busy bit in the new task */
	if (tstype==TSwitch_JMP || tstype==TSwitch_IRET) {
		new_tss.desc.SetBusy(true);
		new_tss.SaveSelector();
	}
	/* Setup the new cr3 */
	PAGING_SetDirBase(new_cr3);
	/* Load the new selectors */
	if (reg_flags & FLAG_VM) {
//		LOG_MSG("Entering v86 task");
		SegSet16(cs,new_cs);
		cpu.code.big=false;
		cpu.cpl=3;			//We don't have segment caches so this will do
	} else {
		//DEBUG_EnableDebugger();
		/* Protected mode task */
		CPU_LLDT(new_ldt);
		/* Load the new CS*/
		Descriptor cs_desc;
		cpu.cpl=new_cs & 3;
		cpu.gdt.GetDescriptor(new_cs,cs_desc);
		if (!cs_desc.saved.seg.p) {
			E_Exit("Task switch with non present code-segment");
			return false;
		}
		switch (cs_desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			if (cpu.cpl != cs_desc.DPL()) E_Exit("Task CS RPL != DPL");
			goto doconforming;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
doconforming:
			Segs.phys[cs]=cs_desc.GetBase();
			cpu.code.big=cs_desc.Big()>0;
			Segs.val[cs]=new_cs;
			break;
		default:
			E_Exit("Task switch CS Type %d",cs_desc.Type());
		}
	}
	CPU_SetSegGeneral(es,new_es);
	CPU_SetSegGeneral(ss,new_ss);
	CPU_SetSegGeneral(ds,new_ds);
	CPU_SetSegGeneral(fs,new_fs);
	CPU_SetSegGeneral(gs,new_gs);
	CPU_LTR(new_tss_selector);
//	LOG_MSG("Task CPL %X CS:%X IP:%X SS:%X SP:%X eflags %x",cpu.cpl,SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);
	return true;
}

void CPU_StartException(void) {
	CPU_Interrupt(cpu.exception.which,CPU_INT_EXCEPTION | ((cpu.exception.which>=8) ? CPU_INT_HAS_ERROR : 0));
}

void CPU_SetupException(Bitu which,Bitu error) {
	cpu.exception.which=which;
	cpu.exception.error=error;
}

void CPU_Exception(Bitu which,Bitu error ) {
//	LOG_MSG("Exception %d CS:%X IP:%X FLAGS:%X",num,SegValue(cs),reg_eip,reg_flags);
	CPU_SetupException(which,error);
	CPU_StartException();
}

Bit8u lastint;
void CPU_Interrupt(Bitu num,Bitu type,Bitu opLen) {
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
		if (DEBUG_Breakpoint()) {
			CPU_Cycles=0;
			return;
		}
	};
#endif
	if (!cpu.pmode) {
		/* Save everything on a 16-bit stack */
		CPU_Push16(reg_flags & 0xffff);
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		SETFLAGBIT(IF,false);
		SETFLAGBIT(TF,false);
		/* Get the new CS:IP from vector table */
		PhysPt base=cpu.idt.GetBase();
		reg_eip=mem_readw(base+(num << 2));
		Segs.val[cs]=mem_readw(base+(num << 2)+2);
		Segs.phys[cs]=Segs.val[cs]<<4;
		cpu.code.big=false;
		return;
	} else {
		/* Protected Mode Interrupt */
//		if (type&CPU_INT_SOFTWARE && cpu.v86) goto realmode_interrupt;
//		DEBUG_EnableDebugger();
//		LOG_MSG("interrupt start CPL %d v86 %d",cpu.cpl,cpu.v86);
		if ((reg_flags & FLAG_VM) && (type&CPU_INT_SOFTWARE)) {
//			LOG_MSG("Software int in v86, AH %X IOPL %x",reg_ah,(reg_flags & FLAG_IOPL) >>12);
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				reg_eip-=opLen;
				CPU_Exception(13,0);
				return;
			}
		} 
		Descriptor gate;
//TODO Check for software interrupt and check gate's dpl<cpl
		cpu.idt.GetDescriptor(num<<3,gate);
		if (type&CPU_INT_SOFTWARE && gate.DPL()<cpu.cpl) {
			reg_eip-=opLen;
			CPU_Exception(13,num*8+2);
			return;
		}
		switch (gate.Type()) {
		case DESC_286_INT_GATE:		case DESC_386_INT_GATE:
		case DESC_286_TRAP_GATE:	case DESC_386_TRAP_GATE:
			{
				Descriptor cs_desc;
				Bitu gate_sel=gate.GetSelector();
				Bitu gate_off=gate.GetOffset();
				cpu.gdt.GetDescriptor(gate_sel,cs_desc);
				Bitu cs_dpl=cs_desc.DPL();
				if (cs_dpl>cpu.cpl) E_Exit("Interrupt to higher privilege");
				switch (cs_desc.Type()) {
				case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
					if (cs_dpl<cpu.cpl) {
						/* Prepare for gate to inner level */
						Bitu n_ss,n_esp;
						Bitu o_ss,o_esp;
						o_ss=SegValue(ss);
						o_esp=reg_esp;
						cpu_tss.Get_SSx_ESPx(cs_dpl,n_ss,n_esp);
						Descriptor n_ss_desc;
						cpu.gdt.GetDescriptor(n_ss,n_ss_desc);
						Segs.phys[ss]=n_ss_desc.GetBase();
						Segs.val[ss]=n_ss;
						if (n_ss_desc.Big()) {
							cpu.stack.big=true;
							cpu.stack.mask=0xffffffff;
							reg_esp=n_esp;
						} else {
							cpu.stack.big=false;
							cpu.stack.mask=0xffff;
							reg_sp=n_esp;
						}
						if (gate.Type() & 0x8) {	/* 32-bit Gate */
							if (reg_flags & FLAG_VM) {
								CPU_Push32(SegValue(gs));SegSet16(gs,0x0);
								CPU_Push32(SegValue(fs));SegSet16(fs,0x0);
								CPU_Push32(SegValue(ds));SegSet16(ds,0x0);
								CPU_Push32(SegValue(es));SegSet16(es,0x0);
							}
							CPU_Push32(o_ss);
							CPU_Push32(o_esp);
						} else {					/* 16-bit Gate */
							if (reg_flags & FLAG_VM) E_Exit("V86 to 16-bit gate");
							CPU_Push16(o_ss);
							CPU_Push16(o_esp);
						}
//						LOG_MSG("INT:Gate to inner level SS:%X SP:%X",n_ss,n_esp);
						cpu.cpl=cs_dpl;
						goto do_interrupt;
					} 
				case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
				/* Prepare stack for gate to same priviledge */
					if (reg_flags & FLAG_VM) 
						E_Exit("V86 interrupt doesn't change to pl0");

do_interrupt:
					if (gate.Type() & 0x8) {	/* 32-bit Gate */
						CPU_Push32(reg_flags);
						CPU_Push32(SegValue(cs));
						CPU_Push32(reg_eip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push32(cpu.exception.error);
					} else {					/* 16-bit gate */
						CPU_Push16(reg_flags & 0xffff);
						CPU_Push16(SegValue(cs));
						CPU_Push16(reg_ip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push16(cpu.exception.error);
					}
					break;		
				default:
					E_Exit("INT:Gate Selector points to illegal descriptor with type %x",cs_desc.Type());
				}
				if (!(gate.Type()&1))
					SETFLAGBIT(IF,false);			
				SETFLAGBIT(TF,false);
				SETFLAGBIT(NT,false);
				SETFLAGBIT(VM,false);
				Segs.val[cs]=(gate_sel&0xfffc) | cpu.cpl;
				Segs.phys[cs]=cs_desc.GetBase();
				cpu.code.big=cs_desc.Big()>0;
				LOG(LOG_CPU,LOG_NORMAL)("INT:Gate to %X:%X big %d %s",gate_sel,gate_off,cs_desc.Big(),gate.Type() & 0x8 ? "386" : "286");
				reg_eip=gate_off;
				return;
			}
		case DESC_TASK_GATE:
			CPU_SwitchTask(gate.GetSelector(),TSwitch_CALL_INT);
			if (type & CPU_INT_HAS_ERROR) {
				//TODO Be sure about this, seems somewhat unclear
				if (cpu_tss.is386) CPU_Push32(cpu.exception.error);
				else CPU_Push16(cpu.exception.error);
			}
			return;
		default:
			E_Exit("Illegal descriptor type %X for int %X",gate.Type(),num);
		}
	}
	assert(1);
	return ; // make compiler happy
}

void CPU_IRET(bool use32) {
	if (!cpu.pmode) {					/* RealMode IRET */
realmode_iret:
		if (use32) {
			reg_eip=CPU_Pop32();
			SegSet16(cs,CPU_Pop32());
			CPU_SetFlagsd(CPU_Pop32());
		} else {
			reg_eip=CPU_Pop16();
			SegSet16(cs,CPU_Pop16());
			CPU_SetFlagsw(CPU_Pop16());
		}
		cpu.code.big=false;
		return;
	} else {	/* Protected mode IRET */
		if (reg_flags & FLAG_VM) {
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				reg_eip--;
				CPU_Exception(13,0);
				return;
			} else goto realmode_iret;
		}
//		DEBUG_EnableDebugger();
//		LOG_MSG("IRET start CPL %d v86 %d",cpu.cpl,cpu.v86);
		/* Check if this is task IRET */	
		if (GETFLAG(NT)) {
			if (GETFLAG(VM)) E_Exit("Pmode IRET with VM bit set");
			if (!cpu_tss.IsValid()) E_Exit("TASK Iret without valid TSS");
			Bitu back_link=cpu_tss.Get_back();
			CPU_SwitchTask(back_link,TSwitch_IRET);
			return;
		}
		Bitu n_cs_sel,n_eip,n_flags;
		if (use32) {
			n_eip=CPU_Pop32();
			n_cs_sel=CPU_Pop32() & 0xffff;
			n_flags=CPU_Pop32();
			if (n_flags & FLAG_VM) {
				cpu.cpl=3;
				CPU_SetFlags(n_flags,FMASK_ALL | FLAG_VM);
				Bitu n_ss,n_esp,n_es,n_ds,n_fs,n_gs;
				n_esp=CPU_Pop32();
				n_ss=CPU_Pop32() & 0xffff;

				n_es=CPU_Pop32() & 0xffff;
				n_ds=CPU_Pop32() & 0xffff;
				n_fs=CPU_Pop32() & 0xffff;
				n_gs=CPU_Pop32() & 0xffff;
				CPU_SetSegGeneral(ss,n_ss);
				CPU_SetSegGeneral(es,n_es);
				CPU_SetSegGeneral(ds,n_ds);
				CPU_SetSegGeneral(fs,n_fs);
				CPU_SetSegGeneral(gs,n_gs);
				reg_eip=n_eip & 0xffff;
				reg_esp=n_esp;
				cpu.code.big=false;
				SegSet16(cs,n_cs_sel);
				LOG(LOG_CPU,LOG_NORMAL)("IRET:Back to V86: CS:%X IP %X SS:%X SP %X FLAGS:%X",SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);	
				return;
			}
		} else {
			n_eip=CPU_Pop16();
			n_cs_sel=CPU_Pop16();
			n_flags=(reg_flags & 0xffff0000) | CPU_Pop16();
			if (n_flags & FLAG_VM) E_Exit("VM Flag in 16-bit iret");
		}
		Bitu n_cs_rpl=n_cs_sel & 3;
		Descriptor n_cs_desc;
		cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc);
		if (n_cs_rpl<cpu.cpl)
			E_Exit("IRET to lower privilege");
		if (n_cs_rpl==cpu.cpl) {	
			/* Return to same level */
			switch (n_cs_desc.Type()) {
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (!(cpu.cpl==n_cs_desc.DPL())) E_Exit("IRET:Same Level:NC:DPL!=CPL");
				break;
			case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
				if (!(n_cs_desc.DPL()>=cpu.cpl)) E_Exit("IRET:Same level:C:DPL<CPL");
				break;
			default:
				E_Exit("IRET:Same level:Illegal descriptor type %X",n_cs_desc.Type());
			}
			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;
			reg_eip=n_eip;
			CPU_SetFlagsd(n_flags);
			LOG(LOG_CPU,LOG_NORMAL)("IRET:Same level:%X:%X big %d",n_cs_sel,n_eip,cpu.code.big);
		} else {
			/* Return to outer level */
			switch (n_cs_desc.Type()) {
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (n_cs_desc.DPL()!=n_cs_rpl) E_Exit("IRET:Outer level:NC:CS RPL != CS DPL");
				break;
			case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
				if (n_cs_desc.DPL()<=cpu.cpl) E_Exit("IRET:Outer level:C:DPL <= CPL");
				break;
			default:
				E_Exit("IRET:Outer level:Illegal descriptor type %X",n_cs_desc.Type());
			}
			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;
			cpu.cpl=n_cs_rpl;
			reg_eip=n_eip;
			CPU_SetFlagsd(n_flags);
			Bitu n_ss,n_esp;
			if (use32) {
				n_esp=CPU_Pop32();
				n_ss=CPU_Pop32() & 0xffff;
			} else {
				n_esp=CPU_Pop16();
				n_ss=CPU_Pop16();
			}
			CPU_SetSegGeneral(ss,n_ss);
			if (cpu.stack.big) {
				reg_esp=n_esp;
			} else {
				reg_sp=n_esp;
			}
			//TODO Maybe validate other segments, but why would anyone use them?
			LOG(LOG_CPU,LOG_NORMAL)("IRET:Outer level:%X:X big %d",n_cs_sel,n_eip,cpu.code.big);
		}
		return;
	}
}

void CPU_JMP(bool use32,Bitu selector,Bitu offset,Bitu opLen) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		if (!use32) {
			reg_eip=offset&0xffff;
		} else {
			reg_eip=offset;
		}
		SegSet16(cs,selector);
		cpu.code.big=false;
		return;
	} else {
		Bitu rpl=selector & 3;
		Descriptor desc;
		cpu.gdt.GetDescriptor(selector,desc);
		if (!desc.saved.seg.p) {
			reg_eip -= opLen;
			CPU_Exception(0x0B,selector & 0xfffc);
			return;
		}
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
			return;
		case DESC_386_TSS_A:
			if (desc.DPL()<cpu.cpl) E_Exit("JMP:TSS:dpl<cpl");
			if (desc.DPL()<rpl) E_Exit("JMP:TSS:dpl<rpl");
			LOG(LOG_CPU,LOG_NORMAL)("JMP:TSS to %X",selector);
			CPU_SwitchTask(selector,TSwitch_JMP);
			break;
		default:
			E_Exit("JMP Illegal descriptor type %X",desc.Type());
		}
	}
	assert(1);
}



void CPU_CALL(bool use32,Bitu selector,Bitu offset,Bitu opLen) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
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
		return;
	} else {
		Descriptor call;
		Bitu rpl=selector & 3;
		cpu.gdt.GetDescriptor(selector,call);
		if (!call.saved.seg.p) {
			reg_eip -= opLen;
			CPU_Exception(0x0B,selector & 0xfffc);
			return;
		}
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
				reg_eip=offset & 0xffff;
			} else {
				CPU_Push32(SegValue(cs));
				CPU_Push32(reg_eip);
				reg_eip=offset;
			}
			Segs.phys[cs]=call.GetBase();
			cpu.code.big=call.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			return;
		case DESC_386_CALL_GATE: 
		case DESC_286_CALL_GATE:
			{
				if (call.DPL()<cpu.cpl) E_Exit("Call:Gate:Gate DPL<CPL");
				if (call.DPL()<rpl)		E_Exit("Call:Gate:Gate DPL<RPL");			
				Descriptor n_cs_desc;
				Bitu n_cs_sel=call.GetSelector();
				if (!cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc)) E_Exit("Call:Gate:Invalid CS selector.");
				if (!n_cs_desc.saved.seg.p) {
					reg_eip -= opLen;
					CPU_Exception(0x0B,selector & 0xfffc);
					return;
				}
				Bitu n_cs_dpl	= n_cs_desc.DPL();
				Bitu n_cs_rpl	= n_cs_sel & 3;
				Bitu n_eip		= call.GetOffset();
				switch (n_cs_desc.Type()) {
				case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
					/* Check if we goto innter priviledge */
					if (n_cs_dpl < cpu.cpl) {
						/* Get new SS:ESP out of TSS */
						Bitu n_ss_sel,n_esp;
						Descriptor n_ss_desc;
						cpu_tss.Get_SSx_ESPx(n_cs_dpl,n_ss_sel,n_esp);
						if (!cpu.gdt.GetDescriptor(n_ss_sel,n_ss_desc)) E_Exit("Call:Gate:Invalid SS selector.");
						/* New CPL is new SS DPL */
						cpu.cpl			= n_ss_desc.DPL();
						/* Load the new SS:ESP and save data on it */
						Bitu o_esp		= reg_esp;
						Bitu o_ss		= SegValue(ss);
						PhysPt o_stack  = SegPhys(ss)+(reg_esp & cpu.stack.mask);
					
						CPU_SetSegGeneral(ss,n_ss_sel);
						if (cpu.stack.big) 	reg_esp=n_esp;
						else reg_sp=n_esp;
						if (call.Type()==DESC_386_CALL_GATE) {
							CPU_Push32(o_ss);		//save old stack
							CPU_Push32(o_esp);
							for (Bitu i=0;i<(call.saved.gate.paramcount&31);i++) 
								CPU_Push32(mem_readd(o_stack+i*4));
							CPU_Push32(SegValue(cs));
							CPU_Push32(reg_eip);
						} else {
							CPU_Push16(o_ss);		//save old stack
							CPU_Push16(o_esp);
							for (Bitu i=0;i<(call.saved.gate.paramcount&31);i++) 
								CPU_Push16(mem_readw(o_stack+i*2));
							CPU_Push16(SegValue(cs));
							CPU_Push16(reg_eip);
						}

						/* Switch to new CS:EIP */
						Segs.phys[cs]	= n_cs_desc.GetBase();
						Segs.val[cs]	= (n_cs_sel & 0xfffc) | cpu.cpl;
						cpu.code.big	= n_cs_desc.Big()>0;
						reg_eip			= n_eip;
						if (!use32)	reg_eip&=0xffff;
						break;		
					}
				case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
call_gate_same_privilege:
					E_Exit("Call gate to same priviledge");
					break;
				}
			}			/* Call Gates */
			break;
		case DESC_386_TSS_A:
			if (call.DPL()<cpu.cpl) E_Exit("CALL:TSS:dpl<cpl");
			if (call.DPL()<rpl) E_Exit("CALL:TSS:dpl<rpl");
			LOG(LOG_CPU,LOG_NORMAL)("CALL:TSS to %X",selector);
			CPU_SwitchTask(selector,TSwitch_CALL_INT);
			break;
		default:
			E_Exit("CALL:Descriptor type %x unsupported",call.Type());

		}
	}
	assert(1);
}


void CPU_RET(bool use32,Bitu bytes,Bitu opLen) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
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
		return;
	} else {
		Bitu offset,selector;
		if (!use32) selector	= mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask) + 2);
		else 		selector	= mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask) + 4) & 0xffff;

		Descriptor desc;
		Bitu rpl=selector & 3;
		if (rpl<cpu.cpl) E_Exit("RET to lower privilege");
		cpu.gdt.GetDescriptor(selector,desc);

		if (!desc.saved.seg.p) {
			reg_eip -= opLen;
			CPU_Exception(0x0B,selector & 0xfffc);
			return;
		};

		if (!use32) {
			offset=CPU_Pop16();
			selector=CPU_Pop16();
		} else {
			offset=CPU_Pop32();
			selector=CPU_Pop32() & 0xffff;
		}
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
			if (cpu.stack.big) {
				reg_esp+=bytes;
			} else {
				reg_sp+=bytes;
			}
			LOG(LOG_CPU,LOG_NORMAL)("RET - Same level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return;
		} else {
			/* Return to outer level */
			if (bytes) E_Exit("RETF outeer level with immediate value");
			Bitu n_esp,n_ss;
			if (use32) {
				n_esp = CPU_Pop32();
				n_ss = CPU_Pop32() & 0xffff;
			} else {
				n_esp = CPU_Pop16();
				n_ss = CPU_Pop16();
			}
			cpu.cpl = rpl;
			CPU_SetSegGeneral(ss,n_ss);
			if (cpu.stack.big) {
                reg_esp = n_esp;
			} else {
				reg_sp = n_esp;
			}
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=selector;
			reg_eip=offset;
//			LOG(LOG_MISC,LOG_ERROR)("RET - Higher level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return;
		}
		LOG(LOG_CPU,LOG_NORMAL)("Prot ret %X:%X",selector,offset);
		return;
	}
	assert(1);
}


void CPU_SLDT(Bitu & selector) {
	selector=cpu.gdt.SLDT();
}

void CPU_LLDT(Bitu selector) {
	cpu.gdt.LLDT(selector);
	LOG(LOG_CPU,LOG_NORMAL)("LDT Set to %X",selector);
}

void CPU_STR(Bitu & selector) {
	selector=cpu_tss.selector;
}

void CPU_LTR(Bitu selector) {
	cpu_tss.SetSelector(selector);
}

Bitu gdt_count=0;

void CPU_LGDT(Bitu limit,Bitu base) {
	LOG(LOG_CPU,LOG_NORMAL)("GDT Set to base:%X limit:%X count %d",base,limit,gdt_count++);
	cpu.gdt.SetLimit(limit);
	cpu.gdt.SetBase(base);
//	if (gdt_count>20) DEBUG_EnableDebugger();
//	DEBUG_EnableDebugger();
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
	case 2:
		paging.cr2=value;
		break;
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
	case 2:
		return paging.cr2;
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
	word&=0xf;
	if (cpu.cr0 & 1) word|=1; 
	word|=(cpu.cr0&0xfffffff0);
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
	value &= 0xffff;
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		Segs.val[seg]=value;
		Segs.phys[seg]=value << 4;
		if (seg==ss) {
			cpu.stack.big=false;
			cpu.stack.mask=0xffff;
		}
		return false;
	} else {
		Descriptor desc;
		cpu.gdt.GetDescriptor(value,desc);

		if (value!=0) {
			if (!desc.saved.seg.p) {
				if (seg==ss) E_Exit("CPU_SetSegGeneral: Stack segment not present.");
				// Throw Exception 0x0B - Segment not present
				CPU_SetupException(0x0B,value & 0xfffc);
				return true;
			} else if (seg==ss) {
				// Stack segment loaded with illegal segment ?
				if ((desc.saved.seg.type<DESC_DATA_EU_RO_NA) || (desc.saved.seg.type>DESC_DATA_ED_RW_A)) {
					CPU_SetupException(0x0D,value & 0xfffc);
					return true;
				}
			}
		}
		Segs.val[seg]=value;
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
		return false;
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

static Bits HLT_Decode(void) {
	/* Once an interrupt occurs, it should change cpu core */
	if (reg_eip!=cpu.hlt.eip || SegValue(cs) != cpu.hlt.cs) {
		cpudecoder=cpu.hlt.old_decoder;
	} else {
		CPU_Cycles=0;
	}
	return 0;
}

void CPU_HLT(Bitu opLen) {
	if (cpu.cpl) {
		reg_eip-=opLen;
		CPU_Exception(13,0);	
		return;
	}
	CPU_Cycles=0;
	cpu.hlt.cs=SegValue(cs);
	cpu.hlt.eip=reg_eip;
	cpu.hlt.old_decoder=cpudecoder;
	cpudecoder=&HLT_Decode;
	return;
}


extern void GFX_SetTitle(Bits cycles ,Bits frameskip);
static void CPU_CycleIncrease(void) {
	Bits old_cycles=CPU_CycleMax;
	if(CPU_CycleUp < 100){
		CPU_CycleMax = (Bits)(CPU_CycleMax * (1 + (float)CPU_CycleUp / 100.0));
	} else {
		CPU_CycleMax = (Bits)(CPU_CycleMax + CPU_CycleUp);
	}
    
	CPU_CycleLeft=0;CPU_Cycles=0;
	if (CPU_CycleMax==old_cycles) CPU_CycleMax++;
	LOG_MSG("CPU:%d cycles",CPU_CycleMax);
	GFX_SetTitle(CPU_CycleMax,-1);
}

static void CPU_CycleDecrease(void) {
	if(CPU_CycleDown < 100){
		CPU_CycleMax = (Bits)(CPU_CycleMax / (1 + (float)CPU_CycleDown / 100.0));
	} else {
		CPU_CycleMax = (Bits)(CPU_CycleMax - CPU_CycleDown);
	}
	CPU_CycleLeft=0;CPU_Cycles=0;
	if (CPU_CycleMax <= 0) CPU_CycleMax=1;
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

	reg_flags=0x2;
	CPU_SetFlags(FLAG_IF,FMASK_ALL);		//Enable interrupts
	cpu.cr0=0xffffffff;
	CPU_SET_CRX(0,0);						//Initialize
	cpu.code.big=false;
	cpu.stack.mask=0xffff;
	cpu.stack.big=false;
	cpu.idt.SetBase(0);
	cpu.idt.SetLimit(1023);
	
	/* Init the cpu cores */
	CPU_Core_Normal_Init();
	CPU_Core_Full_Init();
#if (C_DYNAMIC_X86)
	CPU_Core_Dyn_X86_Init();
#endif

	KEYBOARD_AddEvent(KBD_f11,KBD_MOD_CTRL,CPU_CycleDecrease);
	KEYBOARD_AddEvent(KBD_f12,KBD_MOD_CTRL,CPU_CycleIncrease);

	CPU_Cycles=0;
	CPU_CycleMax=section->Get_int("cycles");;
	CPU_CycleUp=section->Get_int("cycleup");
	CPU_CycleDown=section->Get_int("cycledown");
	const char * core=section->Get_string("core");
	cpudecoder=&CPU_Core_Normal_Run;
	if (!strcasecmp(core,"normal")) {
		cpudecoder=&CPU_Core_Normal_Run;
	} else if (!strcasecmp(core,"full")) {
		cpudecoder=&CPU_Core_Full_Run;
	} 
#if (C_DYNAMIC_X86)
	else if (!strcasecmp(core,"dynamic")) {
		cpudecoder=&CPU_Core_Dyn_X86_Run;
	} 
#endif
	else {
		LOG_MSG("CPU:Unknown core type %s, switcing back to normal.",core);
	}
	CPU_JMP(false,0,0);					//Setup the first cpu core

	if (!CPU_CycleMax) CPU_CycleMax = 1800;
	if(!CPU_CycleUp)   CPU_CycleUp = 500;
	if(!CPU_CycleDown) CPU_CycleDown = 20;
	CPU_CycleLeft=0;
	GFX_SetTitle(CPU_CycleMax,-1);
}

