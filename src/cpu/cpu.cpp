/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

/* $Id: cpu.cpp,v 1.73 2005-08-15 13:43:44 c2woody Exp $ */

#include <assert.h>
#include "dosbox.h"
#include "cpu.h"
#include "memory.h"
#include "debug.h"
#include "mapper.h"
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
Bits CPU_CycleMax = 2500;
Bits CPU_CycleUp = 0;
Bits CPU_CycleDown = 0;
CPU_Decoder * cpudecoder;

void CPU_Core_Full_Init(void);
void CPU_Core_Normal_Init(void);
void CPU_Core_Simple_Init(void);
void CPU_Core_Dyn_X86_Init(void);


/* In debug mode exceptions are tested and dosbox exits when 
 * a unhandled exception state is detected. 
 * USE CHECK_EXCEPT to raise an exception in that case to see if that exception
 * solves the problem.
 * 
 * In non-debug mode dosbox doesn't do detection (and hence doesn't crash at
 * that point). (game might crash later due to the unhandled exception) */

#if C_DEBUG
// #define CPU_CHECK_EXCEPT 1
// #define CPU_CHECK_IGNORE 1
 /* Use CHECK_EXCEPT when something doesn't work to see if a exception is 
 * needed that isn't enabled by default.*/
#else
/* NORMAL NO CHECKING => More Speed */
#define CPU_CHECK_IGNORE 1
#endif /* C_DEBUG */

#if defined(CPU_CHECK_IGNORE)
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	cond;					\
}
#elif defined(CPU_CHECK_EXCEPT)
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) {					\
		CPU_Exception(exc,sel);		\
		return;				\
	}					\
}
#else
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) E_Exit(msg);			\
}
#endif


void CPU_Push16(Bitu value) {
	Bit32u new_esp=(reg_esp&~cpu.stack.mask)|((reg_esp-2)&cpu.stack.mask);
	mem_writew(SegPhys(ss) + (new_esp & cpu.stack.mask) ,value);
	reg_esp=new_esp;
}

void CPU_Push32(Bitu value) {
	Bit32u new_esp=(reg_esp&~cpu.stack.mask)|((reg_esp-4)&cpu.stack.mask);
	mem_writed(SegPhys(ss) + (new_esp & cpu.stack.mask) ,value);
	reg_esp=new_esp;
}

Bitu CPU_Pop16(void) {
	Bitu val=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp=(reg_esp&~cpu.stack.mask)|((reg_esp+2)&cpu.stack.mask);
	return val;
}

Bitu CPU_Pop32(void) {
	Bitu val=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp=(reg_esp&~cpu.stack.mask)|((reg_esp+4)&cpu.stack.mask);
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
	reg_flags=(reg_flags & ~mask)|(word & mask)|2|FLAG_ID;
	cpu.direction=1-((reg_flags & FLAG_DF) >> 9);
}

bool CPU_PrepareException(Bitu which,Bitu error) {
	cpu.exception.which=which;
	cpu.exception.error=error;
	return true;
}

bool CPU_CLI(void) {
	if (cpu.pmode && ((!GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) || (GETFLAG(VM) && (GETFLAG_IOPL<3)))) {
		return CPU_PrepareException(EXCEPTION_GP,0);
	} else {
		SETFLAGBIT(IF,false);
		return false;
	}
}

bool CPU_STI(void) {
	if (cpu.pmode && ((!GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) || (GETFLAG(VM) && (GETFLAG_IOPL<3)))) {
		return CPU_PrepareException(EXCEPTION_GP,0);
	} else {
 		SETFLAGBIT(IF,true);
		return false;
	}
}

bool CPU_POPF(Bitu use32) {
	if (cpu.pmode && GETFLAG(VM) && (GETFLAG(IOPL)!=FLAG_IOPL)) {
		/* Not enough privileges to execute POPF */
		return CPU_PrepareException(EXCEPTION_GP,0);
	}
	Bitu mask=FMASK_ALL;
	/* IOPL field can only be modified when CPL=0 or in real mode: */
	if (cpu.pmode && (cpu.cpl>0)) mask &= (~FLAG_IOPL);
	if (cpu.pmode && !GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) mask &= (~FLAG_IF);
	if (use32)
		CPU_SetFlags(CPU_Pop32(),mask);
	else CPU_SetFlags(CPU_Pop16(),mask & 0xffff);
	return false;
}

bool CPU_PUSHF(Bitu use32) {
	if (cpu.pmode && GETFLAG(VM) && (GETFLAG(IOPL)!=FLAG_IOPL)) {
		/* Not enough privileges to execute PUSHF */
		return CPU_PrepareException(EXCEPTION_GP,0);
	}
	if (use32) 
		CPU_Push32(reg_flags & 0xfcffff);
	else CPU_Push16(reg_flags);
	return false;
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
		if ((new_sel & 0xfffc)==0) {
			selector=0;
			base=0;
			limit=0;
			is386=1;
			return true;
		}
		if (new_sel&4) return false;
		if (!cpu.gdt.GetDescriptor(new_sel,desc)) return false;
		switch (desc.Type()) {
			case DESC_286_TSS_A:		case DESC_286_TSS_B:
			case DESC_386_TSS_A:		case DESC_386_TSS_B:
				break;
			default:
				return false;
		}
		if (!desc.saved.seg.p) return false;
		selector=new_sel;
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

bool CPU_SwitchTask(Bitu new_tss_selector,TSwitchType tstype,Bitu old_eip) {
	TaskStateSegment new_tss;
	if (!new_tss.SetSelector(new_tss_selector)) 
		E_Exit("Illegal TSS for switch, selector=%x, switchtype=%x",new_tss_selector,tstype);
	if (tstype==TSwitch_IRET) {
		if (!new_tss.desc.IsBusy())
			E_Exit("TSS not busy for IRET");
	} else {
		if (new_tss.desc.IsBusy())
			E_Exit("TSS busy for JMP/CALL/INT");
	}
	Bitu new_cr3=0;
	Bitu new_eax,new_ebx,new_ecx,new_edx,new_esp,new_ebp,new_esi,new_edi;
	Bitu new_es,new_cs,new_ss,new_ds,new_fs,new_gs;
	Bitu new_ldt,new_eip,new_eflags;
	/* Read new context from new TSS */
	if (new_tss.is386) {
		new_cr3=mem_readd(new_tss.base+offsetof(TSS_32,cr3));
		new_eip=mem_readd(new_tss.base+offsetof(TSS_32,eip));
		new_eflags=mem_readd(new_tss.base+offsetof(TSS_32,eflags));
		new_eax=mem_readd(new_tss.base+offsetof(TSS_32,eax));
		new_ecx=mem_readd(new_tss.base+offsetof(TSS_32,ecx));
		new_edx=mem_readd(new_tss.base+offsetof(TSS_32,edx));
		new_ebx=mem_readd(new_tss.base+offsetof(TSS_32,ebx));
		new_esp=mem_readd(new_tss.base+offsetof(TSS_32,esp));
		new_ebp=mem_readd(new_tss.base+offsetof(TSS_32,ebp));
		new_edi=mem_readd(new_tss.base+offsetof(TSS_32,edi));
		new_esi=mem_readd(new_tss.base+offsetof(TSS_32,esi));

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

	/* Check if we need to clear busy bit of old TASK */
	if (tstype==TSwitch_JMP || tstype==TSwitch_IRET) {
		cpu_tss.desc.SetBusy(false);
		cpu_tss.SaveSelector();
	}
	Bit32u old_flags = reg_flags;
	if (tstype==TSwitch_IRET) old_flags &= (~FLAG_NT);

	/* Save current context in current TSS */
	if (cpu_tss.is386) {
		mem_writed(cpu_tss.base+offsetof(TSS_32,eflags),old_flags);
		mem_writed(cpu_tss.base+offsetof(TSS_32,eip),old_eip);

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

	/* Setup a back link to the old TSS in new TSS */
	if (tstype==TSwitch_CALL_INT) {
		if (new_tss.is386) {
			mem_writed(new_tss.base+offsetof(TSS_32,back),cpu_tss.selector);
		} else {
			mem_writew(new_tss.base+offsetof(TSS_16,back),cpu_tss.selector);
		}
		/* And make the new task's eflag have the nested task bit */
		new_eflags|=FLAG_NT;
	}
	/* Set the busy bit in the new task */
	if (tstype==TSwitch_JMP || tstype==TSwitch_CALL_INT) {
		new_tss.desc.SetBusy(true);
		new_tss.SaveSelector();
	}

//	cpu.cr0|=CR0_TASKSWITCHED;
	if (new_tss_selector == cpu_tss.selector) {
		reg_eip = old_eip;
		new_cs = SegValue(cs);
		new_ss = SegValue(ss);
		new_ds = SegValue(ds);
		new_es = SegValue(es);
		new_fs = SegValue(fs);
		new_gs = SegValue(gs);
	} else {
	
		/* Setup the new cr3 */
		PAGING_SetDirBase(new_cr3);

		/* Load new context */
		if (new_tss.is386) {
			reg_eip=new_eip;
			CPU_SetFlags(new_eflags,FMASK_ALL | FLAG_VM);
			reg_eax=new_eax;
			reg_ecx=new_ecx;
			reg_edx=new_edx;
			reg_ebx=new_ebx;
			reg_esp=new_esp;
			reg_ebp=new_ebp;
			reg_edi=new_edi;
			reg_esi=new_esi;

//			new_cs=mem_readw(new_tss.base+offsetof(TSS_32,cs));
		} else {
			E_Exit("286 task switch");
		}
	}
	/* Load the new selectors */
	if (reg_flags & FLAG_VM) {
//		LOG_MSG("Entering v86 task");
		SegSet16(cs,new_cs);
		cpu.code.big=false;
		cpu.cpl=3;			//We don't have segment caches so this will do
	} else {
		/* Protected mode task */
		if (new_ldt!=0) CPU_LLDT(new_ldt);
		/* Load the new CS*/
		Descriptor cs_desc;
		cpu.cpl=new_cs & 3;
		if (!cpu.gdt.GetDescriptor(new_cs,cs_desc))
			E_Exit("Task switch with CS beyond limits");
		if (!cs_desc.saved.seg.p)
			E_Exit("Task switch with non present code-segment");
		switch (cs_desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			if (cpu.cpl != cs_desc.DPL()) E_Exit("Task CS RPL != DPL");
			goto doconforming;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			if (cpu.cpl < cs_desc.DPL()) E_Exit("Task CS RPL < DPL");
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
	if (!cpu_tss.SetSelector(new_tss_selector)) LOG(LOG_CPU,LOG_NORMAL)("TaskSwitch: set tss selector %X failed",new_tss_selector);
	cpu_tss.desc.SetBusy(true);
//	LOG_MSG("Task CPL %X CS:%X IP:%X SS:%X SP:%X eflags %x",cpu.cpl,SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);
	return true;
}

bool CPU_IO_Exception(Bitu port,Bitu size) {
	if (cpu.pmode && ((GETFLAG_IOPL<cpu.cpl) || GETFLAG(VM))) {
		if (!cpu_tss.is386) goto doexception;
		PhysPt where=cpu_tss.base+0x66;
		Bitu ofs=mem_readw(where);
		if (ofs>cpu_tss.limit) goto doexception;
		where=cpu_tss.base+ofs+(port/8);
		Bitu map=mem_readw(where);
		Bitu mask=(0xffff>>(16-size)) << (port&7);
		if (map & mask) goto doexception;
	}
	return false;
doexception:
	LOG(LOG_CPU,LOG_NORMAL)("IO Exception port %X",port);
	return CPU_PrepareException(EXCEPTION_GP,0);
}

void CPU_Exception(Bitu which,Bitu error ) {
//	LOG_MSG("Exception %d error %x",which,error);
	cpu.exception.error=error;
	CPU_Interrupt(which,CPU_INT_EXCEPTION | ((which>=8) ? CPU_INT_HAS_ERROR : 0),reg_eip);
}

const char* translateVXDid(Bitu nr) {
	switch (nr) {
		case 0x0001: return "VMM";		// Virtual Machine Manager
		case 0x0002: return "DEBUG";
		case 0x0003: return "VPICD";	// Virtual PIC Device
		case 0x0004: return "VDMAD";	// Virtual DMA Device
		case 0x0005: return "VTD";		// Virtual Timer Device
		case 0x0006: return "V86MMGR";	// Virtual Device
		case 0x0007: return "PageSwap";
		case 0x000A: return "VDD";		// Virtual Display Device
		case 0x000C: return "VMD";		// Virtual Mouse Device
		case 0x000D: return "VKD";		// Virtual Keyboard Device
		case 0x000E: return "VCD";		// Virtual COMM Device
		case 0x0010: return "IOS";		// BlockDev / IOS
		case 0x0011: return "VMCPD";
		case 0x0012: return "EBIOS";
		case 0x0015: return "DOSMGR";
		case 0x0017: return "SHELL";
		case 0x0018: return "VMPoll";
		case 0x001A: return "DOSNET";
		case 0x001B: return "VFD";
		case 0x001C: return "LoadHi";
		case 0x0020: return "Int13";
		case 0x0021: return "PAGEFILE";
		case 0x0026: return "VPOWERD";
		case 0x0027: return "VXDLDR";
		case 0x002A: return "VWIN32";
		case 0x002B: return "VCOMM";
		case 0x0033: return "CONFIGMG";
		case 0x0036: return "VFBACKUP";
		case 0x0037: return "VMINI";
		case 0x0040: return "IFSMgr";
		case 0x0041: return "VCDFSD";
		case 0x0048: return "PERF";
		case 0x011F: return "VFLATD";
		case 0x0446: return "VADLIB";
		case 0x044a: return "mmdevldr";
		case 0x0484: return "IfsMgr";
		case 0x048B: return "VCACHE";
		case 0x357E: return "DSOUND";
		default:
			return "unknown";
	}
}

const char* translateVXDservice(Bitu nr, Bitu sv) {
	switch (nr) {
		case 0x0001:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "get current VM handle";
				case 0x0002	: return "test current VM handle";
				case 0x0003	: return "get system VM handle";
				case 0x0004	: return "test system VM handle";
				case 0x0005	: return "validate VM handle";
				case 0x0006	: return "get VMM reenter count";
				case 0x0007	: return "begin reentrant execution";
				case 0x0008	: return "end reentrant execution";
				case 0x0009	: return "install V86 breakpoint";
				case 0x000A	: return "remove V86 breakpoint";
				case 0x000B	: return "allocate V86 callback";
				case 0x000C	: return "allocation PM callback";
				case 0x000D	: return "call when VM returns";
				case 0x000E	: return "schedule global event";
				case 0x000F	: return "schedule VM event";
				case 0x0010	: return "call global event";
				case 0x0011	: return "call VM event";
				case 0x0012	: return "cancel global event";
				case 0x0013	: return "cancel VM event";
				case 0x0014	: return "call priority VM event";
				case 0x0015	: return "cancel priority VM event";
				case 0x0016	: return "get NMI handler address";
				case 0x0017	: return "set NMI handler address";
				case 0x0018	: return "hook NMI event";
				case 0x0019	: return "call when VM interrupts enabled";
				case 0x001A	: return "enable VM interrupts";
				case 0x001B	: return "disable VM interrupts";
				case 0x001C	: return "map flat";
				case 0x001D	: return "map linear to VM address";
				case 0x001E	: return "adjust execution priority";
				case 0x001F	: return "begin critical section";
				case 0x0020	: return "end critical section";
				case 0x0021	: return "end critical section and suspend";
				case 0x0022	: return "claim critical section";
				case 0x0023	: return "release critical section";
				case 0x0024	: return "call when not critical";
				case 0x0025	: return "create semaphore";
				case 0x0026	: return "destroy semaphore";
				case 0x0027	: return "wait on semaphore";
				case 0x0028	: return "signal semaphore";
				case 0x0029	: return "get critical section status";
				case 0x002A	: return "call when task switched";
				case 0x002B	: return "suspend VM";
				case 0x002C	: return "resume VM";
				case 0x002D	: return "no-fail resume VM";
				case 0x002E	: return "nuke VM";
				case 0x002F	: return "crash current VM";
				case 0x0030	: return "get execution focus";
				case 0x0031	: return "set execution focus";
				case 0x0032	: return "get time slice priority";
				case 0x0033	: return "set time slice priority";
				case 0x0034	: return "get time slice granularity";
				case 0x0035	: return "set time slice granularity";
				case 0x0036	: return "get time slice information";
				case 0x0037	: return "adjust execution time";
				case 0x0038	: return "release time slice";
				case 0x0039	: return "wake up VM";
				case 0x003A	: return "call when idle";
				case 0x003B	: return "get next VM handle";
				case 0x003C	: return "set global timeout";
				case 0x003D	: return "set VM timeout";
				case 0x003E	: return "cancel timeout";
				case 0x003F	: return "get system time";
				case 0x0040	: return "get VM execution time";
				case 0x0041	: return "hook V86 interrupt chain";
				case 0x0042	: return "get V86 interrupt vector";
				case 0x0043	: return "set V86 interrupt vector";
				case 0x0044	: return "get PM interrupt vector";
				case 0x0045	: return "set PM interrupt vector";
				case 0x0046	: return "simulate interrupt";
				case 0x0047	: return "simulate IRET";
				case 0x0048	: return "simulate far call";
				case 0x0049	: return "simulate far jump";
				case 0x004A	: return "simulate far RET";
				case 0x004B	: return "simulate far RET N";
				case 0x004C	: return "build interrupt stack frame";
				case 0x004D	: return "simulate push";
				case 0x004E	: return "simulate pop";
				case 0x004F	: return "_HeapAllocate";
				case 0x0050	: return "_HeapReAllocate";
				case 0x0051	: return "_HeapFree";
				case 0x0052	: return "_HeapGetSize";
				case 0x0053	: return "_PageAllocate";
				case 0x0054	: return "_PageReAllocate";
				case 0x0055	: return "_PageFree";
				case 0x0056	: return "_PageLock";
				case 0x0057	: return "_PageUnLock";
				case 0x0058	: return "_PageGetSizeAddr";
				case 0x0059	: return "_PageGetAllocInfo";
				case 0x005A	: return "_GetFreePageCount";
				case 0x005B	: return "_GetSysPageCount";
				case 0x005C	: return "_GetVMPgCount";
				case 0x005D	: return "_MapIntoV86";
				case 0x005E	: return "_PhysIntoV86";
				case 0x005F	: return "_TestGlobalV86Mem";
				case 0x0060	: return "_ModifyPageBits";
				case 0x0061	: return "copy page table";
				case 0x0062	: return "map linear into V86";
				case 0x0063	: return "linear page lock";
				case 0x0064	: return "linear page unlock";
				case 0x0065	: return "_SetResetV86Pageabl";
				case 0x0066	: return "_GetV86PageableArray";
				case 0x0067	: return "_PageCheckLinRange";
				case 0x0068	: return "page out dirty pages";
				case 0x0069	: return "discard pages";
				case 0x006A	: return "_GetNulPageHandle";
				case 0x006B	: return "get first V86 page";
				case 0x006C	: return "map physical address to linear address";
				case 0x006D	: return "_GetAppFlatDSAlias";
				case 0x006E	: return "_SelectorMapFlat";
				case 0x006F	: return "_GetDemandPageInfo";
				case 0x0070	: return "_GetSetPageOutCount";
				case 0x0071	: return "hook V86 page";
				case 0x0072	: return "assign device V86 pages";
				case 0x0073	: return "deassign device V86 pages";
				case 0x0074	: return "get array of V86 pages for device";
				case 0x0075	: return "_SetNULPageAddr";
				case 0x0076	: return "allocate GDT selector";
				case 0x0077	: return "free GDT selector";
				case 0x0078	: return "allocate LDT selector";
				case 0x0079	: return "free LDT selector";
				case 0x007A	: return "_BuildDescriptorDWORDs";
				case 0x007B	: return "get descriptor";
				case 0x007C	: return "set descriptor";
				case 0x007D	: return "toggle HMA";
				case 0x007E	: return "get fault hook addresses";
				case 0x007F	: return "hook V86 fault";
				case 0x0080	: return "hook PM fault";
				case 0x0081	: return "hook VMM fault";
				case 0x0082	: return "begin nested V86 execution";
				case 0x0083	: return "begin nested execution";
				case 0x0084	: return "execute V86-mode interrupt";
				case 0x0085	: return "resume execution";
				case 0x0086	: return "end nested execution";
				case 0x0087	: return "allocate PM application callback area";
				case 0x0088	: return "get current PM application callback area";
				case 0x0089	: return "set V86 execution mode";
				case 0x008A	: return "set PM execution mode";
				case 0x008B	: return "begin using locked PM stack";
				case 0x008C	: return "end using locked PM stack";
				case 0x008D	: return "save client state";
				case 0x008E	: return "restore client state";
				case 0x008F	: return "execute VxD interrupt";
				case 0x0090	: return "hook device service";
				case 0x0091	: return "hook device V86 API";
				case 0x0092	: return "hook device PM API";
				case 0x0093	: return "system control (see also #02657)";
				case 0x0094	: return "simulate I/O";
				case 0x0095	: return "install multiple I/O handlers";
				case 0x0096	: return "install I/O handler";
				case 0x0097	: return "enable global trapping";
				case 0x0098	: return "enable local trapping";
				case 0x0099	: return "disable global trapping";
				case 0x009A	: return "disable local trapping";
				case 0x009B	: return "create list";
				case 0x009C	: return "destroy list";
				case 0x009D	: return "allocate list";
				case 0x009E	: return "attach list";
				case 0x009F	: return "attach list tail";
				case 0x00A0	: return "insert into list";
				case 0x00A1	: return "remove from list";
				case 0x00A2	: return "deallocate list";
				case 0x00A3	: return "get first item in list";
				case 0x00A4	: return "get next item in list";
				case 0x00A5	: return "remove first item in list";
				case 0x00A6	: return "add instance item";
				case 0x00A7	: return "allocate device callback area";
				case 0x00A8	: return "allocate global V86 data area";
				case 0x00A9	: return "allocate temporary V86 data area";
				case 0x00AA	: return "free temporary V86 data area";
				case 0x00AB	: return "get decimal integer from profile";
				case 0x00AC	: return "convert decimal string to integer";
				case 0x00AD	: return "get fixed-point number from profile";
				case 0x00AE	: return "convert fixed-point string";
				case 0x00AF	: return "get hex integer from profile";
				case 0x00B0	: return "convert hex string to integer";
				case 0x00B1	: return "get boolean value from profile";
				case 0x00B2	: return "convert boolean string";
				case 0x00B3	: return "get string from profile";
				case 0x00B4	: return "get next string from profile";
				case 0x00B5	: return "get environment string";
				case 0x00B6	: return "get exec path";
				case 0x00B7	: return "get configuration directory";
				case 0x00B8	: return "open file";
				case 0x00B9	: return "get PSP segment";
				case 0x00BA	: return "get DOS vectors";
				case 0x00BB	: return "get machine information";
				case 0x00BC	: return "get/set HMA information";
				case 0x00BD	: return "set system exit code";
				case 0x00BE	: return "fatal error handler";
				case 0x00BF	: return "fatal memory error";
				case 0x00C0	: return "update system clock";
				case 0x00C1	: return "test if debugger installed";
				case 0x00C2	: return "output debugger string";
				case 0x00C3	: return "output debugger character";
				case 0x00C4	: return "input debugger character";
				case 0x00C5	: return "debugger convert hex to binary";
				case 0x00C6	: return "debugger convert hex to decimal";
				case 0x00C7	: return "debugger test if valid handle";
				case 0x00C8	: return "validate client pointer";
				case 0x00C9	: return "test reentry";
				case 0x00CA	: return "queue debugger string";
				case 0x00CB	: return "log procedure call";
				case 0x00CC	: return "debugger test current VM";
				case 0x00CD	: return "get PM interrupt type";
				case 0x00CE	: return "set PM interrupt type";
				case 0x00CF	: return "get last updated system time";
				case 0x00D0	: return "get last updated VM execution time";
				case 0x00D1	: return "test if double-byte character-set lead byte";
				case 0x00D2	: return "_AddFreePhysPage";
				case 0x00D3	: return "_PageResetHandlePAddr";
				case 0x00D4	: return "_SetLastV86Page";
				case 0x00D5	: return "_GetLastV86Page";
				case 0x00D6	: return "_MapFreePhysReg";
				case 0x00D7	: return "_UnmapFreePhysReg";
				case 0x00D8	: return "_XchgFreePhysReg";
				case 0x00D9	: return "_SetFreePhysRegCalBk";
				case 0x00DA	: return "get next arena (MCB)";
				case 0x00DB	: return "get name of ugly TSR";
				case 0x00DC	: return "get debug options";
				case 0x00DD	: return "set physical HMA alias";
				case 0x00DE	: return "_GetGlblRng0V86IntBase";
				case 0x00DF	: return "add global V86 data area";
				case 0x00E0	: return "get/set detailed VM error";
				case 0x00E1	: return "Is_Debug_Chr";
				case 0x00E2	: return "clear monochrome screen";
				case 0x00E3	: return "output character to mono screen";
				case 0x00E4	: return "output string to mono screen";
				case 0x00E5	: return "set current position on mono screen";
				case 0x00E6	: return "get current position on mono screen";
				case 0x00E7	: return "get character from mono screen";
				case 0x00E8	: return "locate byte in ROM";
				case 0x00E9	: return "hook invalid page fault";
				case 0x00EA	: return "unhook invalid page fault";
				case 0x00EB	: return "set delete on exit file";
				case 0x00EC	: return "close VM";
				case 0x00ED	: return "Enable_Touch_1st_Meg";
				case 0x00EE	: return "Disable_Touch_1st_Meg";
				case 0x00EF	: return "install exception handler";
				case 0x00F0	: return "remove exception handler";
				case 0x00F1	: return "Get_Crit_Status_No_Block";
				case 0x00F2	: return "_Schedule_VM_RTI_Event";
				case 0x00F3	: return "_Trace_Out_Service";
				case 0x00F4	: return "_Debug_Out_Service";
				case 0x00F5	: return "_Debug_Flags_Service";
				case 0x00F6	: return "VMM add import module name";
				case 0x00F7	: return "VMM Add DDB";
				case 0x00F8	: return "VMM Remove DDB";
				case 0x00F9	: return "get thread time slice priority";
				case 0x00FA	: return "set thread time slice priority";
				case 0x00FB	: return "schedule thread event";
				case 0x00FC	: return "cancel thread event";
				case 0x00FD	: return "set thread timeout";
				case 0x00FE	: return "set asynchronous timeout";
				case 0x00FF	: return "_AllocatreThreadDataSlot";
				case 0x0100	: return "_FreeThreadDataSlot";
				case 0x0101	: return "create Mutex";
				case 0x0102	: return "destroy Mutex";
				case 0x0103	: return "get Mutex owner";
				case 0x0104	: return "call when thread switched";
				case 0x0105	: return "create thread";
				case 0x0106	: return "start thread";
				case 0x0107	: return "terminate thread";
				case 0x0108	: return "get current thread handle";
				case 0x0109	: return "test current thread handle";
				case 0x010A	: return "Get_Sys_Thread_Handle";
				case 0x010B	: return "Test_Sys_Thread_Handle";
				case 0x010C	: return "Validate_Thread_Handle";
				case 0x010D	: return "Get_Initial_Thread_Handle";
				case 0x010E	: return "Test_Initial_Thread_Handle";
				case 0x010F	: return "Debug_Test_Valid_Thread_Handle";
				case 0x0110	: return "Debug_Test_Cur_Thread";
				case 0x0111	: return "VMM_GetSystemInitState";
				case 0x0112	: return "Cancel_Call_When_Thread_Switched";
				case 0x0113	: return "Get_Next_Thread_Handle";
				case 0x0114	: return "Adjust_Thread_Exec_Priority";
				case 0x0115	: return "_Deallocate_Device_CB_Area";
				case 0x0116	: return "Remove_IO_Handler";
				case 0x0117	: return "Remove_Mult_IO_Handlers";
				case 0x0118	: return "unhook V86 interrupt chain";
				case 0x0119	: return "unhook V86 fault handler";
				case 0x011A	: return "unhook PM fault handler";
				case 0x011B	: return "unhook VMM fault handler";
				case 0x011C	: return "unhook device service";
				case 0x011D	: return "_PageReserve";
				case 0x011E	: return "_PageCommit";
				case 0x011F	: return "_PageDecommit";
				case 0x0120	: return "_PagerRegister";
				case 0x0121	: return "_PagerQuery";
				case 0x0122	: return "_PagerDeregister";
				case 0x0123	: return "_ContextCreate";
				case 0x0124	: return "_ContextDestroy";
				case 0x0125	: return "_PageAttach";
				case 0x0126	: return "_PageFlush";
				case 0x0127	: return "_SignalID";
				case 0x0128	: return "_PageCommitPhys";
				case 0x0129	: return "_Register_Win32_Services";
				case 0x012A	: return "Cancel_Call_When_Not_Critical";
				case 0x012B	: return "Cancel_Call_When_Idle";
				case 0x012C	: return "Cancel_Call_When_Task_Switched";
				case 0x012D	: return "_Debug_Printf_Service";
				case 0x012E	: return "enter Mutex";
				case 0x012F	: return "leave Mutex";
				case 0x0130	: return "simulate VM I/O";
				case 0x0131	: return "Signal_Semaphore_No_Switch";
				case 0x0132	: return "_MMSwitchContext";
				case 0x0133	: return "_MMModifyPermissions";
				case 0x0134	: return "_MMQuery";
				case 0x0135	: return "_EnterMustComplete";
				case 0x0136	: return "_LeaveMustComplete";
				case 0x0137	: return "_ResumeExecMustComplete";
				case 0x0138	: return "get thread termination status";
				case 0x0139	: return "_GetInstanceInfo";
				case 0x013A	: return "_ExecIntMustComplete";
				case 0x013B	: return "_ExecVxDIntMustComplete";
				case 0x013C	: return "begin V86 serialization";
				case 0x013D	: return "unhook V86 page";
				case 0x013E	: return "VMM_GetVxDLocationList";
				case 0x013F	: return "VMM_GetDDBList: get start of VxD chain";
				case 0x0140	: return "unhook NMI event";
				case 0x0141	: return "Get_Instanced_V86_Int_Vector";
				case 0x0142	: return "get or set real DOS PSP";
				case 0x0143	: return "call priority thread event";
				case 0x0144	: return "Get_System_Time_Address";
				case 0x0145	: return "Get_Crit_Status_Thread";
				case 0x0146	: return "Get_DDB";
				case 0x0147	: return "Directed_Sys_Control";
				case 0x0148	: return "_RegOpenKey";
				case 0x0149	: return "_RegCloseKey";
				case 0x014A	: return "_RegCreateKey";
				case 0x014B	: return "_RegDeleteKey";
				case 0x014C	: return "_RegEnumKey";
				case 0x014D	: return "_RegQueryValue";
				case 0x014E	: return "_RegSetValue";
				case 0x014F	: return "_RegDeleteValue";
				case 0x0150	: return "_RegEnumValue";
				case 0x0151	: return "_RegQueryValueEx";
				case 0x0152	: return "_RegSetValueEx";
				case 0x0153	: return "_CallRing3";
				case 0x0154	: return "Exec_PM_Int";
				case 0x0155	: return "_RegFlushKey";
				case 0x0156	: return "_PageCommitContig";
				case 0x0157	: return "_GetCurrentContext";
				case 0x0158	: return "_LocalizeSprintf";
				case 0x0159	: return "_LocalizeStackSprintf";
				case 0x015A	: return "Call_Restricted_Event";
				case 0x015B	: return "Cancel_Restricted_Event";
				case 0x015C	: return "Register_PEF_Provider";
				case 0x015D	: return "_GetPhysPageInfo";
				case 0x015E	: return "_RegQueryInfoKey";
				case 0x015F	: return "MemArb_Reserve_Pages";
				case 0x0160	: return "Time_Slice_Sys_VM_Idle";
				case 0x0161	: return "Time_Slice_Sleep";
				case 0x0162	: return "Boost_With_Decay";
				case 0x0163	: return "Set_Inversion_Pri";
				case 0x0164	: return "Reset_Inversion_Pri";
				case 0x0165	: return "Release_Inversion_Pri";
				case 0x0166	: return "Get_Thread_Win32_Pri";
				case 0x0167	: return "Set_Thread_Win32_Pri";
				case 0x0168	: return "Set_Thread_Static_Boost";
				case 0x0169	: return "Set_VM_Static_Boost";
				case 0x016A	: return "Release_Inversion_Pri_ID";
				case 0x016B	: return "Attach_Thread_To_Group";
				case 0x016C	: return "Detach_Thread_From_Group";
				case 0x016D	: return "Set_Group_Static_Boost";
				case 0x016E	: return "_GetRegistryPath";
				case 0x016F	: return "_GetRegistryKey";
				case 0x0170	: return "_CleanupNestedExec";
				case 0x0171	: return "_RegRemapPreDefKey";
				case 0x0172	: return "End_V86_Serialization";
				case 0x0173	: return "_Assert_Range";
				case 0x0174	: return "_Sprintf";
				case 0x0175	: return "_PageChangePager";
				case 0x0176	: return "_RegCreateDynKey";
				case 0x0177	: return "RegQMulti";
				case 0x0178	: return "Boost_Thread_With_VM";
				case 0x0179	: return "Get_Boot_Flags";
				case 0x017A	: return "Set_Boot_Flags";
				case 0x017B	: return "_lstrcpyn";
				case 0x017C	: return "_lstrlen";
				case 0x017D	: return "_lmemcpy";
				case 0x017E	: return "_GetVxDName";
				case 0x017F	: return "Force_Mutexes_Free";
				case 0x0180	: return "Restore_Forced_Mutexes";
				case 0x0181	: return "_AddReclaimableItem";
				case 0x0182	: return "_SetReclaimableItem";
				case 0x0183	: return "_EnumReclaimableItem";
				case 0x0184	: return "Time_Slice_Wake_Sys_VM";
				case 0x0185	: return "VMM_Replace_Global_Environment";
				case 0x0186	: return "Begin_Non_Serial_Nest_V86_Exec";
				case 0x0187	: return "Get_Nest_Exec_Status";
				case 0x0188	: return "Open_Boot_Log";
				case 0x0189	: return "Write_Boot_Log";
				case 0x018A	: return "Close_Boot_Log";
				case 0x018B	: return "EnableDisable_Boot_Log";
				case 0x018C	: return "_Call_On_My_Stack";
				case 0x018D	: return "Get_Inst_V86_Int_Vec_Base";
				case 0x018E	: return "_lstrcmpi";
				case 0x018F	: return "_strupr";
				case 0x0190	: return "Log_Fault_Call_Out";
				case 0x0191	: return "_AtEventTime";
				default: return "dunnoSV";
			}
		case 0x0002:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "DEBUG_Fault";
				case 0x0002	: return "DEBUG_CheckFault";
				case 0x0003	: return "DEBUG_LoadSyms";
				default: return "dunnoSV";
			}
		case 0x0003:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "virtualize IRQ";
				case 0x0002	: return "set interrupt request";
				case 0x0003	: return "clear interrupt request";
				case 0x0004	: return "physical EOI";
				case 0x0005	: return "get complete status";
				case 0x0006	: return "get status";
				case 0x0007	: return "test physical request";
				case 0x0008	: return "physically mask";
				case 0x0009	: return "physically unmask";
				case 0x000A	: return "set automatic masking";
				case 0x000B	: return "get IRQ complete status";
				case 0x000C	: return "convert handle to IRQ";
				case 0x000D	: return "convert IRQ to interrupt";
				case 0x000E	: return "convert interrupt to IRQ";
				case 0x000F	: return "call on hardware interrupt";
				case 0x0010	: return "force default owner";
				case 0x0011	: return "force default behavior";
				case 0x0012	: return "VPICD_Auto_Mask_At_Inst_Swap";
				case 0x0013	: return "VPICD_Begin_Inst_Page_Swap";
				case 0x0014	: return "VPICD_End_Inst_Page_Swap";
				case 0x0015	: return "VPICD_Virtual_EOI";
				case 0x0016	: return "VPICD_Get_Virtualization_Count";
				case 0x0017	: return "VPICD_Post_Sys_Critical_Init";
				case 0x0018	: return "VPICD_VM_SlavePIC_Mask_Change";
				default: return "dunnoSV";
			}
 		case 0x0004:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "virtualize channel";
				case 0x0002	: return "get region information";
				case 0x0003	: return "set region information";
				case 0x0004	: return "get virtual state";
				case 0x0005	: return "set virtual state";
				case 0x0006	: return "set physical state";
				case 0x0007	: return "mask channel";
				case 0x0008	: return "unmask channel";
				case 0x0009	: return "lock DMA region";
				case 0x000A	: return "unlock DMA region";
				case 0x000B	: return "scatter lock";
				case 0x000C	: return "scatter unlock";
				case 0x000D	: return "reserve buffer space";
				case 0x000E	: return "request buffer";
				case 0x000F	: return "release buffer";
				case 0x0010	: return "copy to buffer";
				case 0x0011	: return "copy from buffer";
				case 0x0012	: return "default handler";
				case 0x0013	: return "disable translation";
				case 0x0014	: return "enable translation";
				case 0x0015	: return "get EISA address mode";
				case 0x0016	: return "set EISA address mode";
				case 0x0017	: return "unlock DMA region (ND)";
				case 0x0018	: return "VDMAD_Phys_Mask_Channel";
				case 0x0019	: return "VDMAD_Phys_Unmask_Channel";
				case 0x001A	: return "VDMAD_Unvirtualize_Channel";
				case 0x001B	: return "VDMAD_Set_IO_Address";
				case 0x001C	: return "VDMAD_Get_Phys_Count";
				case 0x001D	: return "VDMAD_Get_Phys_Status";
				case 0x001E	: return "VDMAD_Get_Max_Phys_Page";
				case 0x001F	: return "VDMAD_Set_Channel_Callbacks";
				case 0x0020	: return "VDMAD_Get_Virt_Count";
				case 0x0021	: return "VDMAD_Set_Virt_Count";
				default: return "dunnoSV";
			}
		case 0x0005:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "update system clock";
				case 0x0002	: return "get interrupt period";
				case 0x0003	: return "begin minimum interrupt period";
				case 0x0004	: return "end minimum interrupt period";
				case 0x0005	: return "disable trapping";
				case 0x0006	: return "enable trapping";
				case 0x0007	: return "get real time";
				case 0x0008	: return "VTD_Get_Date_And_Time";
				case 0x0009	: return "VTD_Adjust_VM_Count";
				case 0x000A	: return "VTD_Delay";
				default: return "dunnoSV";
			}
 		case 0x0006:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "allocate V86 pages";
				case 0x0002	: return "set EMS and XMS limits";
				case 0x0003	: return "get EMS and XMS limits";
				case 0x0004	: return "set mapping information";
				case 0x0005	: return "get mapping information";
				case 0x0006	: return "Xlat API";
				case 0x0007	: return "load client pointer";
				case 0x0008	: return "allocate buffer";
				case 0x0009	: return "free buffer";
				case 0x000A	: return "get Xlat buffer state";
				case 0x000B	: return "set Xlat buffer state";
				case 0x000C	: return "get VM flat selector";
				case 0x000D	: return "map pages";
				case 0x000E	: return "free page map region";
				case 0x000F	: return "_LocalGlobalReg";
				case 0x0010	: return "get page status";
				case 0x0011	: return "set local A20";
				case 0x0012	: return "reset base pages";
				case 0x0013	: return "set available mapped pages";
				case 0x0014	: return "V86MMGR_NoUMBInitCalls";
				case 0x0015	: return "V86MMGR_Get_EMS_XMS_Avail";
				case 0x0016	: return "V86MMGR_Toggle_HMA";
				case 0x0017	: return "V86MMGR_Dev_Init";
				case 0x0018	: return "V86MMGR_Alloc_UM_Page";
				default: return "dunnoSV";
			}
  		case 0x0007:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "test create";
				case 0x0002	: return "create swap file";
				case 0x0003	: return "destroy swap file";
				case 0x0004	: return "in";
				case 0x0005	: return "out";
				case 0x0006	: return "test if I/O valid";
				case 0x0007	: return "Read_Or_Write";
				case 0x0008	: return "Grow_File";
				case 0x0009	: return "Init_File";
				default: return "dunnoSV";
			}
 		case 0x000a:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "PIF state";
				case 0x0002	: return "get GrabRtn";
				case 0x0003	: return "hide cursor";
				case 0x0004	: return "set VM type";
				case 0x0005	: return "get ModTime";
				case 0x0006	: return "set HCurTrk";
				case 0x0007	: return "message clear screen";
				case 0x0008	: return "message foreground color";
				case 0x0009	: return "message background color";
				case 0x000A	: return "message output text";
				case 0x000B	: return "message set cursor position";
				case 0x000C	: return "query access";
				case 0x000D	: return "VDD_Check_Update_Soon";
				case 0x000E	: return "VDD_Get_Mini_Dispatch_Table";
				case 0x000F	: return "VDD_Register_Virtual_Port";
				case 0x0010	: return "VDD_Get_VM_Info";
				case 0x0011	: return "VDD_Get_Special_VM_IDs";
				case 0x0012	: return "VDD_Register_Extra_Screen_Selector";
				case 0x0013	: return "VDD_Takeover_VGA_Port";
				default: return "dunnoSV";
			}
 		case 0x000c:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "set mouse type";
				case 0x0002	: return "get mouse owner";
				case 0x0003	: return "VMOUSE_Post_Pointer_Message";
				case 0x0004	: return "VMOUSE_Set_Cursor_Proc";
				case 0x0005	: return "VMOUSE_Call_Cursor_Proc";
				case 0x0006	: return "VMOUSE_Set_Mouse_Data~Get_Mouse_Data";
				case 0x0007	: return "VMOUSE_Manipulate_Pointer_Message";
				case 0x0008	: return "VMOUSE_Set_Middle_Button";
				case 0x0009	: return "VMD_Set_Middle_Button";
				case 0x000A	: return "VMD_Enable_Disable_Mouse_Events";
				case 0x000B	: return "VMD_Post_Absolute_Pointer_Message";
				default: return "dunnoSV";
			}
 		case 0x000d:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "define hotkey";
				case 0x0002	: return "remove hotkey";
				case 0x0003	: return "locally enable hotkey";
				case 0x0004	: return "locally disable hotkey";
				case 0x0005	: return "reflect hotkey";
				case 0x0006	: return "cancel hotkey state";
				case 0x0007	: return "force keys";
				case 0x0008	: return "get keyboard owner";
				case 0x0009	: return "define paste mode";
				case 0x000A	: return "start pasting";
				case 0x000B	: return "cancel paste";
				case 0x000C	: return "get message key";
				case 0x000D	: return "peek message key";
				case 0x000E	: return "flush message key queue";
				case 0x000F : return "VKD_Enable_Keyboard";
				case 0x0010	: return "VKD_Disable_Keyboard";
				case 0x0011	: return "VKD_Get_Shift_State";
				case 0x0012	: return "VKD_Filter_Keyboard_Input";
				case 0x0013	: return "VKD_Put_Byte";
				case 0x0014	: return "VKD_Set_Shift_State";
				default: return "dunnoSV";
			}
		case 0x000e:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "set port global";
				case 0x0002	: return "get focus";
				case 0x0003	: return "virtualize port";
				case 0x0004	: return "VCD_Acquire_Port";
				case 0x0005	: return "VCD_Free_Port";
				case 0x0006	: return "VCD_Acquire_Port_Windows_Style";
				case 0x0007	: return "VCD_Free_Port_Windows_Style";
				case 0x0008	: return "VCD_Steal_Port_Windows_Style";
				case 0x0009	: return "VCD_Find_COM_Index";
				case 0x000A	: return "VCD_Set_Port_Global_Special";
				case 0x000B	: return "VCD_Virtualize_Port_Dynamic";
				case 0x000C	: return "VCD_Unvirtualize_Port_Dynamic";
				default: return "dunnoSV";
			}
 		case 0x0010:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "register device";
				case 0x0002	: return "find INT 13 drive";
				case 0x0003	: return "get device list";
				case 0x0004	: return "send command";
				case 0x0005	: return "command complete";
				case 0x0006	: return "synchronous command";
				case 0x0007	: return "IOS_Register";
				case 0x0008	: return "IOS_Requestor_Service";
				case 0x0009	: return "IOS_Exclusive_Access";
				case 0x000A	: return "IOS_Send_Next_Command";
				case 0x000B	: return "IOS_Set_Async_Time_Out";
				case 0x000C	: return "IOS_Signal_Semaphore_No_Switch";
				case 0x000D	: return "IOSIdleStatus";
				case 0x000E	: return "IOSMapIORSToI24";
				case 0x000F	: return "IOSMapIORSToI21";
				case 0x0010	: return "PrintLog";
				default: return "dunnoSV";
			}
 		case 0x0011:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get_version";
				case 0x0001	: return "get_virt_state";
				case 0x0002	: return "set_virt_state";
				case 0x0003	: return "get_cr0_state";
				case 0x0004	: return "set_cr0_state";
				case 0x0005	: return "get_thread_state";
				case 0x0006	: return "set_thread_state";
				case 0x0007	: return "get_FP_instruction_size";
				case 0x0008	: return "set_thread_precision";
				default: return "dunnoSV";
			}
 		case 0x0012:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get EBIOS version";
				case 0x0001	: return "get unused memory";
				default: return "dunnoSV";
			}
		case 0x0015:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "set exec VM data";
				case 0x0002	: return "coyp VM drive state";
				case 0x0003	: return "execute VM";
				case 0x0004	: return "get InDOS pointer";
				case 0x0005	: return "add device";
				case 0x0006	: return "remove device";
				case 0x0007	: return "instance device";
				case 0x0008	: return "get DOS critical status";
				case 0x0009	: return "enable InDOS polling";
				case 0x000A	: return "backfill allowed";
				case 0x000B	: return "LocalGlobalReg";
				case 0x000C	: return "Init_UMB_Area";
				case 0x000D	: return "Begin_V86_App";
				case 0x000E	: return "End_V86_App";
				case 0x000F	: return "Alloc_Local_Sys_VM_Mem";
				case 0x0010	: return "DOSMGR_Grow_CDSs";
				case 0x0011	: return "DOSMGR_Translate_Server_DOS_Call";
				case 0x0012	: return "DOSMGR_MMGR_PSP_Change_Notifier";
				default: return "dunnoSV";
			}
		case 0x0017:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "resolve contention";
				case 0x0002	: return "event";
				case 0x0003	: return "SYSMODAL message";
				case 0x0004	: return "message";
				case 0x0005	: return "get VM information";
				case 0x0006	: return "_SHELL_PostMessage";
				case 0x0007	: return "_SHELL_WinExec";
				case 0x0008	: return "_SHELL_CallDll";
				case 0x0009	: return "SHELL_OpenClipboard";
				case 0x000A	: return "SHELL_SetClipboardData";
				case 0x000B	: return "SHELL_GetClipboardData";
				case 0x000C	: return "SHELL_CloseClipboard";
				case 0x000D	: return "_SHELL_Install_Taskman_Hooks";
				case 0x000E	: return "SHELL_Hook_Properties";
				case 0x000F	: return "SHELL_Unhook_Properties";
				case 0x0010	: return "SHELL_OEMKeyScan";
				case 0x0011	: return "SHELL_Update_User_Activity";
				case 0x0012	: return "_SHELL_UnhookSystemBroadcast";
				case 0x0013	: return "_SHELL_LocalAllocEx";
				case 0x0014	: return "_SHELL_LocalFree";
				case 0x0015	: return "_SHELL_LoadLibrary";
				case 0x0016	: return "_SHELL_FreeLibrary";
				case 0x0017	: return "_SHELL_GetProcAddress";
				case 0x0018	: return "_SHELL_CallDll";
				case 0x0019	: return "_SHELL_SuggestSingleMSDOSMode";
				case 0x001A	: return "SHELL_CheckHotkeyAllowed";
				case 0x001B	: return "_SHELL_GetDOSAppInfo";
				default: return "dunnoSV";
			}
		case 0x0018:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "enable/disable";
				case 0x0002	: return "reset detection";
				case 0x0003	: return "check idle";
				default: return "dunnoSV";
			}
		case 0x001a:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "send FILESYSCHANGE";
				case 0x0002	: return "do PSP adjust";
				default: return "dunnoSV";
			}
		case 0x001b:
		case 0x001c:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				default: return "dunnoSV";
			}
 		case 0x0021:
			switch (sv & 0x7fff) {
				case 0x0000	: return "get version";
				case 0x0001	: return "init file";
				case 0x0002	: return "clean up";
				case 0x0003	: return "grow file";
				case 0x0004	: return "read or write";
				case 0x0005	: return "cancel";
				case 0x0006	: return "test I/O valid";
				case 0x0007	: return "Get_Size_Info";
				case 0x0008	: return "Set_Async_Manager";
				case 0x0009	: return "Call_Async_Manager";
				default: return "dunnoSV";
			}
 		case 0x0027:
			switch (sv & 0x7fff) {
				case 0x0000	: return "VXDLDR_Get_Version";
				case 0x0001	: return "VXDLDR_LoadDevice";
				case 0x0002	: return "VXDLDR_UnloadDevice";
				case 0x0003	: return "VXDLDR_DevInitSucceeded";
				case 0x0004	: return "VXDLDR_DevInitFailed";
				case 0x0005	: return "VXDLDR_GetDeviceList";
				case 0x0006	: return "VXDLDR_UnloadMe";
				case 0x0007	: return "PELDR_LoadModule";
				case 0x0008	: return "PELDR_GetModuleHandle";
				case 0x0009	: return "PELDR_GetModuleUsage";
				case 0x000A	: return "PELDR_GetEntryPoint";
				case 0x000B	: return "PELDR_GetProcAddress";
				case 0x000C	: return "PELDR_AddExportTable";
				case 0x000D	: return "PELDR_RemoveExportTable";
				case 0x000E	: return "PELDR_FreeModule";
				case 0x000F	: return "VXDLDR_Notify";
				case 0x0010	: return "_PELDR_InitCompleted";
				case 0x0011	: return "_PELDR_LoadModuleEx";
				default: return "dunnoSV";
			}
 		case 0x002A:
			switch (sv & 0x7fff) {
				case 0x0000 : return "VWin32_Get_Version";
				case 0x0001 : return "VWin32_Wake_For_Event";
				case 0x0002 : return "_VWIN32_QueueUserApc";
				case 0x0003 : return "_VWIN32_Get_Thread_Context";
				case 0x0004 : return "_VWIN32_Set_Thread_Context";
				case 0x0005 : return "_VWIN32_CopyMem";
				case 0x0006 : return "_VWIN32_BlockForTermination";
				case 0x0007 : return "_VWIN32_Emulate_Npx";
				case 0x0008 : return "_VWIN32_CheckDelayedNpxTrap";
				case 0x0009 : return "VWIN32_EnterCrstR0";
				case 0x000A : return "VWIN32_LeaveCrstR0";
				case 0x000B : return "_VWIN32_FaultPopup";
				case 0x000C : return "VWIN32_GetContextHandle";
				case 0x000D : return "VWIN32_GetCurrentProcessHandle";
				case 0x000E : return "_VWIN32_SetWin32Event";
				case 0x000F : return "_VWIN32_PulseWin32Event";
				case 0x0010 : return "_VWIN32_ResetWin32Event";
				case 0x0011 : return "_VWIN32_WaitSingleObject";
				case 0x0012 : return "_VWIN32_WaitMultipleObjects";
				case 0x0013 : return "_VWIN32_CreateRing0Thread";
				case 0x0014 : return "_VWIN32_CloseVxDHandle";
				case 0x0015 : return "VWIN32_ActiveTimeBiasSet";
				case 0x0016 : return "VWIN32_GetCurrentDirectory";
				case 0x0017 : return "VWIN32_BlueScreenPopup";
				case 0x0018 : return "VWIN32_TerminateApp";
				case 0x0019 : return "_VWIN32_QueueKernelAPC";
				case 0x001A : return "VWIN32_SysErrorBox";
				case 0x001B : return "_VWIN32_IsClientWin32";
				case 0x001C : return "VWIN32_IFSRIPWhenLev2Taken";
				default: return "dunnoSV";
			}
 		case 0x002b:
			switch (sv & 0x7fff) {
				case 0x0000	: return "VCOMM_Get_Version";
				case 0x0001	: return "_VCOMM_Register_Port_Driver";
				case 0x0002	: return "_VCOMM_Acquire_Port";
				case 0x0003	: return "_VCOMM_Release_Port";
				case 0x0004	: return "_VCOMM_OpenComm";
				case 0x0005	: return "_VCOMM_SetCommState";
				case 0x0006	: return "_VCOMM_GetCommState";
				case 0x0007	: return "_VCOMM_SetupComm";
				case 0x0008	: return "_VCOMM_TransmitCommChar";
				case 0x0009	: return "_VCOMM_CloseComm";
				case 0x000A	: return "_VCOMM_GetCommQueueStatus";
				case 0x000B	: return "_VCOMM_ClearCommError";
				case 0x000C	: return "_VCOMM_GetModemStatus";
				case 0x000D	: return "_VCOMM_GetCommProperties";
				case 0x000E	: return "_VCOMM_EscapeCommFunction";
				case 0x000F	: return "_VCOMM_PurgeComm";
				case 0x0010	: return "_VCOMM_SetCommEventMask";
				case 0x0011	: return "_VCOMM_GetCommEventMask";
				case 0x0012	: return "_VCOMM_WriteComm";
				case 0x0013	: return "_VCOMM_ReadComm";
				case 0x0014	: return "_VCOMM_EnableCommNotification";
				case 0x0015	: return "_VCOMM_GetLastError";
				case 0x0016	: return "_VCOMM_Steal_Port";
				case 0x0017	: return "_VCOMM_SetReadCallBack";
				case 0x0018	: return "_VCOMM_SetWriteCallBack";
				case 0x0019	: return "_VCOMM_GetSetCommTimeouts";
				case 0x001A	: return "_VCOMM_SetWriteRequest";
				case 0x001B	: return "_VCOMM_SetReadRequest";
				case 0x001C	: return "_VCOMM_Dequeue_Request";
				case 0x001D	: return "_VCOMM_Dequeue_Request";
				case 0x001E	: return "_VCOMM_Enumerate_DevNodes";
				case 0x001F	: return "VCOMM_Map_Win32DCB_To_Ring0";
				case 0x0020	: return "VCOMM_Map_Ring0DCB_To_Win32";
				case 0x0021	: return "_VCOMM_Get_Contention_Handler";
				case 0x0022	: return "_VCOMM_Map_Name_To_Resource";
				default: return "dunnoSV";
			}
 		case 0x0033:
			switch (sv & 0x7fff) {
				case 0x0000	: return "_CONFIGMG_Get_Version";
				case 0x0001	: return "_CONFIGMG_Initialize";
				case 0x0002	: return "_CONFIGMG_Locate_DevNode";
				case 0x0003	: return "_CONFIGMG_Get_Parent";
				case 0x0004	: return "_CONFIGMG_Get_Child";
				case 0x0005	: return "_CONFIGMG_Get_Sibling";
				case 0x0006	: return "_CONFIGMG_Get_Device_ID_Size";
				case 0x0007	: return "_CONFIGMG_Get_Device_ID";
				case 0x0008	: return "_CONFIGMG_Get_Depth";
				case 0x0009	: return "_CONFIGMG_Get_Private_DWord";
				case 0x000A	: return "_CONFIGMG_Set_Private_DWord";
				case 0x000B	: return "_CONFIGMG_Create_DevNode";
				case 0x000C	: return "_CONFIGMG_Query_Remove_SubTree";
				case 0x000D	: return "_CONFIGMG_Remove_SubTree";
				case 0x000E	: return "_CONFIGMG_Register_Device_Driver";
				case 0x000F	: return "_CONFIGMG_Register_Enumerator";
				case 0x0010 : return "_CONFIGMG_Register_Arbitrator";
				case 0x0011	: return "_CONFIGMG_Deregister_Arbitrator";
				case 0x0012	: return "_CONFIGMG_Query_Arbitrator_Free_Size";
				case 0x0013	: return "_CONFIGMG_Query_Arbitrator_Free_Data";
				case 0x0014	: return "_CONFIGMG_Sort_NodeList";
				case 0x0015	: return "_CONFIGMG_Yield";
				case 0x0016	: return "_CONFIGMG_Lock";
				case 0x0017	: return "_CONFIGMG_Unlock";
				case 0x0018	: return "_CONFIGMG_Add_Empty_Log_Conf";
				case 0x0019	: return "_CONFIGMG_Free_Log_Conf";
				case 0x001A	: return "_CONFIGMG_Get_First_Log_Conf";
				case 0x001B	: return "_CONFIGMG_Get_Next_Log_Conf";
				case 0x001C	: return "_CONFIGMG_Add_Res_Des";
				case 0x001D	: return "_CONFIGMG_Modify_Res_Des";
				case 0x001E	: return "_CONFIGMG_Free_Res_Des";
				case 0x001F	: return "_CONFIGMG_Get_Next_Res_Des";
				case 0x0020	: return "_CONFIGMG_Get_Performance_Info";
				case 0x0021	: return "_CONFIGMG_Get_Res_Des_Data_Size";
				case 0x0022	: return "_CONFIGMG_Get_Res_Des_Data";
				case 0x0023	: return "_CONFIGMG_Process_Events_Now";
				case 0x0024	: return "_CONFIGMG_Create_Range_List";
				case 0x0025	: return "_CONFIGMG_Add_Range";
				case 0x0026	: return "_CONFIGMG_Delete_Range";
				case 0x0027	: return "_CONFIGMG_Test_Range_Available";
				case 0x0028	: return "_CONFIGMG_Dup_Range_List";
				case 0x0029	: return "_CONFIGMG_Free_Range_List";
				case 0x002A	: return "_CONFIGMG_Invert_Range_List";
				case 0x002B	: return "_CONFIGMG_Intersect_Range_List";
				case 0x002C	: return "_CONFIGMG_First_Range";
				case 0x002D	: return "_CONFIGMG_Next_Range";
				case 0x002E	: return "_CONFIGMG_Dump_Range_List";
				case 0x002F	: return "_CONFIGMG_Load_DLVxDs";
				case 0x0030	: return "_CONFIGMG_Get_DDBs";
				case 0x0031	: return "_CONFIGMG_Get_CRC_CheckSum";
				case 0x0032	: return "_CONFIGMG_Register_DevLoader";
				case 0x0033	: return "_CONFIGMG_Reenumerate_DevNode";
				case 0x0034	: return "_CONFIGMG_Setup_DevNode";
				case 0x0035	: return "_CONFIGMG_Reset_Children_Marks";
				case 0x0036	: return "_CONFIGMG_Get_DevNode_Status";
				case 0x0037	: return "_CONFIGMG_Remove_Unmarked_Children";
				case 0x0038	: return "_CONFIGMG_ISAPNP_To_CM";
				case 0x0039	: return "_CONFIGMG_CallBack_Device_Driver";
				case 0x003A	: return "_CONFIGMG_CallBack_Enumerator";
				case 0x003B	: return "_CONFIGMG_Get_Alloc_Log_Conf";
				case 0x003C	: return "_CONFIGMG_Get_DevNode_Key_Size";
				case 0x003D	: return "_CONFIGMG_Get_DevNode_Key";
				case 0x003E	: return "_CONFIGMG_Read_Registry_Value";
				case 0x003F	: return "_CONFIGMG_Write_Registry_Value";
				case 0x0040	: return "_CONFIGMG_Disable_DevNode";
				case 0x0041	: return "_CONFIGMG_Enable_DevNode";
				case 0x0042	: return "_CONFIGMG_Move_DevNode";
				case 0x0043	: return "_CONFIGMG_Set_Bus_Info";
				case 0x0044	: return "_CONFIGMG_Get_Bus_Info";
				case 0x0045	: return "_CONFIGMG_Set_HW_Prof";
				case 0x0046	: return "_CONFIGMG_Recompute_HW_Prof";
				case 0x0047	: return "_CONFIGMG_Query_Change_HW_Prof";
				case 0x0048	: return "_CONFIGMG_Get_Device_Driver_Private_DWord";
				case 0x0049	: return "_CONFIGMG_Set_Device_Driver_Private_DWord";
				case 0x004A	: return "_CONFIGMG_Get_HW_Prof_Flags";
				case 0x004B	: return "_CONFIGMG_Set_HW_Prof_Flags";
				case 0x004C	: return "_CONFIGMG_Read_Registry_Log_Confs";
				case 0x004D	: return "_CONFIGMG_Run_Detection";
				case 0x004E	: return "_CONFIGMG_Call_At_Appy_Time";
				case 0x004F	: return "_CONFIGMG_Fail_Change_HW_Prof";
				case 0x0050	: return "_CONFIGMG_Set_Private_Problem";
				case 0x0051	: return "_CONFIGMG_Debug_DevNode";
				case 0x0052	: return "_CONFIGMG_Get_Hardware_Profile_Info";
				case 0x0053	: return "_CONFIGMG_Register_Enumerator_Function";
				case 0x0054	: return "_CONFIGMG_Call_Enumerator_Function";
				case 0x0055	: return "_CONFIGMG_Add_ID";
				case 0x0056	: return "_CONFIGMG_Find_Range";
				case 0x0057	: return "_CONFIGMG_Get_Global_State";
				case 0x0058	: return "_CONFIGMG_Broadcast_Device_Change_Message";
				case 0x0059	: return "_CONFIGMG_Call_DevNode_Handler";
				case 0x005A	: return "_CONFIGMG_Remove_Reinsert_All";
				default: return "dunnoSV";
			}
 		case 0x048B:
			switch (sv & 0x7fff) {
				case 0x0000	: return "VXDLDR_Get_Version";
				case 0x0001 : return "VCACHE_Register";
				case 0x0002 : return "VCACHE_GetSize";
				case 0x0003 : return "VCACHE_CheckAvail";
				case 0x0004 : return "VCACHE_FindBlock";
				case 0x0005 : return "VCACHE_FreeBlock";
				case 0x0006 : return "VCACHE_MakeMRU";
				case 0x0007 : return "VCACHE_Hold";
				case 0x0008 : return "VCACHE_Unhold";
				case 0x0009 : return "VCACHE_Enum";
				case 0x000A : return "VCACHE_TestHandle";
				case 0x000B : return "VCACHE_VerifySums";
				case 0x000C : return "VCACHE_RecalcSums";
				case 0x000D : return "VCACHE_TestHold";
				case 0x000E : return "VCACHE_GetStats";
				case 0x000F : return "VCache_Deregister";
				case 0x0010 : return "VCache_AdjustMinimum";
				case 0x0011 : return "VCache_SwapBuffers";
				case 0x0012 : return "VCache_RelinquishPage";
				case 0x0013 : return "VCache_UseThisPage";
				case 0x0014 : return "_VCache_CreateLookupCache";
				case 0x0015 : return "_VCache_CloseLookupCache";
				case 0x0016 : return "_VCache_DeleteLookupCache";
				case 0x0017 : return "_VCache_Lookup";
				case 0x0018 : return "_VCache_UpdateLookup";
				default: return "dunnoSV";
			}
		default: return "dunnoNR";
	 }
}


Bit8u lastint;
void CPU_Interrupt(Bitu num,Bitu type,Bitu oldeip) {
	lastint=num;
//	if ((num!=0x16) && (num!=0x1a) && (num!=0x28) && (num!=0x2a) && (num!=0x13)) LOG_MSG("INT %x EAX:%04X ECX:%04X EDX:%04X EBX:%04X CS:%04X EIP:%08X SS:%04X ESP:%08X",num,reg_eax,reg_ecx,reg_edx,reg_ebx,SegValue(cs),reg_eip,SegValue(ss),reg_esp);
	if ((num==0x20) && (cpu.pmode)) {
		PhysPt csip20 = SegPhys(cs)+reg_eip;
		Bitu sv = mem_readw(csip20+2);
		Bitu nr = mem_readw(csip20+4);
		LOG_MSG("VxD service %s, %s (%X:%X)",translateVXDid(nr),translateVXDservice(nr,sv),nr,sv);
	}
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
		CPU_Push16(oldeip);
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
		if ((reg_flags & FLAG_VM) && (type&CPU_INT_SOFTWARE) && !(type&CPU_INT_NOIOPLCHECK)) {
//			LOG_MSG("Software int in v86, AH %X IOPL %x",reg_ah,(reg_flags & FLAG_IOPL) >>12);
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				CPU_Exception(EXCEPTION_GP,0);
				return;
			}
		} 
		Descriptor gate;

		if (!cpu.idt.GetDescriptor(num<<3,gate)) {
			// zone66
			CPU_Exception(EXCEPTION_GP,num*8+2+(type&CPU_INT_SOFTWARE)?0:1);
			return;
		}

		if ((type&CPU_INT_SOFTWARE) && (gate.DPL()<cpu.cpl)) {
			// zone66, win3.x e
			CPU_Exception(EXCEPTION_GP,num*8+2);
			return;
		}

		CPU_CHECK_COND(!gate.saved.seg.p,
			"INT:Gate segment not present",
			EXCEPTION_NP,num*8+2+(type&CPU_INT_SOFTWARE)?0:1)

		switch (gate.Type()) {
		case DESC_286_INT_GATE:		case DESC_386_INT_GATE:
		case DESC_286_TRAP_GATE:	case DESC_386_TRAP_GATE:
			{
				Descriptor cs_desc;
				Bitu gate_sel=gate.GetSelector();
				Bitu gate_off=gate.GetOffset();
				CPU_CHECK_COND((gate_sel & 0xfffc)==0,
					"INT:Gate with CS zero selector",
					EXCEPTION_GP,(type&CPU_INT_SOFTWARE)?0:1)
				CPU_CHECK_COND(!cpu.gdt.GetDescriptor(gate_sel,cs_desc),
					"INT:Gate with CS beyond limit",
					EXCEPTION_GP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)

				Bitu cs_dpl=cs_desc.DPL();
				CPU_CHECK_COND(cs_dpl>cpu.cpl,
					"Interrupt to higher privilege",
					EXCEPTION_GP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
				switch (cs_desc.Type()) {
				case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
					if (cs_dpl<cpu.cpl) {
						/* Prepare for gate to inner level */
						CPU_CHECK_COND(!cs_desc.saved.seg.p,
							"INT:Inner level:CS segment not present",
							EXCEPTION_NP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
						CPU_CHECK_COND((reg_flags & FLAG_VM) && (cs_dpl!=0),
							"V86 interrupt calling codesegment with DPL>0",
							EXCEPTION_GP,gate_sel & 0xfffc)

						Bitu n_ss,n_esp;
						Bitu o_ss,o_esp;
						o_ss=SegValue(ss);
						o_esp=reg_esp;
						cpu_tss.Get_SSx_ESPx(cs_dpl,n_ss,n_esp);
						CPU_CHECK_COND((n_ss & 0xfffc)==0,
							"INT:Gate with SS zero selector",
							EXCEPTION_TS,(type&CPU_INT_SOFTWARE)?0:1)
						Descriptor n_ss_desc;
						CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
							"INT:Gate with SS beyond limit",
							EXCEPTION_TS,(n_ss & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
						CPU_CHECK_COND(((n_ss & 3)!=cs_dpl) || (n_ss_desc.DPL()!=cs_dpl),
							"INT:Inner level with CS_DPL!=SS_DPL and SS_RPL",
							EXCEPTION_TS,(n_ss & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)

						// check if stack segment is a writable data segment
						switch (n_ss_desc.Type()) {
						case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
						case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
							break;
						default:
							E_Exit("INT:Inner level:Stack segment not writable.");		// or #TS(ss_sel+EXT)
						}
						CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
							"INT:Inner level with nonpresent SS",
							EXCEPTION_SS,(n_ss & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)

						// commit point
						Segs.phys[ss]=n_ss_desc.GetBase();
						Segs.val[ss]=n_ss;
						if (n_ss_desc.Big()) {
							cpu.stack.big=true;
							cpu.stack.mask=0xffffffff;
							reg_esp=n_esp;
						} else {
							cpu.stack.big=false;
							cpu.stack.mask=0xffff;
							reg_sp=n_esp & 0xffff;
						}

						cpu.cpl=cs_dpl;
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
						goto do_interrupt;
					} 
					if (cs_dpl!=cpu.cpl)
						E_Exit("Non-conforming intra privilege INT with DPL!=CPL");
				case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
					/* Prepare stack for gate to same priviledge */
					CPU_CHECK_COND(!cs_desc.saved.seg.p,
							"INT:Same level:CS segment not present",
						EXCEPTION_NP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
					if ((reg_flags & FLAG_VM) && (cs_dpl<cpu.cpl))
						E_Exit("V86 interrupt doesn't change to pl0");	// or #GP(cs_sel)

					// commit point
do_interrupt:
					if (gate.Type() & 0x8) {	/* 32-bit Gate */
						CPU_Push32(reg_flags);
						CPU_Push32(SegValue(cs));
						CPU_Push32(oldeip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push32(cpu.exception.error);
					} else {					/* 16-bit gate */
						CPU_Push16(reg_flags & 0xffff);
						CPU_Push16(SegValue(cs));
						CPU_Push16(oldeip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push16(cpu.exception.error);
					}
					break;		
				default:
					E_Exit("INT:Gate Selector points to illegal descriptor with type %x",cs_desc.Type());
				}

				Segs.val[cs]=(gate_sel&0xfffc) | cpu.cpl;
				Segs.phys[cs]=cs_desc.GetBase();
				cpu.code.big=cs_desc.Big()>0;
				reg_eip=gate_off;

				if (!(gate.Type()&1))
					SETFLAGBIT(IF,false);
				SETFLAGBIT(TF,false);
				SETFLAGBIT(NT,false);
				SETFLAGBIT(VM,false);
				LOG(LOG_CPU,LOG_NORMAL)("INT:Gate to %X:%X big %d %s",gate_sel,gate_off,cs_desc.Big(),gate.Type() & 0x8 ? "386" : "286");
				return;
			}
		case DESC_TASK_GATE:
			CPU_SwitchTask(gate.GetSelector(),TSwitch_CALL_INT,oldeip);
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


void CPU_IRET(bool use32,Bitu oldeip) {
	if (!cpu.pmode) {					/* RealMode IRET */
		if (use32) {
			reg_eip=CPU_Pop32();
			SegSet16(cs,CPU_Pop32());
			CPU_SetFlags(CPU_Pop32(),FMASK_ALL);
		} else {
			reg_eip=CPU_Pop16();
			SegSet16(cs,CPU_Pop16());
			CPU_SetFlags(CPU_Pop16(),FMASK_ALL & 0xffff);
		}
		cpu.code.big=false;
		return;
	} else {	/* Protected mode IRET */
		if (reg_flags & FLAG_VM) {
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				// win3.x e
				CPU_Exception(EXCEPTION_GP,0);
				return;
			} else {
				if (use32) {
					reg_eip=CPU_Pop32();
					SegSet16(cs,CPU_Pop32());
					/* IOPL can not be modified in v86 mode by IRET */
					CPU_SetFlags(CPU_Pop32(),FMASK_NORMAL|FLAG_NT);
				} else {
					reg_eip=CPU_Pop16();
					SegSet16(cs,CPU_Pop16());
					/* IOPL can not be modified in v86 mode by IRET */
					CPU_SetFlags(CPU_Pop16(),FMASK_NORMAL|FLAG_NT);
				}
				cpu.code.big=false;
				return;
			}
		}
		/* Check if this is task IRET */	
		if (GETFLAG(NT)) {
			if (GETFLAG(VM)) E_Exit("Pmode IRET with VM bit set");
			CPU_CHECK_COND(!cpu_tss.IsValid(),
				"TASK Iret without valid TSS",
				EXCEPTION_TS,cpu_tss.selector & 0xfffc)
			if (!cpu_tss.desc.IsBusy()) LOG(LOG_CPU,LOG_ERROR)("TASK Iret:TSS not busy");
			Bitu back_link=cpu_tss.Get_back();
			CPU_SwitchTask(back_link,TSwitch_IRET,oldeip);
			return;
		}
		Bitu n_cs_sel,n_eip,n_flags;
		if (use32) {
			// commit point
			n_eip=CPU_Pop32();
			n_cs_sel=CPU_Pop32() & 0xffff;
			n_flags=CPU_Pop32();
			if ((n_flags & FLAG_VM) && (cpu.cpl==0)) {
				reg_eip=n_eip & 0xffff;
				Bitu n_ss,n_esp,n_es,n_ds,n_fs,n_gs;
				n_esp=CPU_Pop32();
				n_ss=CPU_Pop32() & 0xffff;
				n_es=CPU_Pop32() & 0xffff;
				n_ds=CPU_Pop32() & 0xffff;
				n_fs=CPU_Pop32() & 0xffff;
				n_gs=CPU_Pop32() & 0xffff;

				CPU_SetFlags(n_flags,FMASK_ALL | FLAG_VM);
				cpu.cpl=3;

				CPU_SetSegGeneral(ss,n_ss);
				CPU_SetSegGeneral(es,n_es);
				CPU_SetSegGeneral(ds,n_ds);
				CPU_SetSegGeneral(fs,n_fs);
				CPU_SetSegGeneral(gs,n_gs);
				reg_esp=n_esp;
				cpu.code.big=false;
				SegSet16(cs,n_cs_sel);
				LOG(LOG_CPU,LOG_NORMAL)("IRET:Back to V86: CS:%X IP %X SS:%X SP %X FLAGS:%X",SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);	
				return;
			}
			if (n_flags & FLAG_VM) E_Exit("IRET from pmode to v86 with CPL!=0");
		} else {
			n_eip=CPU_Pop16();
			n_cs_sel=CPU_Pop16();
			n_flags=(reg_flags & 0xffff0000) | CPU_Pop16();
			if (n_flags & FLAG_VM) E_Exit("VM Flag in 16-bit iret");
		}
		CPU_CHECK_COND((n_cs_sel & 0xfffc)==0,
			"IRET:CS selector zero",
			EXCEPTION_GP,0)
		Bitu n_cs_rpl=n_cs_sel & 3;
		Descriptor n_cs_desc;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc),
			"IRET:CS selector beyond limits",
			EXCEPTION_GP,n_cs_sel & 0xfffc)
		CPU_CHECK_COND(n_cs_rpl<cpu.cpl,
			"IRET to lower privilege",
			EXCEPTION_GP,n_cs_sel & 0xfffc)

		switch (n_cs_desc.Type()) {
		case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(n_cs_rpl!=n_cs_desc.DPL(),
				"IRET:NC:DPL!=RPL",
				EXCEPTION_GP,n_cs_sel & 0xfffc)
			break;
		case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
			CPU_CHECK_COND(n_cs_desc.DPL()>n_cs_rpl,
				"IRET:C:DPL>RPL",
				EXCEPTION_GP,n_cs_sel & 0xfffc)
			break;
		default:
			E_Exit("IRET:Illegal descriptor type %X",n_cs_desc.Type());
		}
		CPU_CHECK_COND(!n_cs_desc.saved.seg.p,
			"IRET with nonpresent code segment",
			EXCEPTION_NP,n_cs_sel & 0xfffc)

		if (n_cs_rpl==cpu.cpl) {	
			/* Return to same level */
			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;
			reg_eip=n_eip;

			Bitu mask=cpu.cpl ? (FMASK_NORMAL | FLAG_NT) : FMASK_ALL;
			if (GETFLAG_IOPL<cpu.cpl) mask &= (~FLAG_IF);
			CPU_SetFlags(n_flags,mask);
			LOG(LOG_CPU,LOG_NORMAL)("IRET:Same level:%X:%X big %d",n_cs_sel,n_eip,cpu.code.big);
		} else {
			/* Return to outer level */
			Bitu n_ss,n_esp;
			if (use32) {
				n_esp=CPU_Pop32();
				n_ss=CPU_Pop32() & 0xffff;
			} else {
				n_esp=CPU_Pop16();
				n_ss=CPU_Pop16();
			}
			CPU_CHECK_COND((n_ss & 0xfffc)==0,
				"IRET:Outer level:SS selector zero",
				EXCEPTION_GP,0)
			CPU_CHECK_COND((n_ss & 3)!=n_cs_rpl,
				"IRET:Outer level:SS rpl!=CS rpl",
				EXCEPTION_GP,n_ss & 0xfffc)
			Descriptor n_ss_desc;
			CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
				"IRET:Outer level:SS beyond limit",
				EXCEPTION_GP,n_ss & 0xfffc)
			CPU_CHECK_COND(n_ss_desc.DPL()!=n_cs_rpl,
				"IRET:Outer level:SS dpl!=CS rpl",
				EXCEPTION_GP,n_ss & 0xfffc)

			// check if stack segment is a writable data segment
			switch (n_ss_desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				E_Exit("IRET:Outer level:Stack segment not writable");		// or #GP(ss_sel)
			}
			CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
				"IRET:Outer level:Stack segment not present",
				EXCEPTION_NP,n_ss & 0xfffc)

			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;

			Bitu mask=cpu.cpl ? (FMASK_NORMAL | FLAG_NT) : FMASK_ALL;
			if (GETFLAG_IOPL<cpu.cpl) mask &= (~FLAG_IF);
			CPU_SetFlags(n_flags,mask);

			cpu.cpl=n_cs_rpl;
			reg_eip=n_eip;

			Segs.val[ss]=n_ss;
			Segs.phys[ss]=n_ss_desc.GetBase();
			if (n_ss_desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				reg_esp=n_esp;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				reg_sp=n_esp & 0xffff;
			}

			// borland extender, zrdx
			Descriptor desc;
			cpu.gdt.GetDescriptor(SegValue(es),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(es,0); break;
			default: break;	}
			cpu.gdt.GetDescriptor(SegValue(ds),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(ds,0); break;
			default: break;	}
			cpu.gdt.GetDescriptor(SegValue(fs),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(fs,0); break;
			default: break;	}
			cpu.gdt.GetDescriptor(SegValue(gs),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(gs,0); break;
			default: break;	}

			LOG(LOG_CPU,LOG_NORMAL)("IRET:Outer level:%X:%X big %d",n_cs_sel,n_eip,cpu.code.big);
		}
		return;
	}
}


void CPU_JMP(bool use32,Bitu selector,Bitu offset,Bitu oldeip) {
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
		CPU_CHECK_COND((selector & 0xfffc)==0,
			"JMP:CS selector zero",
			EXCEPTION_GP,0)
		Bitu rpl=selector & 3;
		Descriptor desc;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,desc),
			"JMP:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)
		switch (desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(rpl>cpu.cpl,
				"JMP:NC:RPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(cpu.cpl!=desc.DPL(),
				"JMP:NC:RPL != DPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:NC to %X:%X big %d",selector,offset,desc.Big());
			goto CODE_jmp;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:C to %X:%X big %d",selector,offset,desc.Big());
			CPU_CHECK_COND(cpu.cpl<desc.DPL(),
				"JMP:C:CPL < DPL",
				EXCEPTION_GP,selector & 0xfffc)
CODE_jmp:
			if (!desc.saved.seg.p) {
				// win
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}

			/* Normal jump to another selector:offset */
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			reg_eip=offset;
			return;
		case DESC_386_TSS_A:
			CPU_CHECK_COND(desc.DPL()<cpu.cpl,
				"JMP:TSS:dpl<cpl",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(desc.DPL()<rpl,
				"JMP:TSS:dpl<rpl",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("JMP:TSS to %X",selector);
			CPU_SwitchTask(selector,TSwitch_JMP,oldeip);
			break;
		default:
			E_Exit("JMP Illegal descriptor type %X",desc.Type());
		}
	}
	assert(1);
}


void CPU_CALL(bool use32,Bitu selector,Bitu offset,Bitu oldeip) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		if (!use32) {
			CPU_Push16(SegValue(cs));
			CPU_Push16(oldeip);
			reg_eip=offset&0xffff;
		} else {
			CPU_Push32(SegValue(cs));
			CPU_Push32(oldeip);
			reg_eip=offset;
		}
		cpu.code.big=false;
		SegSet16(cs,selector);
		return;
	} else {
		CPU_CHECK_COND((selector & 0xfffc)==0,
			"CALL:CS selector zero",
			EXCEPTION_GP,0)
		Descriptor call;
		Bitu rpl=selector & 3;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,call),
			"CALL:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)
		/* Check for type of far call */
		switch (call.Type()) {
		case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(rpl>cpu.cpl,
				"CALL:CODE:NC:RPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(call.DPL()!=cpu.cpl,
				"CALL:CODE:NC:DPL!=CPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:NC to %X:%X",selector,offset);
			goto call_code;	
		case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
			CPU_CHECK_COND(call.DPL()>cpu.cpl,
				"CALL:CODE:C:DPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:C to %X:%X",selector,offset);
call_code:
			if (!call.saved.seg.p) {
				// borland extender (RTM)
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}
			// commit point
			if (!use32) {
				CPU_Push16(SegValue(cs));
				CPU_Push16(oldeip);
				reg_eip=offset & 0xffff;
			} else {
				CPU_Push32(SegValue(cs));
				CPU_Push32(oldeip);
				reg_eip=offset;
			}
			Segs.phys[cs]=call.GetBase();
			cpu.code.big=call.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			return;
		case DESC_386_CALL_GATE: 
		case DESC_286_CALL_GATE:
			{
				CPU_CHECK_COND(call.DPL()<cpu.cpl,
					"CALL:Gate:Gate DPL<CPL",
					EXCEPTION_GP,selector & 0xfffc)
				CPU_CHECK_COND(call.DPL()<rpl,
					"CALL:Gate:Gate DPL<RPL",
					EXCEPTION_GP,selector & 0xfffc)
				CPU_CHECK_COND(!call.saved.seg.p,
					"CALL:Gate:Segment not present",
					EXCEPTION_NP,selector & 0xfffc)
				Descriptor n_cs_desc;
				Bitu n_cs_sel=call.GetSelector();

				CPU_CHECK_COND((n_cs_sel & 0xfffc)==0,
					"CALL:Gate:CS selector zero",
					EXCEPTION_GP,0)
				CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc),
					"CALL:Gate:CS beyond limits",
					EXCEPTION_GP,n_cs_sel & 0xfffc)
				Bitu n_cs_dpl	= n_cs_desc.DPL();
				CPU_CHECK_COND(n_cs_dpl>cpu.cpl,
					"CALL:Gate:CS DPL>CPL",
					EXCEPTION_GP,n_cs_sel & 0xfffc)
				Bitu n_cs_rpl	= n_cs_sel & 3;
				Bitu n_eip		= call.GetOffset();
				switch (n_cs_desc.Type()) {
				case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
					/* Check if we goto inner priviledge */
					if (n_cs_dpl < cpu.cpl) {
						CPU_CHECK_COND(!n_cs_desc.saved.seg.p,
							"CALL:Gate:CS not present",
							EXCEPTION_NP,n_cs_sel & 0xfffc)
						/* Get new SS:ESP out of TSS */
						Bitu n_ss_sel,n_esp;
						Descriptor n_ss_desc;
						cpu_tss.Get_SSx_ESPx(n_cs_dpl,n_ss_sel,n_esp);
						CPU_CHECK_COND((n_ss_sel & 0xfffc)==0,
							"CALL:Gate:NC:SS selector zero",
							EXCEPTION_TS,0)
						CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss_sel,n_ss_desc),
							"CALL:Gate:Invalid SS selector",
							EXCEPTION_TS,n_ss_sel & 0xfffc)
						CPU_CHECK_COND(((n_ss_sel & 3)!=n_cs_desc.DPL()) || (n_ss_desc.DPL()!=n_cs_desc.DPL()),
							"CALL:Gate:Invalid SS selector privileges",
							EXCEPTION_TS,n_ss_sel & 0xfffc)

						switch (n_ss_desc.Type()) {
						case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
						case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
							// writable data segment
							break;
						default:
							E_Exit("Call:Gate:SS no writable data segment");	// or #TS(ss_sel)
						}
						CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
							"CALL:Gate:Stack segment not present",
							EXCEPTION_SS,n_ss_sel & 0xfffc)

						/* Load the new SS:ESP and save data on it */
						Bitu o_esp		= reg_esp;
						Bitu o_ss		= SegValue(ss);
						PhysPt o_stack  = SegPhys(ss)+(reg_esp & cpu.stack.mask);

						// catch pagefaults
						if (call.saved.gate.paramcount&31) {
							if (call.Type()==DESC_386_CALL_GATE) {
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--) 
									mem_readd(o_stack+i*4);
							} else {
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									mem_readw(o_stack+i*2);
							}
						}

						// commit point
						Segs.val[ss]=n_ss_sel;
						Segs.phys[ss]=n_ss_desc.GetBase();
						if (n_ss_desc.Big()) {
							cpu.stack.big=true;
							cpu.stack.mask=0xffffffff;
							reg_esp=n_esp;
						} else {
							cpu.stack.big=false;
							cpu.stack.mask=0xffff;
							reg_sp=n_esp & 0xffff;
						}

						cpu.cpl = n_cs_desc.DPL();
						Bit16u oldcs    = SegValue(cs);
						/* Switch to new CS:EIP */
						Segs.phys[cs]	= n_cs_desc.GetBase();
						Segs.val[cs]	= (n_cs_sel & 0xfffc) | cpu.cpl;
						cpu.code.big	= n_cs_desc.Big()>0;
						reg_eip			= n_eip;
						if (!use32)	reg_eip&=0xffff;

						if (call.Type()==DESC_386_CALL_GATE) {
							CPU_Push32(o_ss);		//save old stack
							CPU_Push32(o_esp);
							if (call.saved.gate.paramcount&31)
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--) 
									CPU_Push32(mem_readd(o_stack+i*4));
							CPU_Push32(oldcs);
							CPU_Push32(oldeip);
						} else {
							CPU_Push16(o_ss);		//save old stack
							CPU_Push16(o_esp);
							if (call.saved.gate.paramcount&31)
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									CPU_Push16(mem_readw(o_stack+i*2));
							CPU_Push16(oldcs);
							CPU_Push16(oldeip);
						}

						break;		
					} else if (n_cs_dpl > cpu.cpl)
						E_Exit("CALL:GATE:CS DPL>CPL");		// or #GP(sel)
				case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
					// zrdx extender

					if (call.Type()==DESC_386_CALL_GATE) {
						CPU_Push32(SegValue(cs));
						CPU_Push32(oldeip);
					} else {
						CPU_Push16(SegValue(cs));
						CPU_Push16(oldeip);
					}

					/* Switch to new CS:EIP */
					Segs.phys[cs]	= n_cs_desc.GetBase();
					Segs.val[cs]	= (n_cs_sel & 0xfffc) | cpu.cpl;
					cpu.code.big	= n_cs_desc.Big()>0;
					reg_eip			= n_eip;
					if (!use32)	reg_eip&=0xffff;
					break;
				default:
					E_Exit("CALL:GATE:CS no executable segment");
				}
			}			/* Call Gates */
			break;
		case DESC_386_TSS_A:
			CPU_CHECK_COND(call.DPL()<cpu.cpl,
				"CALL:TSS:dpl<cpl",
				EXCEPTION_TS,selector & 0xfffc)
			CPU_CHECK_COND(call.DPL()<rpl,
				"CALL:TSS:dpl<rpl",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("CALL:TSS to %X",selector);
			CPU_SwitchTask(selector,TSwitch_CALL_INT,oldeip);
			break;
		default:
			E_Exit("CALL:Descriptor type %x unsupported",call.Type());

		}
	}
	assert(1);
}


void CPU_RET(bool use32,Bitu bytes,Bitu oldeip) {
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
		if(rpl < cpu.cpl) {
			// win setup
			CPU_Exception(EXCEPTION_GP,selector & 0xfffc);
			return;
		}

		CPU_CHECK_COND((selector & 0xfffc)==0,
			"RET:CS selector zero",
			EXCEPTION_GP,0)
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,desc),
			"RET:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)

		if (cpu.cpl==rpl) {	
			/* Return to same level */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
				CPU_CHECK_COND(cpu.cpl!=desc.DPL(),
					"RET to NC segment of other privilege",
					EXCEPTION_GP,selector & 0xfffc)
				goto RET_same_level;
			case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
				CPU_CHECK_COND(desc.DPL()>cpu.cpl,
					"RET to C segment of higher privilege",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			default:
				E_Exit("RET from illegal descriptor type %X",desc.Type());
			}
RET_same_level:
			if (!desc.saved.seg.p) {
				// borland extender (RTM)
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}

			// commit point
			if (!use32) {
				offset=CPU_Pop16();
				selector=CPU_Pop16();
			} else {
				offset=CPU_Pop32();
				selector=CPU_Pop32() & 0xffff;
			}

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
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
				CPU_CHECK_COND(desc.DPL()!=rpl,
					"RET to outer NC segment with DPL!=RPL",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
				CPU_CHECK_COND(desc.DPL()>rpl,
					"RET to outer C segment with DPL>RPL",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			default:
				E_Exit("RET from illegal descriptor type %X",desc.Type());		// or #GP(selector)
			}

			CPU_CHECK_COND(!desc.saved.seg.p,
				"RET:Outer level:CS not present",
				EXCEPTION_NP,selector & 0xfffc)

			// commit point
			Bitu n_esp,n_ss;
			if (use32) {
				offset=CPU_Pop32();
				selector=CPU_Pop32() & 0xffff;
				reg_esp+=bytes;
				n_esp = CPU_Pop32();
				n_ss = CPU_Pop32() & 0xffff;
			} else {
				offset=CPU_Pop16();
				selector=CPU_Pop16();
				reg_esp+=bytes;
				n_esp = CPU_Pop16();
				n_ss = CPU_Pop16();
			}

			CPU_CHECK_COND((n_ss & 0xfffc)==0,
				"RET to outer level with SS selector zero",
				EXCEPTION_GP,0)

			Descriptor n_ss_desc;
			CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
				"RET:SS beyond limits",
				EXCEPTION_GP,n_ss & 0xfffc)

			CPU_CHECK_COND(((n_ss & 3)!=rpl) || (n_ss_desc.DPL()!=rpl),
				"RET to outer segment with invalid SS privileges",
				EXCEPTION_GP,n_ss & 0xfffc)
			switch (n_ss_desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				E_Exit("RET:SS selector type no writable data segment");	// or #GP(selector)
			}
			CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
				"RET:Stack segment not present",
				EXCEPTION_SS,n_ss & 0xfffc)

			cpu.cpl = rpl;
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector&0xfffc) | cpu.cpl;
			reg_eip=offset;

			Segs.val[ss]=n_ss;
			Segs.phys[ss]=n_ss_desc.GetBase();
			if (n_ss_desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				reg_esp=n_esp+bytes;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				reg_sp=(n_esp & 0xffff)+bytes;
			}

			Descriptor desc;
			cpu.gdt.GetDescriptor(SegValue(es),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(es,0); break;
			default: break;	}
			cpu.gdt.GetDescriptor(SegValue(ds),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(ds,0); break;
			default: break;	}
			cpu.gdt.GetDescriptor(SegValue(fs),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(fs,0); break;
			default: break;	}
			cpu.gdt.GetDescriptor(SegValue(gs),desc);
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
			case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
				if (cpu.cpl>desc.DPL()) CPU_SetSegGeneral(gs,0); break;
			default: break;	}

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

bool CPU_LLDT(Bitu selector) {
	if (!cpu.gdt.LLDT(selector)) {
		LOG(LOG_CPU,LOG_ERROR)("LLDT failed, selector=%X",selector);
		return true;
	}
	LOG(LOG_CPU,LOG_NORMAL)("LDT Set to %X",selector);
	return false;
}

void CPU_STR(Bitu & selector) {
	selector=cpu_tss.selector;
}

bool CPU_LTR(Bitu selector) {
	if ((selector & 0xfffc)==0) {
		cpu_tss.SetSelector(selector);
		return false;
	}
	TSS_Descriptor desc;
	if ((selector & 4) || (!cpu.gdt.GetDescriptor(selector,desc))) {
		LOG(LOG_CPU,LOG_ERROR)("LTR failed, selector=%X",selector);
		return CPU_PrepareException(EXCEPTION_GP,selector);
	}

	if ((desc.Type()==DESC_286_TSS_A) || (desc.Type()==DESC_386_TSS_A)) {
		if (!desc.saved.seg.p) {
			LOG(LOG_CPU,LOG_ERROR)("LTR failed, selector=%X (not present)",selector);
			return CPU_PrepareException(EXCEPTION_NP,selector);
		}
		if (!cpu_tss.SetSelector(selector)) E_Exit("LTR failed, selector=%X",selector);
		cpu_tss.desc.SetBusy(true);
	} else {
		/* Descriptor was no available TSS descriptor */ 
		LOG(LOG_CPU,LOG_NORMAL)("LTR failed, selector=%X (type=%X)",selector,desc.Type());
		return CPU_PrepareException(EXCEPTION_GP,selector);
	}
	return false;
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


void CPU_SET_CRX(Bitu cr,Bitu value) {
	switch (cr) {
	case 0:
		{
			Bitu changed=cpu.cr0 ^ value;		
			if (!changed) return;
			cpu.cr0=value;
			if (value & CR0_PROTECTION) {
				cpu.pmode=true;
				LOG(LOG_CPU,LOG_NORMAL)("Protected mode");
				PAGING_Enable((value & CR0_PAGING)>0);
			} else {
				cpu.pmode=false;
				if (value & CR0_PAGING) LOG_MSG("Paging requested without PE=1");
				PAGING_Enable(false);
				LOG(LOG_CPU,LOG_NORMAL)("Real mode");
			}
			break;
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
}

bool CPU_WRITE_CRX(Bitu cr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	if ((cr==1) || (cr>4)) return CPU_PrepareException(EXCEPTION_UD,0);
	CPU_SET_CRX(cr,value);
	return false;
}

Bitu CPU_GET_CRX(Bitu cr) {
	switch (cr) {
	case 0:
		return cpu.cr0;
	case 2:
		return paging.cr2;
	case 3:
		return PAGING_GetDirBase() & 0xfffff000;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, CR%d",cr);
		break;
	}
	return 0;
}

bool CPU_READ_CRX(Bitu cr,Bit32u & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	if ((cr==1) || (cr>4)) return CPU_PrepareException(EXCEPTION_UD,0);
	retvalue=CPU_GET_CRX(cr);
	return false;
}


bool CPU_WRITE_DRX(Bitu dr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (dr) {
	case 0:
	case 1:
	case 2:
	case 3:
		cpu.drx[dr]=value;
		break;
	case 4:
	case 6:
		cpu.drx[6]=(value|0xffff0ff0) & 0xffffefff;
		break;
	case 5:
	case 7:
		cpu.drx[7]=(value|0x400) & 0xffff2fff;
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV DR%d,%X",dr,value);
		break;
	}
	return false;
}

bool CPU_READ_DRX(Bitu dr,Bit32u & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (dr) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 6:
	case 7:
		retvalue=cpu.drx[dr];
		break;
	case 4:
		retvalue=cpu.drx[6];
		break;
	case 5:
		retvalue=cpu.drx[7];
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, DR%d",dr);
		retvalue=0;
		break;
	}
	return false;
}


void CPU_SMSW(Bitu & word) {
	word=cpu.cr0;
}

Bitu CPU_LMSW(Bitu word) {
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	word&=0xf;
	if (cpu.cr0 & 1) word|=1; 
	word|=(cpu.cr0&0xfffffff0);
	CPU_SET_CRX(0,word);
	return false;
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
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
	case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
		break;

	case DESC_286_INT_GATE:		case DESC_286_TRAP_GATE:	{
	case DESC_386_INT_GATE:		case DESC_386_TRAP_GATE:
		SETFLAGBIT(ZF,false);
		return;
	}

	case DESC_LDT:
	case DESC_TASK_GATE:

	case DESC_286_TSS_A:		case DESC_286_TSS_B:
	case DESC_286_CALL_GATE:

	case DESC_386_TSS_A:		case DESC_386_TSS_B:
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
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
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
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
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
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
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
		if (seg==ss) {
			// Stack needs to be non-zero
			if ((value & 0xfffc)==0) {
				E_Exit("CPU_SetSegGeneral: Stack segment zero");
//				return CPU_PrepareException(EXCEPTION_GP,0);
			}
			Descriptor desc;
			if (!cpu.gdt.GetDescriptor(value,desc)) {
				E_Exit("CPU_SetSegGeneral: Stack segment beyond limits");
//				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}
			if (((value & 3)!=cpu.cpl) || (desc.DPL()!=cpu.cpl)) {
				E_Exit("CPU_SetSegGeneral: Stack segment with invalid privileges");
//				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}

			switch (desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				//Earth Siege 1
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}

			if (!desc.saved.seg.p) {
				E_Exit("CPU_SetSegGeneral: Stack segment not present");	// or #SS(sel)
//				return CPU_PrepareException(EXCEPTION_SS,value & 0xfffc);
			}

			Segs.val[seg]=value;
			Segs.phys[seg]=desc.GetBase();
			if (desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
			}
		} else {
			if ((value & 0xfffc)==0) {
				Segs.val[seg]=value;
				Segs.phys[seg]=0;	// ??
				return false;
			}
			Descriptor desc;
			if (!cpu.gdt.GetDescriptor(value,desc)) {
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:		case DESC_DATA_EU_RO_A:
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:		case DESC_DATA_ED_RO_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
			case DESC_CODE_R_NC_A:			case DESC_CODE_R_NC_NA:
				if (((value & 3)>desc.DPL()) || (cpu.cpl>desc.DPL())) {
					// extreme pinball
					return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
				}
				break;
			case DESC_CODE_R_C_A:			case DESC_CODE_R_C_NA:
				break;
			default:
				// gabriel knight
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);

			}
			if (!desc.saved.seg.p) {
				// win
				return CPU_PrepareException(EXCEPTION_NP,value & 0xfffc);
			}

			Segs.val[seg]=value;
			Segs.phys[seg]=desc.GetBase();
		}

		return false;
	}
}

bool CPU_PopSeg(SegNames seg,bool use32) {
	Bitu val=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	if (CPU_SetSegGeneral(seg,val)) return true;
	Bitu addsp=use32?0x04:0x02;
	reg_esp=(reg_esp&~cpu.stack.mask)|((reg_esp+addsp)&cpu.stack.mask);
	return false;
}

void CPU_CPUID(void) {
	switch (reg_eax) {
	case 0:	/* Vendor ID String and maximum level? */
		reg_eax=1;  /* Maximum level */ 
		reg_ebx='G' | ('e' << 8) | ('n' << 16) | ('u'<< 24); 
		reg_edx='i' | ('n' << 8) | ('e' << 16) | ('I'<< 24); 
		reg_ecx='n' | ('t' << 8) | ('e' << 16) | ('l'<< 24); 
		break;
	case 1:	/* get processor type/family/model/stepping and feature flags */
		reg_eax=0x402;		/* intel 486 sx? */
		reg_ebx=0;			/* Not Supported */
		reg_ecx=0;			/* No features */
		reg_edx=1;			/* FPU */
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

void CPU_HLT(Bitu oldeip) {
	reg_eip=oldeip;
	CPU_Cycles=0;
	cpu.hlt.cs=SegValue(cs);
	cpu.hlt.eip=reg_eip;
	cpu.hlt.old_decoder=cpudecoder;
	cpudecoder=&HLT_Decode;
}

void CPU_ENTER(bool use32,Bitu bytes,Bitu level) {
	level&=0x1f;
	Bitu sp_index=reg_esp&cpu.stack.mask;
	Bitu bp_index=reg_ebp&cpu.stack.mask;
	if (!use32) {
		sp_index-=2;
		mem_writew(SegPhys(ss)+sp_index,reg_bp);
		reg_bp=(Bit16u)(reg_esp-2);
		if (level) {
			for (Bitu i=1;i<level;i++) {	
				sp_index-=2;bp_index-=2;
				mem_writew(SegPhys(ss)+sp_index,mem_readw(SegPhys(ss)+bp_index));
			}
			sp_index-=2;
			mem_writew(SegPhys(ss)+sp_index,reg_bp);
		}
	} else {
		sp_index-=4;
        mem_writed(SegPhys(ss)+sp_index,reg_ebp);
		reg_ebp=(reg_esp-4);
		if (level) {
			for (Bitu i=1;i<level;i++) {	
				sp_index-=4;bp_index-=4;
				mem_writed(SegPhys(ss)+sp_index,mem_readd(SegPhys(ss)+bp_index));
			}
			sp_index-=4;
			mem_writed(SegPhys(ss)+sp_index,reg_ebp);
		}
	}
	sp_index-=bytes;
	reg_esp=(reg_esp&~cpu.stack.mask)|((sp_index)&cpu.stack.mask);
}

extern void GFX_SetTitle(Bits cycles ,Bits frameskip,bool paused);
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
	GFX_SetTitle(CPU_CycleMax,-1,false);
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
	GFX_SetTitle(CPU_CycleMax,-1,false);
}

class CPU: public Module_base {
private:
	static bool inited;
public:
	CPU(Section* configuration):Module_base(configuration) {
		if(inited) {
			Change_Config(configuration);
			return;
		}
		inited=true;
		Section_prop * section=static_cast<Section_prop *>(configuration);
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
	
		CPU_SetFlags(FLAG_IF,FMASK_ALL);		//Enable interrupts
		cpu.cr0=0xffffffff;
		CPU_SET_CRX(0,0);						//Initialize
		cpu.code.big=false;
		cpu.stack.mask=0xffff;
		cpu.stack.big=false;
		cpu.idt.SetBase(0);
		cpu.idt.SetLimit(1023);

		for (Bitu i=0; i<7; i++) cpu.drx[i]=0;
		cpu.drx[6]=0xffff1ff0;
		cpu.drx[7]=0x00000400;

		/* Init the cpu cores */
		CPU_Core_Normal_Init();
		CPU_Core_Simple_Init();
		CPU_Core_Full_Init();
#if (C_DYNAMIC_X86)
		CPU_Core_Dyn_X86_Init();
#endif
		MAPPER_AddHandler(CPU_CycleDecrease,MK_f11,MMOD1,"cycledown","Dec Cycles");
		MAPPER_AddHandler(CPU_CycleIncrease,MK_f12,MMOD1,"cycleup"  ,"Inc Cycles");
		Change_Config(configuration);	
		CPU_JMP(false,0,0,0);					//Setup the first cpu core
	}
	bool Change_Config(Section* newconfig){
		Section_prop * section=static_cast<Section_prop *>(newconfig);
		CPU_CycleLeft=0;//needed ?
		CPU_Cycles=0;
		CPU_CycleMax=section->Get_int("cycles");;
		CPU_CycleUp=section->Get_int("cycleup");
		CPU_CycleDown=section->Get_int("cycledown");
		const char * core=section->Get_string("core");
		cpudecoder=&CPU_Core_Normal_Run;
		if (!strcasecmp(core,"normal")) {
			cpudecoder=&CPU_Core_Normal_Run;
		} else if (!strcasecmp(core,"simple")) {
			cpudecoder=&CPU_Core_Simple_Run;
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
	
		if(CPU_CycleMax <= 0) CPU_CycleMax = 2500;
		if(CPU_CycleUp <= 0)   CPU_CycleUp = 500;
		if(CPU_CycleDown <= 0) CPU_CycleDown = 20;
		GFX_SetTitle(CPU_CycleMax,-1,false);
		return true;
	}
	~CPU(){ /* empty */};
};
	
static CPU * test;

void CPU_ShutDown(Section* sec) {
	delete test;
}

void CPU_Init(Section* sec) {
	test = new CPU(sec);
	sec->AddDestroyFunction(&CPU_ShutDown,true);
}
//initialize static members
bool CPU::inited=false;
