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

#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "dos_inc.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_system.h"
#include "setup.h"
#include "inout.h"
#include "cpu.h"

//#define DPMI_LOG		LOG(LOG_MISC,LOG_ERROR)
#define DPMI_LOG		

#define DPMI_LOG_ERROR	LOG(LOG_MISC,LOG_ERROR)
//#define DPMI_LOG_ERROR		

#define DPMI_DPL		3

#define GDT_ZERO		0
#define GDT_LDT			((0x1 << 3) | DPMI_DPL)
#define GDT_CODE		((0x2 << 3) | DPMI_DPL)
#define GDT_PROTCODE	((0x3 << 3) | DPMI_DPL)
#define GDT_DOSDATA		((0x4 << 3) | DPMI_DPL)
#define GDT_ENVIRONMENT	((0x5 << 3) | DPMI_DPL)

// TEMP
#define GDT_DOSSEG40	(0x40)

/* Amount of descriptors in each table */ 
#define GDT_SIZE		32
#define IDT_SIZE		256
#define LDT_SIZE		1024
#define INT_SIZE		256

#define TOTAL_SIZE		((GDT_SIZE+IDT_SIZE+LDT_SIZE+INT_SIZE)*8)

#define LDT_ENTRY(BLAH_) (BLAH_ << 3)

#define LDT_FIRSTSELECTOR  16

#define DPMI_ERROR_UNSUPPORTED					0x8001
#define DPMI_ERROR_DESCRIPTOR_UNAVAILABLE		0x8011
#define DPMI_ERROR_LINEAR_MEMORY_UNAVAILABLE	0x8012
#define DPMI_ERROR_PHYSICAL_MEMORY_UNAVAILABLE	0x8013
#define DPMI_ERROR_CALLBACK_UNAVAILABLE			0x8015
#define DPMI_ERROR_INVALID_SELECTOR				0x8022
#define DPMI_ERROR_INVALID_VALUE				0x8022
#define DPMI_ERROR_INVALID_HANDLE				0x8023
#define DPMI_ERROR_INVALID_CALLBACK				0x8024
#define DPMI_ERROR_INVALID_LINEAR_ADDRESS		0x8025

#define DPMI_XMSHANDLES_MAX				256
#define DPMI_XMSHANDLE_FREE				0xFFFF
#define DPMI_EXCEPTION_MAX				0x20
#define DPMI_PAGE_SIZE					(4*1024)
#define DPMI_REALMODE_CALLBACK_MAX		32
#define DPMI_REALMODE_STACKSIZE			4096
#define DPMI_PROTMODE_STACK_MAX			3
#define DPMI_PROTMODE_STACKSIZE			(4*1024)
#define DPMI_REALVEC_MAX				17
#define DPMI_SAVESTACK_MAX				1024

#define DPMI_CB_APIMSDOSENTRY_OFFSET	256*8
#define DPMI_CB_ENTERREALMODE_OFFSET	257*8
#define DPMI_CB_SAVESTATE_OFFSET		258*8
#define DPMI_CB_EXCEPTION_OFFSET		259*8
#define DPMI_CB_EXCEPTIONRETURN_OFFSET	260*8
#define DPMI_CB_VENDORENTRY_OFFSET		261*8

#define DPMI_HOOK_HARDWARE_INTS			1

static Bitu rmIndexToInt[DPMI_REALVEC_MAX] = 
{ 0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x1C };

// General functions
void CALLBACK32_SCF(bool val) 
{
	Bit32u tempf=mem_readd(SegPhys(ss)+reg_esp+8) & 0xFFFFFFFE;
	Bit32u newCF=(val==true);
	mem_writed(SegPhys(ss)+reg_esp+8,(tempf | newCF));
};

#define DPMI_CALLBACK_SCF(b) if (dpmi.client.bit32) CALLBACK32_SCF(b); else CALLBACK_SCF(b)

// **********************************************
// SetDescriptor Class
// **********************************************

#pragma pack(1)
class SetDescriptor : public Descriptor {
public:
	void Save(PhysPt address) {
		Bit32u* data = (Bit32u*)&saved;
		mem_writed(address,*data);
		mem_writed(address+4,*(data+1));
	}

	void SetBase(Bitu _base) { 
		saved.seg.base_24_31=_base >> 24;
		saved.seg.base_16_23=_base >> 16;
		saved.seg.base_0_15=_base;
	}
	void SetLimit (Bitu _limit) {
		if (_limit<1048576) saved.seg.g=false;
		else {
			saved.seg.g=true;
			_limit>>=12;
		}
		saved.seg.limit_0_15=_limit;
		saved.seg.limit_16_19=_limit>>16;
	}
	void SetOffset(Bitu _offset) {
		saved.gate.offset_0_15=_offset;
		saved.gate.offset_16_31=(_offset>>16);
	}
	void SetSelector(Bitu _selector) {
		saved.gate.selector=_selector;
	}
	void SetType(Bitu _type) {
		saved.seg.type=_type;
	}
	void Clear(void) {
		saved.fill[0]=saved.fill[1]=0;
	}
};
#pragma pack()

// **********************************************
// DPMI Class
// **********************************************

class DPMI {

public:
	DPMI								(void);
	~DPMI								(void);

	// Settp/Startup methods
	void		Setup					(void);
	Bitu		Entrypoint				(void);
	RealPt		HookInterrupt			(Bitu num, Bitu intHandler);
	void		RestoreHookedInterrupt	(Bitu num, RealPt oldVec);
	void		CreateStackSpace		(void);
	bool		HasClient				(void)		{ return dpmi.client.have; };
	void		Terminate				(void);
	void		Reactivate				(void);

	// DPMI Services
	Bitu		AllocateLDTDescriptor	(Bitu count,Bitu & base);
	Bitu		AllocateLDTDescriptor2	(Bitu count,Bitu & base); // TEMP
	bool		AllocateMem				(Bitu size, Bitu& outHandle, Bitu& linear);
	Bitu		CreateAlias				(Bitu selector, Bit16u& alias);
	void		ReloadSegments			(Bitu selector);
	
	// Special Interrupt handlers
	Bitu		Int2fHandler			(void);
	Bitu		Int31Handler			(void);

	// Exceptions
	void		CreateException			(Bitu num, Bitu errorCode);
	Bitu		ExceptionReturn			(void);

	// Realmode callbacks
	bool		AllocateRealModeCallback(Bitu codeSel,Bitu codeOff,Bitu dataSel, Bitu dataOff, Bitu& segment, Bitu& offset);
	Bitu		RealModeCallback		(void);
	Bitu		RealModeCallbackReturn	(void);

	// Real mode reflection callbacks
	void		PrepareReflectToReal	(Bitu num);
	Bitu		CallRealIRETFrame		(void);
	Bitu		CallRealIRETFrameReturn	(void);
	Bitu		SimulateInt				(void);
	Bitu		SimulateIntReturn		(void);
	Bitu		ptorHandler				(void); 
	Bitu		ptorHandlerReturn		(void);
	Bitu		Int21Handler			(void);
	Bitu		Int21HandlerReturn		(void);
	Bitu		HWIntDefaultHandler		(void);
	void		RemoveIntCallbacks		(void);
	void		RestoreIntCallbacks		(void);

	// Switching modes
	void		SaveRegisterState		(Bitu num);
	void		LoadRegisterState		(Bitu num);	
	Bitu		EnterProtMode			(void);
	Bitu		EnterRealMode			(void);
	Bitu		RealSaveState			(void);
	Bitu		ProtSaveState			(void);

	// virtual interrupt flag
	bool		GetVirtualIntFlag		(void);
	void		SetVirtualIntFlag		(bool on);

	// Internal Stack for saving processor status
	void		PushStack				(Bitu val)	{ saveStack[savePtr++] = val;  };
	Bitu		PopStack				(void)		{ return saveStack[--savePtr]; };
	void		SaveSegments			(void);
	void		SaveRegister			(void);
	void		RestoreSegments			(void);
	void		RestoreRegister			(void);
	
	void		CopyRegistersToBuffer	(PhysPt data);
	void		LoadRegistersFromBuffer	(PhysPt data);

	void		ProvideRealModeStack	(PhysPt prStack, Bitu toCopy);
	void		UpdateRealModeStack		(void);

	// xms handle information
	void		SetXMSHandle			(Bitu handle);
	void		ClearXMSHandles			(void);
	void		FreeXMSHandle			(Bitu handle);
	
	// special msdos api stuff
	Bitu		GetSegmentFromSelector	(Bitu selector);
	bool		GetMsdosSelector		(Bitu realseg, Bitu realoff, Bitu &protsel, Bitu &protoff);
	void		API_Init_MSDOS			(void);
	Bitu		API_Entry_MSDOS			(void);
	Bitu		API_Int21_MSDOS			(void);

private:
	Bitu saveStack[DPMI_SAVESTACK_MAX];
	Bitu savePtr;
	Bitu rm_ss, rm_sp;

	struct {
		struct {
			bool have;
			bool bit32;
			Bitu psp;
		} client;
		Bit16u mem_handle;		/* Handle for GDT/IDT */
		struct {
			PhysPt base;
			Bitu limit;
		} gdt,idt,ldt;
		struct {
			bool	inCall;
			bool	inUse;
			bool	stop;
			Bitu	callCount;
			Bitu	id;
			Bitu	dataSelector,dataOffset;
			Bitu	codeSelector,codeOffset;
			Bitu	realSegment ,realOffset;
		} rmCallback[DPMI_REALMODE_CALLBACK_MAX];

		RealPt	realModeVec	[DPMI_REALVEC_MAX];
		Bitu	oldRealVec	[DPMI_REALVEC_MAX];
		Bitu	defaultHWIntFromProtMode[DPMI_REALVEC_MAX];

		PhysPt	ptorint_base;		/* Base of pmode int handlers that reflect to realmode */
		Bitu	exceptionSelector[DPMI_EXCEPTION_MAX],exceptionOffset[DPMI_EXCEPTION_MAX];

		Bitu	xmsHandles[DPMI_XMSHANDLES_MAX];
		Bitu	protStack;

		Bitu	protStackSelector[DPMI_PROTMODE_STACK_MAX];
		Bitu	realStackSelector[DPMI_PROTMODE_STACK_MAX];
		Bitu	dataSelector	 [DPMI_PROTMODE_STACK_MAX]; 
		Bitu	protStackCurrent;

		Bitu	vIntFlag;

		bool	pharlap;
		bool	suppressRMCB;
	} dpmi;

	Bit32u*	modIntTable;
	DPMI*	prevDPMI;
	
	Bitu	dtaAddress;
	Bitu	save_cs[2],save_ds[2],save_es[2],save_fs[2],save_gs[2],save_ss[2];
	Bitu	save_eax[2],save_ebx[2],save_ecx[2],save_edx[2],save_esi[2],save_edi[2];
	Bitu	save_ebp[2],save_esp[2],save_eip[2],save_fl[2];
};

struct {
	Bitu entry;
	Bitu ptorint;
	Bitu ptorintReturn;
	Bitu int31;
	Bitu int21;
	Bitu int21Return;
	Bitu int2f;
	Bitu enterpmode;
	Bitu enterrmode;
	Bitu protsavestate;
	Bitu realsavestate;
	Bitu simint;
	Bitu simintReturn;
	Bitu rmIntFrame;
	Bitu rmIntFrameReturn;
	Bitu rmCallbackReturn;
	Bitu exception;
	Bitu exceptionret;
	// Special callbacks for special dos extenders
	Bitu apimsdosentry;
	Bitu int21msdos;
} callback;

// ************************************************
// DPMI static functions
// ************************************************

static DPMI*	activeDPMI	= 0;
static Bit32u	originalIntTable[256];

bool DPMI_IsActive(void)
{
	return (cpu.cr0 & CR0_PROTECTION) && activeDPMI && activeDPMI->HasClient();
}

void DPMI_SetVirtualIntFlag(bool on) 
{
	if (activeDPMI) activeDPMI->SetVirtualIntFlag(on);
}

void DPMI_CreateException(Bitu num, Bitu errorCode)
{
	if (activeDPMI) activeDPMI->CreateException(num,errorCode);
}

// ************************************************
// DPMI Methods
// ************************************************

DPMI::DPMI(void)
{
	memset(&dpmi,0,sizeof(dpmi));
	savePtr			= 0; 
	dtaAddress		= 0; 
	rm_ss = rm_sp	= 0;
	modIntTable		= 0;
	prevDPMI		= activeDPMI;
};

DPMI::~DPMI(void)
{
	MEM_ReleasePages(dpmi.mem_handle);	
	// TODO: Free all memory allocated with DOS_GetMemory
	// Activate previous dpmi
	activeDPMI = prevDPMI;
};

void DPMI::ClearXMSHandles(void)
{
	for (Bitu i=0; i<DPMI_XMSHANDLES_MAX; i++) dpmi.xmsHandles[i]=DPMI_XMSHANDLE_FREE;
};

void DPMI::SetXMSHandle(Bitu handle) {
	for (Bitu i=0; i<DPMI_XMSHANDLES_MAX; i++) {
		if (dpmi.xmsHandles[i]==DPMI_XMSHANDLE_FREE) {
			dpmi.xmsHandles[i]=handle;
			return;
		}
	};
	E_Exit("DPMI: No more DPMI XMS Handles available.");
};

void DPMI::FreeXMSHandle(Bitu handle) {
	for (Bitu i=0; i<DPMI_XMSHANDLES_MAX; i++) {
		if (dpmi.xmsHandles[i]==handle) {
			dpmi.xmsHandles[i]=DPMI_XMSHANDLE_FREE;
			break;
		}
	}
};

void DPMI::SaveSegments(void) 
{
	if (savePtr+5>=DPMI_SAVESTACK_MAX) E_Exit("DPMI: Stack too small.");
	saveStack[savePtr++] = SegValue(ds);
	saveStack[savePtr++] = SegValue(es);
	saveStack[savePtr++] = SegValue(fs);
	saveStack[savePtr++] = SegValue(gs);
	saveStack[savePtr++] = SegValue(ss);
}

void DPMI::SaveRegister(void) 
{
	SaveSegments();
	if (savePtr+8>=DPMI_SAVESTACK_MAX) E_Exit("DPMI: Stack too small.");
	saveStack[savePtr++] = reg_eax;
	saveStack[savePtr++] = reg_ebx;
	saveStack[savePtr++] = reg_ecx;
	saveStack[savePtr++] = reg_edx;
	saveStack[savePtr++] = reg_esi;
	saveStack[savePtr++] = reg_edi;
	saveStack[savePtr++] = reg_ebp;
	saveStack[savePtr++] = reg_esp;
};

void DPMI::RestoreSegments(void) 
{
	CPU_SetSegGeneral(ss,saveStack[--savePtr]);
	CPU_SetSegGeneral(gs,saveStack[--savePtr]);
	CPU_SetSegGeneral(fs,saveStack[--savePtr]);
	CPU_SetSegGeneral(es,saveStack[--savePtr]);
	CPU_SetSegGeneral(ds,saveStack[--savePtr]);
};

void DPMI::RestoreRegister(void) 
{
	reg_esp = saveStack[--savePtr];
	reg_ebp = saveStack[--savePtr];
	reg_edi = saveStack[--savePtr];
	reg_esi = saveStack[--savePtr];
	reg_edx = saveStack[--savePtr];
	reg_ecx = saveStack[--savePtr];
	reg_ebx = saveStack[--savePtr];
	reg_eax = saveStack[--savePtr];
	RestoreSegments();
};

void DPMI::CopyRegistersToBuffer(PhysPt data)
{
	// Save Values in structure
	mem_writed(data+0x00, reg_edi);
	mem_writed(data+0x04, reg_esi);
	mem_writed(data+0x08, reg_ebp);
	mem_writed(data+0x0C, 0x0000);
	mem_writed(data+0x10, reg_ebx);
	mem_writed(data+0x14, reg_edx);
	mem_writed(data+0x18, reg_ecx);
	mem_writed(data+0x1C, reg_eax);
	mem_writew(data+0x20, flags.word);
	mem_writew(data+0x22, SegValue(es));
	mem_writew(data+0x24, SegValue(ds));
	mem_writew(data+0x26, SegValue(fs));
	mem_writew(data+0x28, SegValue(gs));
	mem_writew(data+0x2A, reg_ip);
	mem_writew(data+0x2C, SegValue(cs));
	mem_writew(data+0x2E, reg_sp);
	mem_writew(data+0x30, SegValue(ss));
}

void DPMI::LoadRegistersFromBuffer(PhysPt data)
{
	reg_edi = mem_readd(data+0x00);
	reg_esi = mem_readd(data+0x04);
	reg_ebp = mem_readd(data+0x08);
	reg_ebx = mem_readd(data+0x10);
	reg_edx = mem_readd(data+0x14);
	reg_ecx	= mem_readd(data+0x18);
	reg_eax	= mem_readd(data+0x1C);
	CPU_SetFlagsw(mem_readw(data+0x20));
	SegSet16(es,mem_readw(data+0x22));
	SegSet16(ds,mem_readw(data+0x24));
	SegSet16(fs,mem_readw(data+0x26));
	SegSet16(gs,mem_readw(data+0x28));
	reg_esp	= mem_readw(data+0x2E);
	SegSet16(ss,mem_readw(data+0x30));	
	if (!dpmi.client.bit32) {
		reg_eax &= 0xFFFF;
		reg_ebx &= 0xFFFF;
		reg_ecx &= 0xFFFF;
		reg_edx &= 0xFFFF;
		reg_edi &= 0xFFFF;
		reg_esi &= 0xFFFF;
		reg_ebp &= 0xFFFF;
		reg_esp &= 0xFFFF;
	};
};

void DPMI::ProvideRealModeStack(PhysPt prStack, Bitu toCopy) 
{
	// Check stack, if zero provide it
	if ((SegValue(ss)==0) && (reg_sp==0)) {
		SegSet16(ss,rm_ss);
		reg_esp = rm_sp;
	} else {
		if (SegValue(ss)==rm_ss) reg_esp = rm_sp;
	};
	// We have to be in realmode here
	if (toCopy>0) {
		Bitu numBytes = toCopy*2;
		if (reg_esp<numBytes) E_Exit("DPMI:CopyStack: SP invalid.");
		PhysPt targetStack = SegValue(ss)+reg_esp-numBytes;
		MEM_BlockCopy(targetStack,prStack,numBytes);
		reg_esp -= numBytes;
	}
};

void DPMI::UpdateRealModeStack()
{
	if (SegValue(ss)==rm_ss) {
		if (reg_esp>DPMI_REALMODE_STACKSIZE) E_Exit("DPMI:Realmode stack out of range: %04X",reg_esp);
		rm_sp = reg_sp;
	}
};

Bitu DPMI::AllocateLDTDescriptor(Bitu count,Bitu & base) {
	
	SetDescriptor test;
	Bitu i=16, found=0;
	PhysPt address = dpmi.ldt.base + LDT_FIRSTSELECTOR*8;
	while (i<LDT_SIZE) {
		test.Load(address);
		if (test.saved.seg.p)	found=0;
		else					found++;	
		i++;
		address+=8;
		if (found==count) {
			/* Init allocated descriptors */
			test.Clear();
			test.SetType(DESC_DATA_EU_RW_NA);
			test.saved.seg.p   = 1;
			test.saved.seg.big = dpmi.client.bit32;
			test.saved.seg.dpl = DPMI_DPL;
			base	= ((i-found) << 3)|(4|DPMI_DPL);	/* Make it an LDT Entry */
			address	= dpmi.ldt.base+(base & ~7);
			for (;found>0;found--) {
				test.Save(address);
				address+=8;
			}
			return true;
		}
	}
	return false;
}

Bitu DPMI::AllocateLDTDescriptor2(Bitu count,Bitu & base) {
	
	static Bitu allocated = 0;

	SetDescriptor desc;
	Bitu nr = LDT_FIRSTSELECTOR + allocated;
	if (nr+count < LDT_SIZE) {
		desc.Clear();
		desc.SetType(DESC_DATA_EU_RW_NA);
		desc.saved.seg.p   = 1;
		desc.saved.seg.big = dpmi.client.bit32;
		desc.saved.seg.dpl = DPMI_DPL;
		base = (nr << 3)|(4|DPMI_DPL);	/* Make it an LDT Entry */
		allocated += count;
		Bitu address = dpmi.ldt.base+(base & ~7);
		for (;count>0;count--) {
			desc.Save(address);
			address+=8;
		}		
		return true;
	};
	return false;
}

Bitu DPMI::CreateAlias(Bitu selector, Bit16u& alias)
{
	Descriptor oldDesc;
	Bitu base;
	if (!cpu.gdt.GetDescriptor(selector,oldDesc))	{ alias = DPMI_ERROR_INVALID_SELECTOR; return false; };
	if (!AllocateLDTDescriptor(1,base))				{ alias = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE; return false; };
	SetDescriptor desc;
	desc.Clear();
	desc.SetLimit(oldDesc.GetLimit());
	desc.SetBase (oldDesc.GetBase());
	desc.SetType (DESC_DATA_ED_RW_A);
	desc.saved.seg.p=1;
	desc.saved.seg.dpl = DPMI_DPL;
	desc.Save    (dpmi.ldt.base+(base & ~7));
	alias = base;
	return true;
};

void DPMI::ReloadSegments(Bitu selector)
{
	if (SegValue(cs)==selector) CPU_SetSegGeneral(cs,selector);
	if (SegValue(ds)==selector) CPU_SetSegGeneral(ds,selector);
	if (SegValue(es)==selector) CPU_SetSegGeneral(es,selector);
	if (SegValue(fs)==selector) CPU_SetSegGeneral(fs,selector);
	if (SegValue(gs)==selector) CPU_SetSegGeneral(gs,selector);
	if (SegValue(ss)==selector) CPU_SetSegGeneral(ss,selector);
};

void DPMI::CreateException(Bitu num, Bitu errorCode)
{
	if (dpmi.client.bit32) {
		CPU_Push32(SegValue(ss));
		CPU_Push32(reg_esp);
		CPU_Push32(flags.word);
		CPU_Push32(SegValue(cs));
		CPU_Push32(reg_eip-2);						// FIXME: Fake !
		CPU_Push32(errorCode);
		CPU_Push32(GDT_PROTCODE);					// return cs
		CPU_Push32(DPMI_CB_EXCEPTIONRETURN_OFFSET);	// return eip
	} else {
		CPU_Push16(SegValue(ss));
		CPU_Push16(reg_sp);
		CPU_Push16(flags.word);
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip-2);						// FIXME: Fake !
		CPU_Push16(errorCode);
		CPU_Push16(GDT_PROTCODE);					// return cs
		CPU_Push16(DPMI_CB_EXCEPTIONRETURN_OFFSET);	// return eip
	};
	DPMI_LOG("DPMI: Exception occured : %04X (%04X:%08X)",num,dpmi.exceptionSelector[num],dpmi.exceptionOffset[num]);
	CPU_JMP(dpmi.client.bit32,dpmi.exceptionSelector[num],dpmi.exceptionOffset[num]);
};

Bitu DPMI::ExceptionReturn(void) 
{
	Bitu error;
	// Restore Registers
	Bitu newcs;
	if (dpmi.client.bit32) {
		error	= CPU_Pop32();
		reg_eip = CPU_Pop32();
		newcs	= CPU_Pop32();
		CPU_SetFlagsd(CPU_Pop32());
		reg_esp = CPU_Pop32();
		CPU_SetSegGeneral(ss,CPU_Pop32());
	} else {
		error	= CPU_Pop16();
		reg_eip = CPU_Pop16();
		newcs	= CPU_Pop16();
		CPU_SetFlagsw(CPU_Pop16());
		reg_esp = CPU_Pop16();
		CPU_SetSegGeneral(ss,CPU_Pop16());
	};
	DPMI_LOG("DPMI: Return from Exception. Jump to %04X:%08X",SegValue(cs),reg_eip);
	CPU_JMP(dpmi.client.bit32,newcs,reg_eip);
	return 0;
};

void DPMI::RemoveIntCallbacks()
// When switching dpmi clients, remove active callbacks from hardware int
{
	Bitu i;
	modIntTable = new Bit32u[256];
	// read and store interrupt table
	for (i=0; i<256; i++) modIntTable[i] = mem_readd(i*4);
	// set a clean interrupt table
	for (i=0; i<256; i++) mem_writed(i*4,originalIntTable[i]);
};

void DPMI::RestoreIntCallbacks()
{
	if (modIntTable) {
		// restore modified interrupt table
		for (int i=0; i<256; i++) mem_writed(i*4,modIntTable[i]);
		delete[] modIntTable; modIntTable = 0;
	}
};

bool DPMI::AllocateRealModeCallback(Bitu codeSel,Bitu codeOff,Bitu dataSel, Bitu dataOff, Bitu& segment, Bitu& offset)
{
	Bitu num = 0;
	for	(Bitu i=0; i<DPMI_REALMODE_CALLBACK_MAX; i++) if (!dpmi.rmCallback[i].inUse) { num = i; break; };
	if (num<DPMI_REALMODE_CALLBACK_MAX) {
		RealPt entry = CALLBACK_RealPointer(dpmi.rmCallback[num].id);
		dpmi.rmCallback[num].codeSelector	= codeSel;
		dpmi.rmCallback[num].codeOffset		= codeOff;
		dpmi.rmCallback[num].dataSelector	= dataSel;
		dpmi.rmCallback[num].dataOffset		= dataOff;
		dpmi.rmCallback[num].realSegment	= segment = RealSeg(entry);
		dpmi.rmCallback[num].realOffset		= offset  = RealOff(entry);
		dpmi.rmCallback[num].inCall			= false;
		dpmi.rmCallback[num].inUse			= true;
		dpmi.rmCallback[num].callCount		= 0;
		return true;
	} 
	return false;
};

Bitu DPMI::RealModeCallback(void)
{
	// Call protected mode function		
	Bitu num = mem_readw(PhysPt(SegPhys(cs)+reg_eip-2));
	num -= dpmi.rmCallback[0].id;
	if ((num>=DPMI_REALMODE_CALLBACK_MAX) || !dpmi.rmCallback[num].inUse) E_Exit("DPMI: Illegal Realmode callback %02X.",num);

	if (dpmi.rmCallback[num].inCall) DPMI_LOG("DPMI: Recursive Realmode callback %02X",num);
	if (dpmi.protStackCurrent>DPMI_PROTMODE_STACK_MAX) E_Exit("DPMI: Too many recursive Realmode callbacks. Stack failure.");

	PushStack(num);

	DPMI_LOG("DPMI: Realmode Callback %02X (%04X:%08X) enter",num,dpmi.rmCallback[num].codeSelector,dpmi.rmCallback[num].codeOffset);
	dpmi.rmCallback[num].inCall= true;
	dpmi.rmCallback[num].callCount++;

	// Important! Update realmode stack
	UpdateRealModeStack();	
	// Setup stack selector of real mode stack
	SetDescriptor desc;
	if (cpu.gdt.GetDescriptor(dpmi.realStackSelector[dpmi.protStackCurrent],desc)) {
		desc.SetBase (SegValue(ss)<<4);
		desc.SetLimit(0xFFFF);
		desc.Save	 (dpmi.ldt.base+(dpmi.realStackSelector[dpmi.protStackCurrent] & ~7));
	} else E_Exit("DPMI: RealmodeCB: Could not provide real mode stack descriptor.");
	/* Switch to protected mode */
	CPU_SET_CRX(0,cpu.cr0 | CR0_PROTECTION);
	// Setup dataSelector	
	Descriptor data;
	Bitu dataSelector;
	if (dpmi.rmCallback[num].dataSelector==0x0000)	dataSelector = dpmi.dataSelector[dpmi.protStackCurrent];
	else											dataSelector = dpmi.rmCallback[num].dataSelector;
	if (!cpu.gdt.GetDescriptor(dataSelector,data)) E_Exit("DPMI: Init RM-Callback failed.");

	DPMI_LOG("DPMI: CB: Writing RegData at = %04X:%04X",dataSelector,dpmi.rmCallback[num].dataOffset);
	// Prepare data buffer
	CopyRegistersToBuffer(PhysPt(data.GetBase()+dpmi.rmCallback[num].dataOffset));
	DPMI_LOG("DPMI: CB: Stored cs:ip = %04X:%04X",SegValue(cs),reg_ip);
	// setup registers for protected mode func
	CPU_SetSegGeneral(ds,dpmi.realStackSelector[dpmi.protStackCurrent]);	// DS:ESI = RM Stack
	reg_esi = reg_esp;				
	CPU_SetSegGeneral(es,dataSelector);				// ES:EDI = RM Register data
	reg_edi = dpmi.rmCallback[num].dataOffset;
	// SS:ESP = API stack
	CPU_SetSegGeneral(ss,dpmi.protStackSelector[dpmi.protStackCurrent++]);	
	reg_esp = DPMI_PROTMODE_STACKSIZE;
	// prepare stack for iret
	if (dpmi.client.bit32) CPU_Push32(flags.word); else CPU_Push16(flags.word);
	// Setup cs:ip to return to DPMI_ReturnFromRealModeCallback
	CPU_SetSegGeneral(cs,GDT_CODE);
	reg_eip = RealOff(CALLBACK_RealPointer(callback.rmCallbackReturn));
	// call protected mode func
	SETFLAGBIT(IF,false);
	SETFLAGBIT(TF,false);
	CPU_Push32(flags.word);
	CPU_CALL(dpmi.client.bit32,dpmi.rmCallback[num].codeSelector,dpmi.rmCallback[num].codeOffset);
	return 0;
};

Bitu DPMI::RealModeCallbackReturn(void)
{
	// returning from protected mode function, now back to real mode
	Bitu num = PopStack();
	DPMI_LOG("DPMI: Realmode Callback leave %02X",num);
	dpmi.suppressRMCB = false;
	dpmi.rmCallback[num].inCall = false;
	dpmi.rmCallback[num].stop   = false;
	dpmi.rmCallback[num].callCount--;
	PhysPt data = PhysPt(SegPhys(es)+reg_edi);
	DPMI_LOG("DPMI: CB: Reading RegData at = %04X:%04X",SegValue(es),reg_edi);
	/* Swtich to real mode */
	CPU_SET_CRX(0,cpu.cr0 & ~CR0_PROTECTION);
	dpmi.protStackCurrent--;
	// Restore Registers		
	LoadRegistersFromBuffer(data);		
	Bitu newCS = mem_readw(data+0x2C);
	Bitu newIP = mem_readw(data+0x2A);	
	UpdateRealModeStack();
	DPMI_LOG("DPMI: CB: Retored cs:ip = %04X:%04X (%d)",newCS,newIP);
	CPU_JMP(false,newCS,newIP);
	return 0;
};

static Bitu count = 0;

Bitu DPMI::CallRealIRETFrame(void)
{
	Bitu calledIP = mem_readd(SegPhys(ss)+reg_esp);
	Bitu calledCS = mem_readd(SegPhys(ss)+reg_esp+4);
	DPMI_LOG("DPMI: ENTER REAL PROC IRETF %04X:%08X",calledCS,calledIP);
	// Save changed registers
	PushStack(SegValue(cs));
	SaveRegister();
	Bitu toCopy = reg_cx;
	// Load Registers
	PhysPt data		= SegPhys(es) + reg_edi;
	PhysPt prStack	= SegPhys(ss) + reg_esp;
	LoadRegistersFromBuffer(data);
	PushStack(data);
	/* Switch to real mode */
	CPU_SET_CRX(0,cpu.cr0 & ~CR0_PROTECTION);
	// Provide Stack
	ProvideRealModeStack(prStack,toCopy);
	// Push flags
	CPU_Push16(flags.word);
	// Setup IP
	Bitu newCS = mem_readw(data+0x2C);
	Bitu newIP = mem_readw(data+0x2A);
	// Setup cs:ip to return to DPMI_CallRealIRETFrame callback
	SegSet16(cs,RealSeg(CALLBACK_RealPointer(callback.rmIntFrameReturn)));
	reg_ip = RealOff(CALLBACK_RealPointer(callback.rmIntFrameReturn));
	SETFLAGBIT(IF,false);
	SETFLAGBIT(TF,false);
	CPU_CALL(false,newCS,newIP);
	return 0;
}

Bitu DPMI::CallRealIRETFrameReturn(void)
{
	UpdateRealModeStack();
	// returning from realmode func
	DPMI_LOG("DPMI: LEAVE REAL PROC IRETF %d",count);
	/* Switch to protected mode */
	CPU_SET_CRX(0,cpu.cr0 | CR0_PROTECTION);
	// Save registers into real mode structure
	CopyRegistersToBuffer(PopStack());
	// Restore changed Resgisters
	RestoreRegister();
	Bitu newcs = PopStack();
	
	CPU_JMP(dpmi.client.bit32,newcs,reg_eip);

	DPMI_CALLBACK_SCF(false);
	return 0;
};

Bitu DPMI::SimulateInt(void)
{
	Bitu num = reg_bl;
	DPMI_LOG("DPMI: SIM INT %02X %04X called. cs = %04X",num,reg_ax,SegValue(cs));
	// Save changed registers
	PushStack(SegValue(cs));
	SaveRegister();
	Bitu toCopy = reg_cx;
	// Load Registers
	PhysPt data = SegPhys(es) + reg_edi;
	PhysPt prStack = SegPhys(ss) + reg_esp;
	LoadRegistersFromBuffer(data);
	PushStack(data);
	/* Switch to real mode */
	CPU_SET_CRX(0,cpu.cr0 & ~CR0_PROTECTION);
	// Provide Stack
	ProvideRealModeStack(prStack,toCopy);
	// prepare for return
	SegSet16(cs,RealSeg(CALLBACK_RealPointer(callback.simintReturn)));
	reg_ip = RealOff(CALLBACK_RealPointer(callback.simintReturn));
	// Push flags from structure on stack
	DPMI_LOG("DPMI: SimInt1: StackInfo %04X:%04X (%02X %02X)",SegValue(ss),reg_esp,mem_readb(0xD0100+0x01FA),mem_readb(0xD0100+0x01FB));
	flags.word = mem_readw(data+0x20);
	Interrupt(num);
	DPMI_LOG("DPMI: SimInt2: StackInfo %04X:%04X (%02X %02X)",SegValue(ss),reg_esp,mem_readb(0xD0100+0x01FA),mem_readb(0xD0100+0x01FB));
	return 0;
};

Bitu DPMI::SimulateIntReturn(void)
{
	// returning from realmode func
	DPMI_LOG("DPMI: SIM INT return");
	DPMI_LOG("DPMI: SimIntRet1: StackInfo %04X:%04X (%02X %02X)",SegValue(ss),reg_esp,mem_readb(0xD0100+0x01FA),mem_readb(0xD0100+0x01FB));

	UpdateRealModeStack();
	/* Switch to protected mode */
	CPU_SET_CRX(0,cpu.cr0 | CR0_PROTECTION);
	// Save registers into real mode structure
	CopyRegistersToBuffer(PopStack());
	// Restore changed Resgisters
	RestoreRegister();
	Bitu newcs = PopStack();
	DPMI_LOG("DPMI: SimIntRet: JUMP to %04X:%08X",newcs,reg_eip);
	CPU_JMP(dpmi.client.bit32,newcs,reg_eip);
	// Free last realmode stack
	DPMI_CALLBACK_SCF(false);
	DPMI_LOG("DPMI: SimIntRet2: StackInfo %04X:%04X (%02X %02X)",SegValue(ss),reg_esp,mem_readb(0xD0100+0x01FA),mem_readb(0xD0100+0x01FB));
	return 0;
};

void DPMI::PrepareReflectToReal(Bitu num)
{
	// Save segment and stack register
	SaveSegments();
	PushStack(reg_esp); 
	PushStack(num);
	PushStack(reg_eip);
	PushStack(SegValue(cs));
	/* Swtich to real mode */
	CPU_SET_CRX(0,cpu.cr0 & ~CR0_PROTECTION);
	// Setup cs:ip to return to intreturn
	Bitu retcs = RealSeg(CALLBACK_RealPointer(callback.ptorintReturn));
	Bitu retip = RealOff(CALLBACK_RealPointer(callback.ptorintReturn));
	
	SegSet16(cs,RealSeg(CALLBACK_RealPointer(callback.ptorintReturn)));
	reg_ip = RealOff(CALLBACK_RealPointer(callback.ptorintReturn));
	// setup stack
	SegSet16(ss,rm_ss);
	reg_esp = rm_sp;
}

Bitu DPMI::ptorHandler(void) 
{
	/* Pmode interrupt handler that maps the interrupt to realmode */
	Bitu num = reg_eip >> 3;
	if (!dpmi.vIntFlag) {
		if ((num>=0x08) && (num<=0x0F)) return 0;
		if ((num>=0x70) && (num<=0x77)) return 0;
	};
	PrepareReflectToReal(num);	
//	if (num==0x0F)
	DPMI_LOG("DPMI: INT %02X %04X called.",num,reg_ax);
	// Prepare flags for real int
	// CPU_SetFlagsw(flags.word & 0x3ED5); // 0011111011010101b
	Interrupt(num);
	return 0;
} 

Bitu DPMI::ptorHandlerReturn(void) 
// Return from reflected real mode int
{
	UpdateRealModeStack();
	/* Switch to protected mode */
	CPU_SET_CRX(0,cpu.cr0 | CR0_PROTECTION);
	// Restore Registers
	Bitu newcs	= PopStack();
	reg_eip		= PopStack();
	Bitu num	= PopStack();
	reg_esp		= PopStack(); 
	RestoreSegments();
//	if (num==0x0F) 
		DPMI_LOG("DPMI: INT %02X RETURN",num);	
	// hardware ints exit here
	if (((num>=0x08) && (num<=0x0F)) || ((num>=0x70) && (num<=0x77))) {
		CPU_JMP(dpmi.client.bit32,newcs,reg_eip);
		return 0;
	}
	// Change flags on stack to reflect possible results from ints
	if (dpmi.client.bit32) {
		Bit32u oldFlags  = mem_readd(SegPhys(ss)+reg_esp+8) & ~FLAG_MASK;// leave only flags that cannot be changed by int 
		Bit32u userFlags = flags.word & FLAG_MASK;						 // Mask out illegal flags not to change by int (0011111011010101b)
		mem_writed(SegPhys(ss)+reg_esp+8,oldFlags|userFlags);
	} else {
		Bit16u oldFlags  = mem_readw(SegPhys(ss)+reg_sp+4) & ~FLAG_MASK; // leave only flags that cannot be changed by int 
		Bit16u userFlags = flags.word & FLAG_MASK;						 // Mask out illegal flags not to change by int (0011111011010101b)
		mem_writew(SegPhys(ss)+reg_sp+4,oldFlags|userFlags);
	};
	CPU_JMP(dpmi.client.bit32,newcs,reg_eip);
	return 0;
}

Bitu DPMI::Int21Handler(void) 
{
	// Check for exit
	if (reg_ah==0x4C) {
		DPMI_LOG("DPMI: INT 21: Terminating.");		
		Terminate();
	} 
	// Save segment and stack register
	PushStack(SegValue(ss));
	PushStack(reg_esp);
	PushStack(SegValue(ds));
	PushStack(SegValue(es));
	PushStack(SegValue(cs));

	/* Swtich to real mode */
	CPU_SET_CRX(0,cpu.cr0 & ~CR0_PROTECTION);		
	// Setup cs:ip to return to intreturn
	SegSet16(cs,RealSeg(CALLBACK_RealPointer(callback.int21Return)));
	reg_ip = RealOff(CALLBACK_RealPointer(callback.int21Return));
	// setup stack
	SegSet16(ss,rm_ss);
	reg_esp = rm_sp;
	// Call realmode interrupt
	DPMI_LOG("DPMI: INT 21 %04X called.",reg_ax);
	Interrupt(0x21);
	if (reg_ah==0x4C) {
		// Shut doen dpmi and restore previous one
		delete this;
		if (activeDPMI) activeDPMI->Reactivate();
	}
	return 0;
};

Bitu DPMI::Int21HandlerReturn(void) 
{
	UpdateRealModeStack();
	/* Switch to protected mode */
	CPU_SET_CRX(0,cpu.cr0 | CR0_PROTECTION);
	// Restore Registers
	Bitu newcs = PopStack();
	CPU_SetSegGeneral(es,PopStack());
	CPU_SetSegGeneral(ds,PopStack());
	reg_esp = PopStack(); 
	CPU_SetSegGeneral(ss,PopStack());		
	// Set carry flag
	DPMI_CALLBACK_SCF(flags.word & 1);
	DPMI_LOG("DPMI: INT 21 RETURN");	
	CPU_JMP(dpmi.client.bit32,newcs,reg_eip);
	return 0;
}

Bitu DPMI::HWIntDefaultHandler()
// Wir sind hier im Protected mode
// a) durch einen INTerrupt im protected mode 
// b) durch einen INTerrupt im real mode (durch RMCB)
{
	Bitu index	= mem_readw(PhysPt(SegPhys(cs)+reg_eip-2)) - dpmi.defaultHWIntFromProtMode[0];
	if (index>=DPMI_REALVEC_MAX) E_Exit("DPMI: Illegal realmode interrupt callback: %02X",index);
	Bitu num	= rmIndexToInt[index];

	RealPt vec  = RealGetVec(num);

	if (dpmi.rmCallback[index].callCount==0) {
		// INT PROT (Use Handler is already done).
		// Wenn rmcb noch in Realmode Int table installiert, dann originalMethode aufrufef
		if (vec==dpmi.realModeVec[index]) {
			// originalroutine aufrufen
			dpmi.rmCallback[index].stop = false;
			PrepareReflectToReal(num);
			CPU_Push16(flags.word);
			SETFLAGBIT(IF,false);
			SETFLAGBIT(TF,false);
			CPU_CALL(false,RealSeg(dpmi.oldRealVec[index]),RealOff(dpmi.oldRealVec[index]));
		} else {
			// user real mode handler in real mode int table aktiv. 
			// Moeglich, dass dieser den RMCB von Hand noch aufruft
			// dann wird aber callCount>0 sein (da RMCB aktiv) und 
			// dann die alte Routine aufgerufen...
			// RMCB sperren, um einen erneuten Aufruf des User Handlers zu vermeiden,..

			// This is a hack for cybermage wich wont work otherwise. But why ?
			if (num==0x0F) {
				if (dpmi.suppressRMCB) {
					dpmi.suppressRMCB = false;
					return 0;
				} else {
					dpmi.suppressRMCB = true;
				}
			};					
			PrepareReflectToReal(num);
			SETFLAGBIT(IF,false);
			SETFLAGBIT(TF,false);
			CPU_Push16(flags.word);
			CPU_CALL(false,RealSeg(vec),RealOff(vec));
		}
	} else {
		// INT REAL (vom RMCB aktiviert)
		// Falls user handler schon aktiv war (int von prot->reflected to real)
		// rufe original routine auf
		if (dpmi.rmCallback[index].stop) {
			dpmi.rmCallback[index].stop = false;
			PrepareReflectToReal(num);
			CPU_Push16(flags.word);
			SETFLAGBIT(IF,false);
			SETFLAGBIT(TF,false);
			CPU_CALL(false,RealSeg(dpmi.oldRealVec[index]),RealOff(dpmi.oldRealVec[index]));			
		} else {
			// User routine wurde noch nicht aktiviert, callback aber ausgeführt
			// falls spezieller protected mode handler aktiviert wurde, 
			// wird dieser jetzt aufgerufen (user routine im protected mode)			
			Descriptor gate;
			gate.Load(dpmi.idt.base+num*8);
			if ((gate.GetSelector()!=GDT_CODE) || (gate.GetOffset()!=RealOff(dpmi.defaultHWIntFromProtMode[index]))) {
				dpmi.rmCallback[index].stop = true;	// vermeide rekursion
				CPU_JMP(dpmi.client.bit32,gate.GetSelector(),gate.GetOffset());
			} else {
				// kein spezieller Protmode handler - Rufe originalroutine auf
				PrepareReflectToReal(num);
				CPU_Push16(flags.word);
				SETFLAGBIT(IF,false);
				SETFLAGBIT(TF,false);
				CPU_CALL(false,RealSeg(dpmi.oldRealVec[index]),RealOff(dpmi.oldRealVec[index]));			
			};
		};
	}	
	return 0;
};

void DPMI::SaveRegisterState(Bitu num)
// Copy Current Registers to structure
{
	save_cs[num] = SegValue(cs);
	save_ds[num] = SegValue(ds);
	save_es[num] = SegValue(es);
	save_fs[num] = SegValue(fs);
	save_gs[num] = SegValue(gs);
	save_ss[num] = SegValue(ss);
	save_eip[num] = reg_eip;
	save_eax[num] = reg_eax;
	save_ebx[num] = reg_ebx;
	save_ecx[num] = reg_ecx;
	save_edx[num] = reg_edx;
	save_esi[num] = reg_esi;
	save_edi[num] = reg_edi;
	save_ebp[num] = reg_ebp;
	save_esp[num] = reg_esp;
	save_fl [num] = flags.word;	
};

void DPMI::LoadRegisterState(Bitu num)
// Copy Current Registers to structure
{
	CPU_SetSegGeneral(fs,save_fs[num]);
	CPU_SetSegGeneral(gs,save_gs[num]);
	reg_eax = save_eax[num];
	reg_ebx = save_ebx[num];
	reg_ecx = save_ecx[num];
	reg_edx = save_edx[num];
	reg_esi = save_esi[num];
	reg_edi = save_edi[num];
	flags.word = save_fl [num];	
};

Bitu DPMI::EnterProtMode(void) {

	/* Save real mode register state */
	SaveRegisterState(0);

	/* Switch to protected mode */
	CPU_SET_CRX(0,cpu.cr0 | CR0_PROTECTION);	

	CPU_SetSegGeneral(ds,reg_ax);
	CPU_SetSegGeneral(es,reg_cx);
	CPU_SetSegGeneral(ss,reg_dx);
	CPU_SetSegGeneral(ds,reg_ax);

	if (dpmi.client.bit32) { 
		reg_esp = reg_ebx;
		CPU_JMP(true,reg_si,reg_edi);
	} else {
		reg_sp = reg_bx; 
		CPU_JMP(false,reg_si,reg_di);
	};
	
	/* Load prot mode register state (all other unchanged registers */
	LoadRegisterState(1);

	DPMI_LOG("DPMI: Switch to protected mode.");
	return 0;
}

Bitu DPMI::EnterRealMode(void) {

	/* Save Prot Mode Registers */
	SaveRegisterState(1);

	/* Swtich to real mode */
	CPU_SET_CRX(0,cpu.cr0 & ~CR0_PROTECTION);
	// (E)BP will be preserved across the mode switch call so it can be used as a pointer.
	// TODO: If interrupts are disabled when the mode switch procedure is invoked, 
	// they will not be re-enabled by the DPMI host (even temporarily).
	SegSet16(ds,reg_ax);
	SegSet16(es,reg_cx);
	SegSet16(ss,reg_dx);
	SegSet16(fs,0);
	SegSet16(gs,0);
	if (dpmi.client.bit32) { 
		reg_esp = reg_ebx;
		CPU_JMP(true,reg_si,reg_edi);
	} else {
		reg_sp = reg_bx; 
		CPU_JMP(false,reg_si,reg_di);
	};

	/* Load real mode register state (all other unchanged registers) */
	LoadRegisterState(0);
	DPMI_LOG("DPMI: Switch to real mode.");
	return CBRET_NONE;
};

Bitu DPMI::RealSaveState(void) 
{
	/* Save Protected mode state */
	if (reg_al==0) {		
		PhysPt data = SegPhys(es) + reg_edi;
		mem_writew(data+ 0,save_cs[1]);
		mem_writew(data+ 2,save_ds[1]);
		mem_writew(data+ 4,save_es[1]);
		mem_writew(data+ 6,save_fs[1]);
		mem_writew(data+ 8,save_gs[1]);
		mem_writew(data+10,save_ss[1]);
		mem_writed(data+12,save_eax[1]);
		mem_writed(data+16,save_ebx[1]);		
		mem_writed(data+20,save_ecx[1]);		
		mem_writed(data+24,save_edx[1]);		
		mem_writed(data+28,save_esi[1]);		
		mem_writed(data+32,save_edi[1]);		
		mem_writed(data+36,save_ebp[1]);		
		mem_writed(data+40,save_esp[1]);		
		mem_writed(data+44,save_fl [1]);		
		DPMI_LOG("DPMI: Prot Save State.");
	} else if (reg_al==1) {
		/* restore state of prot mode registers */
		PhysPt data = SegPhys(es) + reg_edi;
		save_cs [1] = mem_readw(data+ 0);
		save_ds [1] = mem_readw(data+ 2);
		save_es [1] = mem_readw(data+ 4);
		save_fs [1] = mem_readw(data+ 6);
		save_gs [1] = mem_readw(data+ 8);
		save_ss [1] = mem_readw(data+10);
		save_eax[1] = mem_readd(data+12);
		save_ebx[1] = mem_readd(data+16);
		save_ecx[1] = mem_readd(data+20);
		save_edx[1] = mem_readd(data+24);
		save_edi[1] = mem_readd(data+28);
		save_esi[1] = mem_readd(data+32);
		save_ebp[1] = mem_readd(data+36);
		save_esp[1] = mem_readd(data+40);
//		save_eip[1] = mem_readd(data+44);
		save_fl [1] = mem_readd(data+44);
		DPMI_LOG("DPMI: Prot Restore State.");
	};
	return CBRET_NONE;
};

Bitu DPMI::ProtSaveState(void) 
{
	if (reg_al==0) {		
		/* Save State of real mode registers */
		PhysPt data = SegPhys(es) + reg_edi;
		mem_writew(data+ 0,save_cs[0]);	
		mem_writew(data+ 2,save_ds[0]);
		mem_writew(data+ 4,save_es[0]);
		mem_writew(data+ 6,save_fs[0]);
		mem_writew(data+ 8,save_gs[0]);
		mem_writew(data+10,save_ss[0]);
		mem_writed(data+12,save_eax[0]);
		mem_writed(data+16,save_ebx[0]);		
		mem_writed(data+20,save_ecx[0]);		
		mem_writed(data+24,save_edx[0]);		
		mem_writed(data+28,save_esi[0]);		
		mem_writed(data+32,save_edi[0]);		
		mem_writed(data+36,save_ebp[0]);		
		mem_writed(data+40,save_esp[0]);		
		mem_writed(data+44,save_eip[0]);		
		mem_writed(data+48,save_fl [0]);		
		DPMI_LOG("DPMI: Real Save State.");
	} else if (reg_al==1) {
		/* restore state of real mode registers */
		PhysPt data = SegPhys(es) + reg_edi;
		save_cs [0] = mem_readw(data+ 0);
		save_ds [0] = mem_readw(data+ 2);
		save_es [0] = mem_readw(data+ 4);
		save_fs [0] = mem_readw(data+ 6);
		save_gs [0] = mem_readw(data+ 8);
		save_ss [0] = mem_readw(data+10);
		save_eax[0] = mem_readd(data+12);
		save_ebx[0] = mem_readd(data+16);
		save_ecx[0] = mem_readd(data+20);
		save_edx[0] = mem_readd(data+24);
		save_edi[0] = mem_readd(data+28);
		save_esi[0] = mem_readd(data+32);
		save_ebp[0] = mem_readd(data+36);
		save_esp[0] = mem_readd(data+40);
		save_eip[0] = mem_readd(data+44);
		save_fl [0] = mem_readd(data+48);
		DPMI_LOG("DPMI: Real Restore State.");
	};
	return CBRET_NONE;
};

bool DPMI::GetVirtualIntFlag(void)
// only to call from int 31 cos it uses the pushed flags on int stack
{
	if (dpmi.client.bit32)	return (mem_readd(SegPhys(ss)+reg_esp+8) & FLAG_IF)>0;
	else					return (mem_readd(SegPhys(ss)+reg_sp+4) & FLAG_IF)>0; 
};

void DPMI::SetVirtualIntFlag(bool on)
{
	dpmi.vIntFlag = on;
};

bool DPMI::AllocateMem(Bitu size, Bitu& outHandle, Bitu& linear)
{
	Bitu pages	= (size/DPMI_PAGE_SIZE) + ((size%DPMI_PAGE_SIZE)>0);	// Convert to 4KB pages
	outHandle	= MEM_AllocatePages(pages,true);
	linear		= outHandle*DPMI_PAGE_SIZE;
	if (outHandle!=0) SetXMSHandle(outHandle);
	return (outHandle!=0);
};

/*
bool DPMI::AllocateMem(Bitu size, Bitu& outHandle, Bitu& linear)
{
	Bit16u	handle;
	Bit32u	address;
	Bitu	pages = (size/1024) + ((size%1024)>0);	// Convert to 1KB pages
	if (XMS_AllocateMemory(pages,handle)==0) {
		if (XMS_LockMemory(handle,address)==0) {
			SetXMSHandle(handle);
			outHandle	= handle;
			linear		= address;
			XMS_UnlockMemory(handle);
			return true;
		}
	}
	return false;
};
*/

Bitu DPMI::Int31Handler(void) 
{
	switch (reg_ax) {
	
		case 0x0000:{// Allocate LDT Descriptors
					Bitu base;
					Descriptor desc;
					if (AllocateLDTDescriptor(reg_cx,base)) { 
						reg_ax = base;
						DPMI_LOG("DPMI: 0000: Allocate %d descriptors: %04X",reg_cx,base);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG_ERROR("DPMI: 0000: Allocate %d descriptors failure",reg_cx);
						reg_ax = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE; 
						DPMI_CALLBACK_SCF(true);
					};
					}; break;
		case 0x0001:{// Free Descriptor
					SetDescriptor desc;
					if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
						desc.saved.seg.p = 0;
						desc.Save	(dpmi.ldt.base+(reg_bx & ~7));
						DPMI_LOG("DPMI: 0001: Free Descriptor: %04X",reg_bx);
						DPMI_CALLBACK_SCF(false);					
					} else {
						DPMI_LOG_ERROR("DPMI: 0001: Free Descriptor failure : %04X",reg_bx);
						reg_ax = DPMI_ERROR_INVALID_SELECTOR;
						DPMI_CALLBACK_SCF(true);					
					};
					}; break;
		case 0x0002:{// Segment to Descriptor
					SetDescriptor desc; Bitu base;
					if (AllocateLDTDescriptor(1,base)) { 
						desc.Load	 (dpmi.ldt.base+(base & ~7));
						desc.SetLimit(0xFFFF);
						desc.SetBase (reg_bx<<4);
						desc.saved.seg.dpl=3;
						desc.Save	 (dpmi.ldt.base+(base & ~7));
						reg_ax = base;
						DPMI_LOG("DPMI: 0000: Seg %04X to Desc: %04X",reg_bx,base);
						DPMI_CALLBACK_SCF(false);
					} else {
						// No more Descriptors available
						DPMI_LOG_ERROR("DPMI: 0002: No more Descriptors available.");
						reg_ax = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE; 
						DPMI_CALLBACK_SCF(true);
					};					
					}; break;
		case 0x0003:// Get Next Selector Increment Value
					reg_ax = 8;				
					DPMI_LOG("DPMI: 0003: Get Selector Inc Value: %04X",reg_ax);
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0004:// undocumented (reserved) lock selector 
		case 0x0005:// undocumented (reserved) unlock selector 
					DPMI_LOG("DPMI: 0004: Undoc: (un)lock selector",reg_ax);
					DPMI_CALLBACK_SCF(true);					
					break;
		case 0x0006:{ // Get Segment Base Address
					SetDescriptor desc;
					if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
						DPMI_LOG("DPMI: 0006: Get Base %04X : B:%08X",reg_bx,desc.GetBase());
						reg_cx = desc.GetBase()>>16;
						reg_dx = desc.GetBase()&0xFFFF;
						DPMI_CALLBACK_SCF(false);					
					} else {
						DPMI_LOG_ERROR("DPMI: 0006: Invalid Selector: %04X",reg_bx);
						reg_ax = DPMI_ERROR_INVALID_SELECTOR;
						DPMI_CALLBACK_SCF(true);					
					};
					}; break;
		case 0x0007:{// Set Segment base address
					SetDescriptor desc;
					if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
						Bitu base;
						if (!dpmi.client.bit32) base = (reg_cl<<16)+reg_dx;
						else					base = (reg_cx<<16)+reg_dx;
						desc.SetBase(base);
						desc.Save	(dpmi.ldt.base+(reg_bx & ~7));
						ReloadSegments(reg_bx);
						DPMI_CALLBACK_SCF(false);
						DPMI_LOG("DPMI: 0007: Set Base %04X : B:%08X",reg_bx,base);
					} else {
						DPMI_LOG_ERROR("DPMI: 0007: Invalid Selector: %04X",reg_bx);
						reg_ax = DPMI_ERROR_INVALID_SELECTOR;
						DPMI_CALLBACK_SCF(true);					
					};
					}; break;
		case 0x0008:{// Set Segment limit
					SetDescriptor desc;				
					if ((!dpmi.client.bit32) && (reg_cx!=0)) {
						// 16-bit DPMI implementations can not set segment limits greater 
						// than 0FFFFh (64K) so CX must be zero when calling						
						DPMI_LOG_ERROR("DPMI: 0008: Set Segment Limit invalid: %04X ",reg_bx);
						reg_ax = DPMI_ERROR_INVALID_VALUE;
						DPMI_CALLBACK_SCF(true);
					} else if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
						desc.SetLimit((reg_cx<<16)+reg_dx);
						desc.Save	 (dpmi.ldt.base+(reg_bx & ~7));
						ReloadSegments(reg_bx);
						DPMI_CALLBACK_SCF(false);
						DPMI_LOG("DPMI: 0008: Set Limit %08X",(reg_cx<<16)+reg_dx);
					} else {
						DPMI_LOG_ERROR("DPMI: 0008: Invalid Selector: %04X",reg_bx);
						reg_ax = DPMI_ERROR_INVALID_SELECTOR;
						DPMI_CALLBACK_SCF(true);					
					};
					}; break;
		case 0x0009:{// Set Descriptor Access Rights
					SetDescriptor desc;
					Bit8u rcl = reg_cl;
					Bit8u rch = reg_ch;
					if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
/*						if (!(reg_cl & 0x10) || (reg_ch & 0x20) || (desc.saved.seg.dpl!=DPMI_DPL)) {
							DPMI_LOG_ERROR("DPMI: 0009: Set Rights %04X : failure",reg_bx);
							reg_ax = DPMI_ERROR_INVALID_VALUE;
							DPMI_CALLBACK_SCF(true);
							break;
						};*/
						desc.SetType (reg_cl&0x1F);
						desc.saved.seg.dpl = DPMI_DPL;
						desc.saved.seg.p   = (reg_cl&0x80)>0;
						desc.saved.seg.avl = (reg_ch&0x10)>0;
						desc.saved.seg.r   = (reg_ch&0x20)>0;
						desc.saved.seg.big = (reg_ch&0x40)>0;
						desc.saved.seg.g   = (reg_ch&0x80)>0;
						desc.Save(dpmi.ldt.base+(reg_bx & ~7));
						ReloadSegments(reg_bx);
						DPMI_CALLBACK_SCF(false);
						DPMI_LOG("DPMI: 0009: Set Rights %04X : %04X",reg_bx,reg_cx);
					} else {
						DPMI_LOG_ERROR("DPMI: 0009: Set Rights %04X : failure",reg_bx);
						reg_ax = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE;
						DPMI_CALLBACK_SCF(true);
					};
					}; break;
		case 0x000A:{// Create Alias Descriptor
					Descriptor desc;
					if (CreateAlias(reg_bx, reg_ax)) {
						DPMI_LOG("DPMI: 000A: Create Alias : %04X - %04X",reg_bx,reg_ax);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_CALLBACK_SCF(true);
						DPMI_LOG_ERROR("DPMI: 000A: Invalid Selector: %04X",reg_bx);
					}; }; break;
		case 0x000B:{//Get Descriptor
					SetDescriptor desc;
					if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
						desc.Save(SegPhys(es)+reg_edi);
						DPMI_CALLBACK_SCF(false);
						DPMI_LOG("DPMI: 000B: Get Descriptor %04X : B:%08X L:%08X",reg_bx,desc.GetBase(),desc.GetLimit());
					} else {
						DPMI_LOG_ERROR("DPMI: 000B: Get Descriptor %04X : failure",reg_bx);
						reg_ax = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE;
						DPMI_CALLBACK_SCF(true);
					};
					}; break;
		case 0x000C:{//Set Descriptor
					SetDescriptor desc;
					if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
						desc.Load   (SegPhys(es)+reg_edi);
						// Check access rights 
/*						if (!(desc.saved.seg.type & 0x10) || (desc.saved.seg.r) || (desc.saved.seg.dpl!=DPMI_DPL)) {
							DPMI_LOG("DPMI: 000C: Set Rights %04X : failure",reg_bx);
							reg_ax = DPMI_ERROR_INVALID_VALUE;
							DPMI_CALLBACK_SCF(true);
							break;
						};*/
						if (!desc.saved.seg.p) {
							DPMI_LOG_ERROR("DPMI: 000C: Set Rights %04X : not present",reg_bx);
							desc.saved.seg.p = 1;
						}
//						desc.saved.seg.type |= 0x10;
//						if (!dpmi.client.bit32) {
//							desc.SetBase	(desc.GetBase() & 0xFFFF);
//							desc.SetLimit	(desc.GetLimit() & 0xFFFF);
//						};
						desc.Save(dpmi.ldt.base+(reg_bx & ~7));
						ReloadSegments(reg_bx);
						DPMI_LOG("DPMI: 000B: Set Descriptor %04X : B:%08X L:%08X : P %01X",reg_bx,desc.GetBase(),desc.GetLimit(),desc.saved.seg.p);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG_ERROR("DPMI: 000C: Set Descriptor %04X failed",reg_bx);
						reg_ax = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE;
						DPMI_CALLBACK_SCF(true);
					};
					}; break;
		case 0x000D:{ // Allocate specific LDT Descriptor : TODO: Support it
					DPMI_LOG("DPMI: 000D: Alloc Specific LDT Selector: %04X",reg_bx);
					SetDescriptor desc; 
					if (cpu.gdt.GetDescriptor(reg_bx,desc)) {
						if (!desc.saved.seg.p) {
							desc.saved.seg.p = 1;
							desc.SetLimit(0);
							desc.SetBase (0);
							desc.Save	 (dpmi.ldt.base+(reg_bx & ~7));						
							DPMI_CALLBACK_SCF(false);					 
							break;
						} else {
							DPMI_LOG_ERROR("DPMI: 000D: Invalid Selector: %04X",reg_bx);
							reg_ax = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE;
						};
					} else  reg_ax = DPMI_ERROR_INVALID_SELECTOR;
					DPMI_CALLBACK_SCF(true);					 
					}; break;
		case 0x0100:{// Allocate DOS Memory Block
					Bit16u blocks = reg_bx;
					DPMI_LOG("DPMI: 0100: Allocate DOS Mem: (%04X Blocks)",blocks);
					if (DOS_AllocateMemory(&reg_ax,&blocks)) {
						// Allocate Selector for block
						SetDescriptor desc; Bitu base; Bitu numDesc;
						numDesc = reg_bx/0x1000 + ((reg_bx%0x1000)>0);
						if (AllocateLDTDescriptor(numDesc,base)) { 
							reg_dx = base;
							// First selector
							if (numDesc>1) {								
								Bitu descBase = reg_ax*16;
								Bitu length	  = reg_bx*16;
								desc.Load	 (dpmi.ldt.base+(base & ~7));
								desc.SetBase (descBase);
								desc.SetLimit(dpmi.client.bit32?length:0xFFFF);
								desc.Save	 (dpmi.ldt.base+(base & ~7));
								for (Bitu i=1; i<numDesc; i++) {							
									base     += 8;
									descBase += 0x10000;
									length   -= 0x10000;
									desc.Load	 (dpmi.ldt.base+(base & ~7));
									desc.SetBase (descBase);
									desc.SetLimit((length<=0x10000)?length-1:0xFFFF);
									desc.Save	 (dpmi.ldt.base+(base & ~7));
								}
							} else {
								// one descriptor
								desc.Load	 (dpmi.ldt.base+(base & ~7));
								desc.SetBase (reg_ax*16);
								desc.SetLimit(reg_bx*16);
								desc.Save	 (dpmi.ldt.base+(base & ~7));
							};
							DPMI_LOG("DPMI: 0100: Allocation success: (%04X)",blocks);
							DPMI_CALLBACK_SCF(false);
						} else {
							// No more Descriptors available
							DPMI_LOG_ERROR("DPMI: 0100: Allocation failure: %04X (No Descriptor)",blocks);
							reg_ax = DPMI_ERROR_DESCRIPTOR_UNAVAILABLE; 
							DPMI_CALLBACK_SCF(true);
						};
					} else {
						DPMI_LOG("DPMI: 0100: Allocation failure : %04X (R:%04X)",reg_bx,blocks);
						reg_bx = blocks;
						reg_ax = 0x008; // Insufficient memory
						DPMI_CALLBACK_SCF(true);
					};
					};break;
		case 0x0101:{// Free DOS Memory Block
					SetDescriptor desc;
					if (cpu.gdt.GetDescriptor(reg_dx,desc)) {
						Bitu sel = reg_dx;
						Bitu seg = desc.GetBase()>>4;
						DOS_MCB mcb(seg-1);
						Bitu size = mcb.GetSize()*16;
						if (DOS_FreeMemory(seg)) {
							while (size>0) {
								desc.Load(dpmi.ldt.base+(sel & ~7));
								desc.saved.seg.p = 0;
								desc.Save(dpmi.ldt.base+(sel & ~7));
								size -= (size>=0x10000)?0x10000:size;
								sel+=8;
							};
							DPMI_CALLBACK_SCF(false);
							DPMI_LOG("DPMI: 0101: Free Dos Mem: %04X",reg_dx);
							break;
						}
					} 
					DPMI_LOG_ERROR("DPMI: 0101: Invalid Selector: %04X",reg_bx);
					reg_ax = DPMI_ERROR_INVALID_SELECTOR;
					DPMI_CALLBACK_SCF(true);
					};break;
		case 0x0200:{// Get Real Mode Interrupt Vector		
					RealPt vec = RealGetVec(reg_bl);
					reg_cx = RealSeg(vec);
					reg_dx = RealOff(vec);
					DPMI_LOG("DPMI: 0200: Get Real Int Vector %02X (%04X:%04X)",reg_bl,reg_cx,reg_dx);
					DPMI_CALLBACK_SCF(false);
					}; break;
		case 0x0201:{// Set Real Mode Interrupt Vector		
					DPMI_LOG("DPMI: 0201: Set Real Int Vector %02X (%04X:%04X)",reg_bl,reg_cx,reg_dx);
					RealSetVec(reg_bl,RealMake(reg_cx,reg_dx));
					DPMI_CALLBACK_SCF(false);
					}; break;
		case 0x0202:// Get Processor Exception Handler Vector
					if (reg_bl<DPMI_EXCEPTION_MAX) {
						reg_cx  = dpmi.exceptionSelector[reg_bl];
						reg_edx = dpmi.exceptionOffset[reg_bl];
						DPMI_LOG("DPMI: 0202: Get Exception Vector %02X (%04X:%08X)",reg_bl,reg_cx,reg_edx);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG_ERROR("DPMI: Get Exception Vector failed : %02X",reg_bl);
						DPMI_CALLBACK_SCF(true);
					};
					break;
		case 0x0203:// Set Processor Exception Handler Vector
					if (reg_bl<DPMI_EXCEPTION_MAX) {
						dpmi.exceptionSelector[reg_bl] = reg_cx;
						dpmi.exceptionOffset[reg_bl] = reg_edx;
						DPMI_LOG("DPMI: 0203: Set Exception Vector %02X (%04X:%08X)",reg_bl,reg_cx,reg_edx);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG_ERROR("DPMI: Set Exception Vector failed : %02X",reg_bl);
						DPMI_CALLBACK_SCF(true);
					};
					break;
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0204:{// Get Protected Mode Interrupt Vector
					SetDescriptor gate;
					gate.Load(dpmi.idt.base+reg_bl*8);
					reg_cx  = gate.GetSelector();
					reg_edx = gate.GetOffset();
					DPMI_LOG("DPMI: 0204: Get Prot Int Vector %02X (%04X:%08X)",reg_bl,reg_cx,reg_edx);
					DPMI_CALLBACK_SCF(false);
					}; break;
		case 0x0205:{// Set Protected Mode Interrupt Vector
					SetDescriptor gate;
					gate.Clear();
					gate.saved.seg.p=1;
					gate.SetSelector(reg_cx);
					gate.SetOffset(reg_edx);
					gate.SetType(dpmi.client.bit32?DESC_386_INT_GATE:DESC_286_INT_GATE);
					gate.saved.seg.dpl = DPMI_DPL;
					gate.Save(dpmi.idt.base+reg_bl*8);
					DPMI_LOG("DPMI: 0205: Set Prot Int Vector %02X (%04X:%08X)",reg_bl,reg_cx,reg_edx);
					DPMI_CALLBACK_SCF(false);
					Bitu num = reg_bl;
					};break;
		case 0x0300:// Simulate Real Mode Interrupt
					SimulateInt();
					break;
		case 0x0302:// Call Real Mode Procedure With IRET Frame
					CallRealIRETFrame();
					break;
		case 0x0303:{//Allocate Real Mode Callback Address
					Bitu num = 0;
					for	(Bitu i=0; i<DPMI_REALMODE_CALLBACK_MAX; i++) if (!dpmi.rmCallback[i].inUse) { num = i; break; };
					if (num<DPMI_REALMODE_CALLBACK_MAX) {
						Bitu segment, offset;
						if (AllocateRealModeCallback(SegValue(ds),reg_esi,SegValue(es),reg_edi,segment,offset)) {
							reg_cx = segment; reg_dx = offset;
							DPMI_LOG("DPMI: 0303: Allocate Callback (%04X:%04X)",reg_cx,reg_dx);
							DPMI_CALLBACK_SCF(false);
							break;
						}
					} 
					DPMI_LOG_ERROR("DPMI: 0303: Callback unavailable.");
					reg_ax = DPMI_ERROR_CALLBACK_UNAVAILABLE;
					DPMI_CALLBACK_SCF(true);
					}; break;		
		case 0x0304:{//Free Real Mode Call-Back Address
					Bitu num = DPMI_REALMODE_CALLBACK_MAX;
					for	(Bitu i=0; i<DPMI_REALMODE_CALLBACK_MAX; i++) {
						if ((dpmi.rmCallback[i].realSegment==reg_cx) && (dpmi.rmCallback[i].realOffset==reg_dx)) {
							num = i; break;
						}
					}
					if (num<DPMI_REALMODE_CALLBACK_MAX) {
						DPMI_LOG("DPMI: 0304: Free Callback (%04X:%04X)",reg_cx,reg_dx);
						dpmi.rmCallback[num].inUse = false;
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG_ERROR("DPMI: 0304: Invalid Callback");
						reg_ax = DPMI_ERROR_INVALID_CALLBACK;
						DPMI_CALLBACK_SCF(true);					
					}
					};break;
		case 0x0305:{// Get State Save/Restore Addresses
					RealPt entry = CALLBACK_RealPointer(callback.realsavestate);
					reg_bx = RealSeg(entry);
					reg_cx = RealOff(entry);
					reg_si = GDT_PROTCODE;
					reg_edi= DPMI_CB_SAVESTATE_OFFSET;
					reg_ax = 0; // 20 bytes buffer needed
					DPMI_CALLBACK_SCF(false);
					DPMI_LOG("DPMI: 0305: Get State Save/Rest : R:%04X:%04X P:%04X:%08X",reg_bx,reg_cx,reg_si,reg_edi);
					}; break;	
		case 0x0306:{// Get raw mode switch address
					RealPt entry=CALLBACK_RealPointer(callback.enterpmode);
					reg_bx = RealSeg(entry);
					reg_cx = RealOff(entry);
					reg_si = GDT_PROTCODE;
					reg_edi= DPMI_CB_ENTERREALMODE_OFFSET;
					DPMI_CALLBACK_SCF(false);
					DPMI_LOG("DPMI: 0306: Get Raw Switch      : R:%04X:%04X P:%04X:%08X",reg_bx,reg_cx,reg_si,reg_edi);
					}; break;	
		case 0x0400:// Get Version
					DPMI_LOG("DPMI: 0400: Get Version");
					reg_ax = 90;		// 0.9
					reg_bx = 0x0003;	// 32 Bit DPMI
					reg_cl = 0x04;		// 486
					reg_dx = 0x0870;	// FIXME: Read this from ports
					DPMI_CALLBACK_SCF(false);					
					break;
		case 0x0401: // Get DPMI Capabilities - always fails in 0.9
					DPMI_LOG("DPMI: 0401: Get Capabilities");
					reg_ax = 0x08;		// CONVENTIONAL MEMORY MAPPING capability supported
					DPMI_CALLBACK_SCF(false);					
					break;
		case 0x0500:{// Get Free Memory Information
					PhysPt data = SegPhys(es) + (dpmi.client.bit32 ? reg_edi:reg_di);
					Bitu large	= MEM_FreeLargest();		
					Bitu total	= MEM_FreeTotal();			
					Bitu size	= large;		
					mem_writed(data+0x00,large*DPMI_PAGE_SIZE);	// Size in bytes
					mem_writed(data+0x04,large);				// total number of pages
					mem_writed(data+0x08,large);				// largest block in pages
					mem_writed(data+0x0C,size);					// total linear address space in pages
					mem_writed(data+0x10,total);				// num of unlocked pages - no info
					mem_writed(data+0x14,total);				// num of physical pages not in use
					mem_writed(data+0x18,size);					// total num of physical pages
					mem_writed(data+0x1C,total);				// free linear address space in pages
					mem_writed(data+0x20,0xFFFFFFFF);			// size of paging file in pages
					mem_writed(data+0x24,0xFFFFFFFF);			// reserved
					mem_writed(data+0x28,0xFFFFFFFF);			// reserved
					mem_writed(data+0x2C,0xFFFFFFFF);			// reserved
					DPMI_CALLBACK_SCF(false);
					DPMI_LOG("DPMI: 0500: Get Mem Info (%d KB total)",total*4);
					}; break;
		case 0x0501:{// Allocate Memory	
					Bitu handle,linear;
					Bitu length = (reg_bx<<16)+reg_cx;
					//DPMI_LOG("DPMI: 0501: Allocate memory (%d KB)",length/1024);
					if (AllocateMem(length,handle,linear)) {
						reg_si = handle>>16; 
						reg_di = handle&0xFFFF;
						reg_bx = linear>>16;
						reg_cx = linear&0xFFFF;
						DPMI_CALLBACK_SCF(false);
						// TEMP
						Bitu total  = MEM_FreeLargest();				// in KB					
						DPMI_LOG("DPMI: 0501: Allocation success: H:%04X%04X (%d KB) (R:%d KB)",reg_si,reg_di,length/1024 + ((length%1024)>0),total*4);
					} else {
						reg_ax = DPMI_ERROR_PHYSICAL_MEMORY_UNAVAILABLE;
						DPMI_CALLBACK_SCF(true);
						// TEMP
						Bitu total = MEM_FreeLargest();				// in KB					
						DPMI_LOG("DPMI: 0501: Allocation failure (%d KB) (R:%d KB)",length/1024 + ((length%1024)>0),total*4);
					};
					}; break;
		case 0x0502://Free Memory Block
					DPMI_LOG("DPMI: 0502: Free Mem: H:%04X%04X",reg_si,reg_di);
					MEM_ReleasePages((reg_si<<16)+reg_di);					
					FreeXMSHandle((reg_si<<16)+reg_di);
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0503:{//Resize Memory Block
					Bitu linear,newHandle;
					Bitu newByte		= (reg_bx<<16)+reg_cx;
					Bitu newSize		= (newByte/DPMI_PAGE_SIZE)+((newByte & (DPMI_PAGE_SIZE-1))>0);
					MemHandle handle	= (reg_si<<16)+reg_di;
					DPMI_LOG("DPMI: 0503: Resize Memory: H:%08X (%d KB)",handle,newSize*4);
					if (MEM_ReAllocatePages(handle,newSize,true)) {
						linear = handle * DPMI_PAGE_SIZE;
						reg_si = handle>>16;
						reg_di = handle&0xFFFF;
						reg_bx = linear>>16;
						reg_cx = linear&0xFFFF;
						DPMI_CALLBACK_SCF(false);					
					} else if (AllocateMem(newByte,newHandle,linear)) {							
						// Not possible, try to allocate
						DPMI_LOG("DPMI: 0503: Reallocated Memory: %d KB",newSize*4);
						reg_si = newHandle>>16;
						reg_di = newHandle&0xFFFF;
						reg_bx = linear>>16;
						reg_cx = linear&0xFFFF;
						// copy contents
						Bitu size = MEM_AllocatedPages(handle);
						if (newSize<size) size = newSize;
						MEM_BlockCopy(linear,handle*DPMI_PAGE_SIZE,size*DPMI_PAGE_SIZE);
						// Release old handle
						MEM_ReleasePages(handle);
						FreeXMSHandle(handle);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG_ERROR("DPMI: 0503: Memory unavailable . %08X",newSize);
						reg_ax = DPMI_ERROR_PHYSICAL_MEMORY_UNAVAILABLE;
						DPMI_CALLBACK_SCF(true);
					};
					}; break;
		case 0x0506:// Get Page Attributes
					DPMI_LOG("DPMI: 0506: Get Page Attributes");
					reg_ax = DPMI_ERROR_UNSUPPORTED;
					DPMI_CALLBACK_SCF(true);
					break;
		case 0x0507:// Set Page Attributes
					DPMI_LOG("DPMI: 0507: Set Page Attributes");
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0509:{//Map Conventional Memory in Memory Block
					Bitu xmsAddress	= reg_esi*DPMI_PAGE_SIZE;
					Bitu handle		= reg_esi;
					Bitu offset		= reg_ebx;
					Bitu numPages	= reg_ecx;
					Bitu linearAdr	= reg_edx;
					if ((linearAdr & 3) || ((xmsAddress+offset) & 3)) {
						// Not page aligned
						DPMI_LOG_ERROR("DPMI: Cannot map conventional memory (address not page aligned).");
						reg_ax = DPMI_ERROR_INVALID_LINEAR_ADDRESS;
						DPMI_CALLBACK_SCF(true);					
						break;
					}
					MEM_MapPagesDirect(linearAdr/DPMI_PAGE_SIZE,(xmsAddress+offset)/DPMI_PAGE_SIZE,numPages);
					DPMI_CALLBACK_SCF(false);
					}; break;
		case 0x0600:{//Lock Linear Region
					DPMI_LOG("DPMI: 0600: Lock Linear Region");
					Bitu address = (reg_bx<<16)+reg_cx;
					Bitu size	 = (reg_si<<16)+reg_di;
					DPMI_CALLBACK_SCF(false);
					}; break;
		case 0x0601://unlock Linear Region
					DPMI_LOG("DPMI: 0601: Unlock Linear Region");
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0602:// Mark Real Mode Region as Pageable
					DPMI_LOG("DPMI: 0602: Mark Realmode Region pageable");
					DPMI_CALLBACK_SCF(false);					
					break;
		case 0x0603:// Relock Real Mode Region
					DPMI_LOG("DPMI: 0603: Relock Realmode Region");
					DPMI_CALLBACK_SCF(false);					
					break;
		case 0x0604:// Get page size
					reg_bx=0; reg_cx=DPMI_PAGE_SIZE;
					DPMI_LOG("DPMI: 0604: Get Page Size: %04X",reg_cx);
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0701:// Undocumented discard page contents 
					DPMI_LOG("DPMI: 0701: Discard Page contents");
					DPMI_CALLBACK_SCF(true);
					break;					
		case 0x0702://Mark Page as Demand Paging Candidate
					DPMI_LOG("DPMI: 0702: Mark page as demand paging candidate");
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0703://Discard Page contents
					DPMI_LOG("DPMI: 0703: Discard Page contents");
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0800:{//Physical Address Mapping
					// bx and cx remain the same linear address = physical address
					Bitu phys	= (reg_bx<<16) + reg_cx;
					Bitu size   = (reg_si<<16) + reg_di;
					DPMI_LOG_ERROR("DPMI: 0800: Phys-adr-map not supported : %08X.(%08X).",phys,size);
					DPMI_CALLBACK_SCF(false);
					}; break;
		case 0x0801:// Free physical address mapping
					DPMI_LOG("DPMI: 0801: Free physical address mapping");
					DPMI_CALLBACK_SCF(false);
					break;
		case 0x0900://Get and Disable Virtual Interrupt State
					reg_al = dpmi.vIntFlag;
					dpmi.vIntFlag = 0;
					DPMI_LOG("DPMI: 0900: Get and disbale vi : %01X",reg_al);
					DPMI_CALLBACK_SCF(false);					
					break;
		case 0x0901://Get and Enable Virtual Interrupt State
					reg_al = dpmi.vIntFlag;
					dpmi.vIntFlag = 1;
					DPMI_LOG("DPMI: 0901: Get and enable vi  : %01X",reg_al);
					DPMI_CALLBACK_SCF(false);					
					break;
		case 0x0902:{//Get Virtual Interrupt State
					reg_al = dpmi.vIntFlag;
					DPMI_LOG("DPMI: 0900: Get vi             : %01X",reg_al);
					DPMI_CALLBACK_SCF(false);
					}; break;
		case 0x0A00:{//Get Vendor Specific API Entry Point
					char name[256];
					MEM_StrCopy(SegPhys(ds)+reg_esi,name,255);
					LOG(LOG_MISC,LOG_WARN)("DPMI: Get API: %s",name);
					if (strcmp(name,"MS-DOS")==0) {
						CPU_SetSegGeneral(es,GDT_PROTCODE);
						reg_edi = DPMI_CB_APIMSDOSENTRY_OFFSET;
						API_Init_MSDOS();
						DPMI_CALLBACK_SCF(false);
					} else if (strstr(name,"PHARLAP")!=0) {
						CPU_SetSegGeneral(es,GDT_PROTCODE);
						reg_edi = DPMI_CB_APIMSDOSENTRY_OFFSET;
						API_Init_MSDOS();
						DPMI_CALLBACK_SCF(false);
						dpmi.pharlap = true;
					} else {
						reg_ax = DPMI_ERROR_UNSUPPORTED;
						DPMI_CALLBACK_SCF(true);
					}
					}; break;
		case 0x0D00:{//Allocate Shared Memory
					PhysPt data = SegPhys(es)+reg_edi;
					Bitu length = mem_readd(data);
					Bitu pages  = (length/DPMI_PAGE_SIZE)+((length%DPMI_PAGE_SIZE)>0);
					Bitu handle,linear;
					if (AllocateMem(length,handle,linear)) {
						DPMI_LOG("DPMI: 0D00: Allocate shared memory (%d KB)",pages*4);
						mem_writed(data+0x04,pages*DPMI_PAGE_SIZE);
						mem_writed(data+0x08,handle);
						mem_writed(data+0x0C,linear);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG_ERROR("DPMI: 0D00: Allocation shared failure (%d KB)",pages*4);
						reg_ax = DPMI_ERROR_PHYSICAL_MEMORY_UNAVAILABLE;
						DPMI_CALLBACK_SCF(true);
					};
					}; break; 
		case 0x0B00:// Set debug watchpoint
		case 0x0B01:// Clear debug watchpoint
					DPMI_CALLBACK_SCF(true);	
					break;
		case 0x0E00:// Get Coprocessor Status
					DPMI_LOG("DPMI: 0E00: Get Coprocessor status");
					reg_ax = 0x45; // nope, no coprocessor
					DPMI_CALLBACK_SCF(false);	
					break;
		case 0x0E01:// Set Coprocessor Emulation
					DPMI_LOG("DPMI: 0E01: Set Coprocessor emulation");
					DPMI_CALLBACK_SCF(true);	// failure
					break;
		default	   :LOG(LOG_MISC,LOG_ERROR)("DPMI: Unsupported func %04X",reg_ax);
					reg_ax = DPMI_ERROR_UNSUPPORTED;	
					DPMI_CALLBACK_SCF(true);	// failure
					break;
	};
	return 0;
}

Bitu DPMI::Int2fHandler(void) 
{
	// Only available in ProtectedMode
	// LOG(LOG_MISC,LOG_WARN)("DPMI: 0x2F %04x",reg_ax);
	switch (reg_ax) {	
	case 0x1686:		/* Get CPU Mode */
		reg_ax = 0;
		break;
	case 0x168A:	// Only available in protected mode
		// Get Vendor-Specific API Entry Point
		char name[256];
		MEM_StrCopy(SegPhys(ds)+reg_esi,name,255);
		LOG(LOG_MISC,LOG_WARN)("DPMI: 0x2F 0x168A: Get Specific API :%s",name);
		if (strcmp(name,"MS-DOS")==0) {
			CPU_SetSegGeneral(es,GDT_PROTCODE);
			reg_edi = DPMI_CB_APIMSDOSENTRY_OFFSET;
			reg_al  = 0x00;	// Success, whatever they want...
			API_Init_MSDOS();
		};
		break;
	default : // reflect to real
			  ptorHandler();
			  break;
	}
	return 0;
};

// *********************************************************************
// Callbacks and Callback-Returns
// *********************************************************************

static Bitu DPMI_ExceptionReturn(void)			{ return activeDPMI->ExceptionReturn(); };
static Bitu DPMI_RealModeCallback(void)			{ return activeDPMI->RealModeCallback(); };
static Bitu DPMI_RealModeCallbackReturn(void)	{ return activeDPMI->RealModeCallbackReturn(); };
static Bitu DPMI_CallRealIRETFrame(void)		{ return activeDPMI->CallRealIRETFrame(); };
static Bitu DPMI_CallRealIRETFrameReturn(void)	{ return activeDPMI->CallRealIRETFrameReturn(); };
static Bitu DPMI_SimulateInt(void)				{ return activeDPMI->SimulateInt(); };
static Bitu DPMI_SimulateIntReturn(void)		{ return activeDPMI->SimulateIntReturn(); };
static Bitu DPMI_ptorHandler(void)				{ return activeDPMI->ptorHandler(); };
static Bitu DPMI_ptorHandlerReturn(void)		{ return activeDPMI->ptorHandlerReturn(); };
static Bitu DPMI_Int21Handler(void)				{ return activeDPMI->Int21Handler(); };
static Bitu DPMI_Int21HandlerReturn(void)		{ return activeDPMI->Int21HandlerReturn(); };
static Bitu DPMI_HWIntDefaultHandler(void)		{ return activeDPMI->HWIntDefaultHandler(); };
static Bitu DPMI_EnterProtMode(void)			{ return activeDPMI->EnterProtMode(); };
static Bitu DPMI_EnterRealMode(void)			{ return activeDPMI->EnterRealMode(); };
static Bitu DPMI_RealSaveState(void)			{ return activeDPMI->RealSaveState(); };
static Bitu DPMI_ProtSaveState(void)			{ return activeDPMI->ProtSaveState(); }; 
static Bitu DPMI_Int2fHandler(void)				{ return activeDPMI->Int2fHandler(); }; 
static Bitu DPMI_Int31Handler(void)				{ return activeDPMI->Int31Handler(); }; 
static Bitu DPMI_API_Int21_MSDOS(void)			{ return activeDPMI->API_Int21_MSDOS(); };
static Bitu DPMI_API_Entry_MSDOS(void)			{ return activeDPMI->API_Entry_MSDOS(); };


// ****************************************************************
// Setup stuff
// ****************************************************************

RealPt DPMI::HookInterrupt(Bitu num, Bitu intHandler)
{
	// Setup realmode hook	
	RealPt	oldVec;
	Bitu	segment, offset;
	// Allocate Realmode callback
	RealPt func = CALLBACK_RealPointer(intHandler);
	if (AllocateRealModeCallback(GDT_CODE,RealOff(func),0x0000,0x0000,segment,offset)) {
		oldVec		= RealGetVec(num);
		RealSetVec(num,RealMake(segment,offset));
	} else E_Exit("DPMI: Couldnt allocate Realmode-Callback for INT %04X",num);
	// Setup protmode hook
	func = CALLBACK_RealPointer(intHandler);
	SetDescriptor gate;
	gate.Load		(dpmi.idt.base+num*8);
	gate.SetSelector(GDT_CODE);
	gate.SetOffset	(RealOff(func));
	gate.Save		(dpmi.idt.base+num*8);
	return oldVec;
}

void DPMI::RestoreHookedInterrupt(Bitu num, RealPt oldVec)
{
	//.Restore hooked int 
	RealSetVec(num,oldVec);
	RealPt func = CALLBACK_RealPointer(callback.ptorint);
	SetDescriptor gate;
	gate.Load		(dpmi.idt.base+num*8);
	gate.SetSelector(GDT_CODE);
	gate.SetOffset	(RealOff(func));
	gate.Save		(dpmi.idt.base+num*8);
}

void DPMI::Terminate(void) 
{
	Bitu i;
	cpu.cpl	= 0;
	dpmi.client.have = false;
	// 1. Clear the LDT 
	for (i=0; i<LDT_SIZE*8; i++) mem_writeb(dpmi.ldt.base+i,0);
	// 2.Deallocate Callbacks
	for (i=0; i<DPMI_REALMODE_CALLBACK_MAX; i++) dpmi.rmCallback[i].inUse = false;
	// 3.Deallocate XMS Memory
	for (i=0; i<DPMI_XMSHANDLES_MAX; i++) if (dpmi.xmsHandles[i]!=0xFFFF) MEM_ReleasePages(dpmi.xmsHandles[i]);
#if DPMI_HOOK_HARDWARE_INTS
	// 4.Restore hooked ints
	for (i=0; i<DPMI_REALVEC_MAX; i++) {
		if (RealGetVec(rmIndexToInt[i])!=0)
			RestoreHookedInterrupt(rmIndexToInt[i],dpmi.oldRealVec[i]);
	}
#endif
};

void DPMI::CreateStackSpace(void)
{
	// Alloocate protected mode stack
	Bitu i,base;
	SetDescriptor desc;
	if (!AllocateLDTDescriptor(DPMI_PROTMODE_STACK_MAX,base)) E_Exit("DPMI: Couldnt allocate protected mode stack for callbacks");
	for (i=0; i<DPMI_PROTMODE_STACK_MAX; i++) {
		dpmi.protStackSelector[i] = base;
		desc.Load	 (dpmi.ldt.base+(base & ~7));
		desc.SetLimit(DPMI_PROTMODE_STACKSIZE-1);
		desc.SetBase (dpmi.protStack + i*DPMI_PROTMODE_STACKSIZE);
		desc.Save	 (dpmi.ldt.base+(base & ~7));		
		base += 8;
	};
	// Allocate Descriptors for real mode stack in realmode callback func
	if (!AllocateLDTDescriptor(DPMI_PROTMODE_STACK_MAX,base)) E_Exit("DPMI: Couldnt allocate real mode stack for callbacks");
	for (i=0; i<DPMI_PROTMODE_STACK_MAX; i++) {
		dpmi.realStackSelector[i] = base;
		base += 8;
	};
	// Allocate Descriptors for real mode datasegment in realmode callback func
	if (!AllocateLDTDescriptor(DPMI_PROTMODE_STACK_MAX,base)) E_Exit("DPMI: Couldnt allocate data area for callbacks");
	for (i=0; i<DPMI_PROTMODE_STACK_MAX; i++) {
		// We need mem & descriptor for register data area
		dpmi.dataSelector[i] = base;
		desc.Load	 (dpmi.ldt.base+(base & ~7));
		desc.SetLimit(63);
		desc.SetBase (DOS_GetMemory(64/16)<<4);
		desc.Save	 (dpmi.ldt.base+(base & ~7));		
		base += 8;
	};

	dpmi.protStackCurrent = 0;	
};

static Bitu DPMI_EntryPoint(void) 
{
	if (!activeDPMI) {
		// First client - read int table
		for (Bitu i=0; i<256; i++) originalIntTable[i] = mem_readd(i*4);
	} else {
		// Another client already running, remove active callbacks from int table
		activeDPMI->RemoveIntCallbacks();
	}
	activeDPMI = new DPMI();		
	return activeDPMI->Entrypoint();
}

Bitu DPMI::Entrypoint(void)
{
	/* This should switch to pmode */
	if (dpmi.client.have) E_Exit("DPMI:Already have a client");
	
	LOG(LOG_MISC,LOG_ERROR)("DPMI: Entrypoint (%d Bit)",(reg_ax & 1) ? 32:16);

	// Create gdt, ldt, idt and other stuff
	Setup();
	
	// Save Realmode Registers
	SaveRegisterState(0);
	
	dpmi.client.have  = true;
	dpmi.client.bit32 = reg_ax & 1;
	
	// Clear XMS Handles
	ClearXMSHandles();
	/* Clear the LDT */
	Bitu i;
	for (i=0;i<LDT_SIZE*8;i++) mem_writeb(dpmi.ldt.base+i,0);
	/* Setup IDT */
	SetDescriptor gate;
	Bitu selIntType = dpmi.client.bit32 ? DESC_386_INT_GATE:DESC_286_INT_GATE;
	for (i=0;i<256;i++) {
		gate.Clear();
		gate.SetSelector(GDT_PROTCODE);
		gate.SetOffset	(i*8);
		gate.SetType	(selIntType);
		gate.saved.seg.p   = 1;
		gate.saved.seg.dpl = DPMI_DPL;
		gate.Save(dpmi.idt.base+i*8);
	}
	
	/* Load GDT and IDT */
	CPU_LIDT(dpmi.idt.limit,dpmi.idt.base);
	CPU_LGDT(dpmi.gdt.limit,dpmi.gdt.base);
	/* Switch to pmode */
	Bitu old_cr0=CPU_GET_CRX(0);
	CPU_SET_CRX(0,old_cr0 | 1);
	/* Setup LDT and rest of descriptors for client startup */
	SetDescriptor desc;
	Bitu first;
	AllocateLDTDescriptor(7,first);
	CPU_LLDT(GDT_LDT);
	// Set code segments according to dpmi client
	Descriptor code;
	if (cpu.gdt.GetDescriptor(GDT_CODE,code)) {
		// DMPI callback code (0xC800)
		code.saved.seg.big = dpmi.client.bit32;
		code.Save(dpmi.gdt.base+(GDT_CODE & ~7));
	} else E_Exit("DPMI: cannot initialize code selector 1.");
	if (cpu.gdt.GetDescriptor(GDT_PROTCODE|DPMI_DPL,code)) {
		// DMPI callback code (XMS)
		code.saved.seg.big = dpmi.client.bit32;
		code.Save(dpmi.gdt.base+(GDT_PROTCODE & ~7));
	} else E_Exit("DPMI: cannot initialize code selector 2.");

	/* Setup Selector for PSP, which will be ES */
	DOS_PSP psp(dos.psp);
	dpmi.client.psp=psp.GetSegment();		
	desc.Clear();
	desc.SetLimit(0xff);
	desc.SetBase (dpmi.client.psp << 4);
	desc.SetType (DESC_DATA_ED_RW_A);
	desc.saved.seg.p   = 1;
	desc.saved.seg.dpl = DPMI_DPL;
	desc.Save(dpmi.ldt.base+(first & ~7));
	CPU_SetSegGeneral(es,first);
	first+=8;
	/* Setup Selector for environment */
	Bitu adr = psp.GetEnvironment()<<4;
	if ((!adr) && psp.GetParent()) {
		DOS_PSP parent = DOS_PSP(psp.GetParent());
		adr = parent.GetEnvironment()<<4;
	} 
	if (!adr) E_Exit("DPMI: Couldnt get environment.");
	
	desc.Clear();
	desc.SetBase (adr);
	desc.SetLimit(0xFF);
	desc.SetType (DESC_DATA_ED_RW_A);
	desc.saved.seg.p   = 1;
	desc.saved.seg.big = 1;
	desc.saved.seg.dpl = DPMI_DPL;
	desc.Save    (dpmi.ldt.base+(first & ~7));
	psp.SetEnvironment(first);
	first+=8;
	/* Setup Selector for DS */
	desc.Clear();
	desc.SetLimit(0xffff);
	desc.SetBase (SegValue(ds) << 4);
	desc.SetType (DESC_DATA_ED_RW_A);
	desc.saved.seg.p   = 1;
	desc.saved.seg.dpl = DPMI_DPL;
	desc.Save(dpmi.ldt.base+(first & ~7));
	CPU_SetSegGeneral(ds,first);
	first+=8;
	/* Get the CS;IP of the stack before changing it */
	Bitu old_cs,old_eip;
	old_eip=mem_readw(SegPhys(ss)+reg_sp);
	old_cs=mem_readw(SegPhys(ss)+reg_sp+2);
	reg_esp+=4;
	/* Setup Selector for SS and clear high word of ESP */
	desc.Clear();
	desc.SetLimit(0xffff);
	desc.SetBase(SegValue(ss) << 4);
	desc.SetType(DESC_DATA_ED_RW_A);
	desc.saved.seg.p   = 1;
	desc.saved.seg.big = dpmi.client.bit32 ? 1 : 0;
	desc.saved.seg.dpl = DPMI_DPL;
	desc.Save(dpmi.ldt.base+(first & ~7));
	CPU_SetSegGeneral(ss,first);
	reg_esp=reg_sp;
	first+=8;	
	/* Setup CS:IP and jmp there */
	desc.Clear();
	desc.SetLimit(0xffff);
	desc.SetBase(old_cs << 4);
	desc.SetType(DESC_CODE_R_NC_A);
	desc.saved.seg.p   = 1;
	desc.saved.seg.big = 0;		//Start up in 16-bit segment
	desc.saved.seg.dpl = DPMI_DPL;
	desc.Save(dpmi.ldt.base+(first & ~7));
	// Init exception handlers
	for (i=0; i<DPMI_EXCEPTION_MAX; i++) {
		dpmi.exceptionSelector[i] = GDT_PROTCODE;
		dpmi.exceptionOffset[i] = DPMI_CB_EXCEPTION_OFFSET;
	};

	// Create Real and ProtMode Stacks
	CreateStackSpace();

#if DPMI_HOOK_HARDWARE_INTS
	// Hook Interrupts in real mode to reflect them to protected mode
	Bitu num;
	for (i=0; i<DPMI_REALVEC_MAX; i++) {
		num = rmIndexToInt[i];
		dpmi.oldRealVec[i]	= HookInterrupt(num, dpmi.defaultHWIntFromProtMode[i]);
		dpmi.realModeVec[i] = RealGetVec(num);
	};
#endif

	dpmi.vIntFlag = 1;
	cpu.cpl = DPMI_DPL;
	/* The final jump to start up the code */
	CPU_JMP(false,first,old_eip);
	return 0;
}

static bool DPMI_Multiplex(void) {
	switch (reg_ax) {	
	case 0x1600:		// Windows check
		reg_al = 0x00;	// No windows
		return true;
	case 0x1686:		/* Get CPU Mode */
		reg_ax = 1;
		return true;
	case 0x1687:{		/* Get Mode switch entry point */
		DPMI_LOG("DPMI: 0x2F 0x1687: Get DPMI entry point.");
		reg_ax=0;		/* supported */
		reg_bx=1;		/* Support 32-bit */
		reg_cl=4;		/* 486 */
		reg_dh=00;		/* dpmi 0.90 */
		reg_dl=90;
		reg_si=0;		/* No need for private data */
		RealPt entry=CALLBACK_RealPointer(callback.entry);
		SegSet16(es,RealSeg(entry));
		reg_di=RealOff(entry);
		return true; };
	
	case 0xED00:/* TNT DOS Extender Detection */
/*				DPMI_LOG("DPMI:INT 2F:TNT Extender detection");
				reg_ax = 0xEDFF;	// Installed
				reg_si = 0x5048;	// "PH"
				reg_di = 0x4152;	// "AR"
				reg_cx = 0x0400;	// Majr/Minor Version
				reg_dx = 0x0001;	// DPMI
				reg_bx = 0x0100;		// DPMI Version
				return true;*/
	case 0xED03:
	case 0xF100: DPMI_LOG("DPMI:INT 2F: Pharlap Detection : %04X.",reg_ax);
	};
	return false;
}

void DPMI_Init(Section* sec) 
{
	Section_prop * section=static_cast<Section_prop *>(sec);
	if (!section->Get_bool("dpmi")) return;

	memset(&callback,0,sizeof(callback));

	/* setup Real mode Callbacks */
	callback.entry=CALLBACK_Allocate();
	CALLBACK_Setup(callback.entry,DPMI_EntryPoint,CB_RETF);
	callback.enterpmode=CALLBACK_Allocate();
	CALLBACK_Setup(callback.enterpmode,DPMI_EnterProtMode,CB_RETF);
	callback.realsavestate=CALLBACK_Allocate();
	CALLBACK_Setup(callback.realsavestate,DPMI_RealSaveState,CB_RETF);
	callback.simint=CALLBACK_Allocate();
	CALLBACK_Setup(callback.simint,DPMI_SimulateInt,CB_IRET);
	callback.simintReturn=CALLBACK_Allocate();
	CALLBACK_Setup(callback.simintReturn,DPMI_SimulateIntReturn,CB_IRET);
	callback.rmIntFrame=CALLBACK_Allocate();
	CALLBACK_Setup(callback.rmIntFrame,DPMI_CallRealIRETFrame,CB_IRET);
	callback.rmIntFrameReturn=CALLBACK_Allocate();
	CALLBACK_Setup(callback.rmIntFrameReturn,DPMI_CallRealIRETFrameReturn,CB_IRET);
	callback.ptorint=CALLBACK_Allocate();
	CALLBACK_Setup(callback.ptorint,DPMI_ptorHandler,CB_IRET);
	callback.ptorintReturn=CALLBACK_Allocate();
	CALLBACK_Setup(callback.ptorintReturn,DPMI_ptorHandlerReturn,CB_IRET);
	callback.int21Return=CALLBACK_Allocate();
	CALLBACK_Setup(callback.int21Return,DPMI_Int21HandlerReturn,CB_IRET);
	callback.rmCallbackReturn=CALLBACK_Allocate();
	CALLBACK_Setup(callback.rmCallbackReturn,DPMI_RealModeCallbackReturn,CB_IRET);
	callback.int21msdos=CALLBACK_Allocate();
	CALLBACK_Setup(callback.int21msdos,DPMI_API_Int21_MSDOS,CB_IRET);
	
	/* Setup multiplex */
	DOS_AddMultiplexHandler(DPMI_Multiplex);
}
	
void DPMI::Reactivate()
{
	/* Load GDT and IDT */
	CPU_LIDT(dpmi.idt.limit,dpmi.idt.base);
	CPU_LGDT(dpmi.gdt.limit,dpmi.gdt.base);
	CPU_LLDT(GDT_LDT);
	cpu.cpl = DPMI_DPL;
	RestoreIntCallbacks();
};

void DPMI::Setup()
{	
	Bitu i;
	Bitu xmssize		= (TOTAL_SIZE|(DPMI_PAGE_SIZE-1))+1;
	Bitu protStackSize	= ((DPMI_PROTMODE_STACK_MAX*DPMI_PROTMODE_STACKSIZE)|(DPMI_PAGE_SIZE-1))+1;
	Bitu sizePages		= ((xmssize+protStackSize) >> 12);

	dpmi.mem_handle = MEM_AllocatePages(sizePages,true);
	if (dpmi.mem_handle==0) {
		LOG_MSG("DPMI:Can't allocate XMS memory, disabling dpmi support.");
		return;
	}

	// Allocate real mode stack space
	rm_ss = DOS_GetMemory(DPMI_REALMODE_STACKSIZE/16);
	rm_sp = DPMI_REALMODE_STACKSIZE;
	/* Allocate the GDT,LDT,IDT Stack space */
	Bitu address = dpmi.mem_handle*DPMI_PAGE_SIZE;;
	// Get Begin of protected mode stack 
	dpmi.protStack = address + xmssize;
	/* Clear the memory */
	PhysPt w;
	for (w=address;w<xmssize;w++) mem_writeb(w,0);
	dpmi.gdt.base=address;
	dpmi.gdt.limit=(GDT_SIZE*8)-1;
	address+=GDT_SIZE*8;
	dpmi.idt.base=address;
	dpmi.idt.limit=(IDT_SIZE*8)-1;
	address+=IDT_SIZE*8;
	address+=4;
	dpmi.ldt.base=address;
	dpmi.ldt.limit=(LDT_SIZE*8)-1;
	address+=LDT_SIZE*8;
	dpmi.ptorint_base=address;
	address+=INT_SIZE*8;
	/* Setup LDT and CODE descriptors in GDT */
	SetDescriptor ldt;
	ldt.Clear();
	ldt.SetBase	(dpmi.ldt.base);
	ldt.SetLimit(dpmi.ldt.limit);
	ldt.SetType	(DESC_LDT);
	ldt.saved.seg.p	  = 1;
	ldt.saved.seg.dpl = DPMI_DPL;
	ldt.Save(dpmi.gdt.base+(GDT_LDT & ~7));
	SetDescriptor code;
	/* Setup Code Descriptor for real mode calls */
	code;
	code.Clear();
	code.SetBase (CB_SEG<<4);
	code.SetLimit(0xFFFF);
	code.SetType (DESC_CODE_R_NC_A);
	code.saved.seg.p   = 1;
	code.saved.seg.big = 1;
	code.saved.seg.dpl = DPMI_DPL;
	code.Save    (dpmi.gdt.base+(GDT_CODE & ~7));
	/* Setup Code Descriptor for protected mode calls */
	code.Clear();
	code.SetBase (dpmi.ptorint_base);
	code.SetLimit(0xFFFF);
	code.SetType (DESC_CODE_R_NC_A);
	code.saved.seg.p   = 1;
	code.saved.seg.big = 1;
	code.saved.seg.dpl = DPMI_DPL;
	code.Save    (dpmi.gdt.base+(GDT_PROTCODE & ~7));
	/* Setup data Descriptor to access first megabyte */
	code.Clear();
	code.SetBase (0);
	code.SetLimit(0xFFFFF);
	code.SetType (DESC_DATA_ED_RW_A);
	code.saved.seg.p   = 1;
	code.saved.seg.big = 1;
	code.saved.seg.dpl = DPMI_DPL;
	code.Save    (dpmi.gdt.base+(GDT_DOSDATA & ~7));
	/* Setup data Descriptor to access Dos Segment 0x40 */
	code.Clear();
	code.SetBase (0x40<<4);
	code.SetLimit(0xFFFF);
	code.SetType (DESC_DATA_ED_RW_A);
	code.saved.seg.p   = 1;
	code.saved.seg.big = 0;
	code.saved.seg.dpl = 0;
	code.Save    (dpmi.gdt.base+(GDT_DOSSEG40 & ~7));
	
#if DPMI_HOOK_HARDWARE_INTS
	// Setup Hardware Interrupt handler
	for (i=0; i<DPMI_REALVEC_MAX; i++) {
		dpmi.defaultHWIntFromProtMode[i]=CALLBACK_Allocate();
		CALLBACK_Setup(dpmi.defaultHWIntFromProtMode[i],DPMI_HWIntDefaultHandler,CB_IRET);
	};
#endif

	// Init Realmode Callbacks
	for (i=0; i<DPMI_REALMODE_CALLBACK_MAX; i++) {
		dpmi.rmCallback[i].id = CALLBACK_Allocate();
		CALLBACK_Setup(dpmi.rmCallback[i].id,DPMI_RealModeCallback,CB_IRET);	
		dpmi.rmCallback[i].inUse = false;
	};

	/* Setup some callbacks used only in pmode */
	callback.apimsdosentry=CALLBACK_Allocate();
	CALLBACK_SetupAt(callback.apimsdosentry,DPMI_API_Entry_MSDOS,CB_RETF,dpmi.ptorint_base+DPMI_CB_APIMSDOSENTRY_OFFSET);
	callback.enterrmode=CALLBACK_Allocate();
	CALLBACK_SetupAt(callback.enterrmode,DPMI_EnterRealMode,CB_RETF,dpmi.ptorint_base+DPMI_CB_ENTERREALMODE_OFFSET);
	callback.protsavestate=CALLBACK_Allocate();
	CALLBACK_SetupAt(callback.protsavestate,DPMI_ProtSaveState,CB_RETF,dpmi.ptorint_base+DPMI_CB_SAVESTATE_OFFSET);
//	callback.exception=CALLBACK_Allocate();
//	CALLBACK_SetupAt(callback.exception,DPMI_Exception,CB_RETF,dpmi.ptorint_base+DPMI_CB_EXCEPTION_OFFSET);
	callback.exceptionret=CALLBACK_Allocate();
	CALLBACK_SetupAt(callback.exceptionret,DPMI_ExceptionReturn,CB_RETF,dpmi.ptorint_base+DPMI_CB_EXCEPTIONRETURN_OFFSET);

	/* Setup table to reflect pmode ints to realmode */
	w=dpmi.ptorint_base;
	for (i=0;i<256;i++) {
		mem_writeb(w,0xFE);						//GRP 4
		mem_writeb(w+1,0x38);					//Extra Callback instruction
		mem_writew(w+2,callback.ptorint);		//The immediate word
		mem_writeb(w+4,0xcf);					//IRET
		w+=8;
	}

	// Set Special 0x31 and 0x21 Handler
	callback.int31=CALLBACK_Allocate();
	CALLBACK_SetupAt(callback.int31,DPMI_Int31Handler,CB_IRET,dpmi.ptorint_base+0x31*8);
	callback.int21=CALLBACK_Allocate();
	CALLBACK_SetupAt(callback.int21,DPMI_Int21Handler,CB_IRET,dpmi.ptorint_base+0x21*8);
	callback.int2f=CALLBACK_Allocate();
	CALLBACK_SetupAt(callback.int2f,DPMI_Int2fHandler,CB_IRET,dpmi.ptorint_base+0x2f*8);
}

// *********************************************************************
// Special Extender capabilities : MS-DOS
// *********************************************************************

Bitu DPMI::GetSegmentFromSelector(Bitu selector)
{
	Bitu base = 0xDEAD;
	SetDescriptor desc;
	if (cpu.gdt.GetDescriptor(selector,desc)) {
		base = desc.GetBase();
		if ((base>0xFFFFF) || (base & 0x0F)) E_Exit("DPMI:MSDOS: Invalid Selector (convert to segment not possible)");
		base >>= 4;
	} else E_Exit("DPMI:MSDOS: Invalid Selector (not found)");
	return base;
};

bool DPMI::GetMsdosSelector(Bitu realseg, Bitu realoff, Bitu &protsel, Bitu &protoff)
{
	if (AllocateLDTDescriptor(1,protsel)) {
		SetDescriptor desc;
		desc.Load		(dpmi.ldt.base+(protsel & ~7));
		desc.SetBase	(realseg<<4);
		desc.SetLimit	(0xFFFF);
		desc.Save		(dpmi.ldt.base+(protsel & ~7));
	} else E_Exit("DPMI:MSDOS: No more selectors.");
	protoff = realoff;
	return true;
};

void DPMI::API_Init_MSDOS(void)
{
	// Enable special Int 21 Handler....
	RealPt func = CALLBACK_RealPointer(callback.int21msdos);
	SetDescriptor gate;
	gate.Load		(dpmi.idt.base+0x21*8);
	gate.SetSelector(GDT_CODE);
	gate.SetOffset	(RealOff(func));
	gate.Save		(dpmi.idt.base+0x21*8);			
};

Bitu DPMI::API_Entry_MSDOS(void) 
{
	LOG(LOG_MISC,LOG_WARN)("DPMI: MSDOS Extension API Entry.");
	switch (reg_ax) {
		case 0x0000:// Get MS-DOS Extension Version
					reg_ax = 0x0000;
					SETFLAGBIT(CF,false);
					break;
		case 0x0100:// Get Selector to Base of LDT
					// Note that the DPMI host has the option of either failing
					// this call, or to return a read-only descriptor (we fail it).
					SETFLAGBIT(CF,true);
					break;
		default:	SETFLAGBIT(CF,true);
					LOG(LOG_MISC,LOG_ERROR)("DPMI:MSDOS-API:Unknown ax on entry point %04X.",reg_ax);
					break;
	}
	return 0;
};

Bitu DPMI::API_Int21_MSDOS(void)
{
	DPMI_LOG("DPMI:MSDOS-API:INT 21 %04X",reg_ax);
	Bitu protsel,protoff,seg,off;
	Bitu sax = reg_ax;
	switch (reg_ah) {
	
		case 0x1a:	/* Set Disk Transfer Area Address */
					dtaAddress = SegPhys(ds) + reg_edx;
					break;
		case 0x25:	{ // Set Protected mode Interrupt Vector
					if (dpmi.pharlap) {
						//LOG(LOG_MISC,LOG_ERROR)
						switch (reg_al) {
							
							case 0x05:	// Set Real mode Int Vector
										RealSetVec(reg_cl,reg_ebx);
										DPMI_CALLBACK_SCF(false);
										break;

							default  :	E_Exit("DPMI:PHARLAP:System call %04X",reg_ax);
						};
						
					} else {
						// ms dos api
						SetDescriptor gate;
						gate.Clear();
						gate.saved.seg.p=1;
						gate.SetSelector(SegValue(ds));
						gate.SetOffset	(reg_edx);
						gate.SetType	(dpmi.client.bit32?DESC_386_INT_GATE:DESC_286_INT_GATE);
						gate.saved.seg.dpl = DPMI_DPL;
						gate.Save(dpmi.idt.base+reg_al*8);
					}
					}; break;
		case 0x35:	{ // Get Protected Mode Interrupt Vector
					SetDescriptor gate;
					gate.Load(dpmi.idt.base+reg_al*8);
					CPU_SetSegGeneral(es,gate.GetSelector());
					reg_ebx = gate.GetOffset();
					DPMI_CALLBACK_SCF(false);
					}; break;

		case 0x2f:	/* Get Disk Transfer Area */
					seg = RealSeg(dos.dta);
					off = RealOff(dos.dta);
					GetMsdosSelector(seg,off,protsel,protoff);
					CPU_SetSegGeneral(es,protsel);
					reg_ebx = protoff;
					break;
	
		case 0x34 : // Get INDOS Flag address
					seg = RealSeg(dos.tables.indosflag);
					off = RealOff(dos.tables.indosflag);
					GetMsdosSelector(seg,off,protsel,protoff);
					CPU_SetSegGeneral(es,protsel);
					reg_bx = protoff;
					break;
		case 0x3c:	{ /* CREATE Create of truncate file */
					char name1[256];
					MEM_StrCopy(SegPhys(ds)+reg_edx,name1,255);
					if (DOS_CreateFile(name1,reg_cx,&reg_ax)) {
						DPMI_CALLBACK_SCF(false);
					} else {
						reg_ax=dos.errorcode;
						DPMI_CALLBACK_SCF(true);
					}
					}; break;
		case 0x3d:	{ /* OPEN Open existing file */
					char name1[256];
					MEM_StrCopy(SegPhys(ds)+reg_edx,name1,255);
					if (DOS_OpenFile(name1,reg_al,&reg_ax)) {
						DPMI_LOG("DOS: Open success: %s",name1);
						DPMI_CALLBACK_SCF(false);
					} else {
						DPMI_LOG("DOS: Open failure: %s",name1);
						reg_ax=dos.errorcode;
						DPMI_CALLBACK_SCF(true);
					}
					};break;
		case 0x3f:	/* READ Read from file or device */
					{ 
						if (reg_ecx>0xFFFF) {
							E_Exit("DPMI:DOS: Read file size > 0xffff");							
						};

						Bit16u toread = reg_ecx;
						dos.echo = true;
						if (DOS_ReadFile(reg_bx,dos_copybuf,&toread)) {
							MEM_BlockWrite(SegPhys(ds)+reg_edx,dos_copybuf,toread);
							reg_eax=toread;
							DPMI_CALLBACK_SCF(false);

						} else {
							reg_ax=dos.errorcode;
							DPMI_CALLBACK_SCF(true);
						}
						dos.echo=false;
						break;
					}
		case 0x40:	{/* WRITE Write to file or device */
					Bit16u towrite = reg_ecx;
					MEM_BlockRead(SegPhys(ds)+reg_edx,dos_copybuf,towrite);
					if (DOS_WriteFile(reg_bx,dos_copybuf,&towrite)) {
						reg_eax=towrite;
	   					DPMI_CALLBACK_SCF(false);
					} else {
						reg_ax=dos.errorcode;
						DPMI_CALLBACK_SCF(true);
					}
					}; break;
		case 0x41:	{ /* UNLINK Delete file */
					char name1[256];
					MEM_StrCopy(SegPhys(ds)+reg_edx,name1,255);
					if (DOS_UnlinkFile(name1)) {
						DPMI_CALLBACK_SCF(false);
					} else {
						reg_ax=dos.errorcode;
						DPMI_CALLBACK_SCF(true);
					}
					}; break;
		case 0x42:	/* LSEEK Set current file position */
					{
						Bit32u pos=(reg_cx<<16) + reg_dx;
						if (DOS_SeekFile(reg_bx,&pos,reg_al)) {
							reg_dx=(Bit16u)(pos >> 16);
							reg_ax=(Bit16u)(pos & 0xFFFF);
							DPMI_CALLBACK_SCF(false);
						} else {
							reg_ax=dos.errorcode;
							DPMI_CALLBACK_SCF(true);
						}
						break;
					}
		case 0x43:	{ /* Get/Set file attributes */
					char name1[256];
					MEM_StrCopy(SegPhys(ds)+reg_edx,name1,255);
					switch (reg_al)
					case 0x00:				/* Get */
					{
						if (DOS_GetFileAttr(name1,&reg_cx)) {
							DPMI_CALLBACK_SCF(false);
						} else {
							DPMI_CALLBACK_SCF(true);
							reg_ax=dos.errorcode;
						}
						break;
					case 0x01:				/* Set */
						DPMI_LOG_ERROR("DOS:Set File Attributes for %s not supported",name1);
						DPMI_CALLBACK_SCF(false);
						break;
					default:
						E_Exit("DOS:0x43:Illegal subfunction %2X",reg_al);
					}
					}; break;

		case 0x4E:	{/* Get first dir entry */
					char name1[256];
					MEM_StrCopy(SegPhys(ds)+reg_edx,name1,255);
					if (DOS_FindFirst(name1,reg_cx)) {
						DPMI_CALLBACK_SCF(false);	
						// Copy result to internal dta
						if (dtaAddress) MEM_BlockCopy(dtaAddress,PhysMake(RealSeg(dos.dta),RealOff(dos.dta)),dpmi.pharlap?43:128);
						reg_ax=0;			/* Undocumented */
					} else {
						reg_ax=dos.errorcode;
						DPMI_CALLBACK_SCF(true);
					};
					}; break;		 
		case 0x4f:	/* FINDNEXT Find next matching file */
					// Copy data to dos dta
					if (dtaAddress) MEM_BlockCopy(PhysMake(RealSeg(dos.dta),RealOff(dos.dta)),dtaAddress,dpmi.pharlap?43:128);
					if (DOS_FindNext()) {
						CALLBACK_SCF(false);
						// Copy result to internal dta
						if (dtaAddress) MEM_BlockCopy(dtaAddress,PhysMake(RealSeg(dos.dta),RealOff(dos.dta)),dpmi.pharlap?43:128);
						reg_ax=0xffff;			/* Undocumented */
					} else {
						reg_ax=dos.errorcode;
						CALLBACK_SCF(true);
					};
					break;		
		case 0x50:	/* Set current PSP */
					if (dpmi.pharlap)	dos.psp = reg_bx; // pharlap uses real mode paragraph address
					else				
					dos.psp = GetSegmentFromSelector(reg_bx);
					DPMI_LOG("DPMI:MSDOS:0x50:Set current psp:%04X",reg_bx);					
					break;	
		case 0x51:	/* Get current PSP */
					if (dpmi.pharlap)	reg_bx = dos.psp; // pharlap uses real mode paragraph address
					else {
						GetMsdosSelector(dos.psp,0x0000,protsel,protoff);
						reg_bx = protsel;
					};
					DPMI_LOG("DPMI:MSDOS:0x51:Get current psp:%04X",reg_bx);					
					break;
		case 0x55 : { // Neuen PSP erstellen
					Bitu segment = GetSegmentFromSelector(reg_dx);
					DOS_ChildPSP(segment,reg_si);
					dos.psp = segment;
					DPMI_LOG("DPMI:MSDOS:0x55:Create new psp:%04X",segment);					
					}; break;
		case 0x5D : // Get Address of dos swappable area
					// FIXME: This is totally faked...
					// FIXME: Add size in bytes (at least pharlap)
					// FIXME: Depending on al, two functions (pharlap)
					GetMsdosSelector(0xDEAD,0xDEAD,protsel,protoff);
					CPU_SetSegGeneral(ds,protsel);
					reg_si = protoff;
					DPMI_LOG("DPMI:MSDOS:0x5D:Get Addres of DOS SwapArea:%04X",reg_si);					
					break;
		case 0x62 :	/* Get Current PSP Address */
					GetMsdosSelector(dos.psp,0x0000,protsel,protoff);
					reg_bx = protsel;
					DPMI_LOG("DPMI:MSDOS:0x62:Get current psp:%04X",reg_bx);	
					break;

		case 0x09:
		case 0x0A:
		case 0x0C:
		case 0x1B:
		case 0x1C:
		case 0x26: // Pharlap != MS-DOS
//		case 0x30: // Pharlap extended information
		case 0x31:  
		case 0x32:  
		case 0x38:
		case 0x3A:
		case 0x3B:
		case 0x47:
		case 0x48: // Pharlap = 4KB mem pages
		case 0x49:
		case 0x4A: // Pharlap = 4KB mem pages
		case 0x4B:
		case 0x52:
		case 0x53:
		case 0x56:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5E:
		case 0x5F:
		case 0x60:
//		case 0x62:
		case 0x65:
		case 0x6C:
					E_Exit("DPMI:MSDOS-API:function %04X not yet supported.",reg_ax);
					break;
		
		// *** PASS THROUGH ***
		case 0x44:	if ((reg_al==0x02) || (reg_al==0x03) || (reg_al==0x04) || (reg_al==0x05) || (reg_al==0x0C) || (reg_al==0x0D)) {
						E_Exit("DPMI:MSDOS-API:function %04X not yet supported.",reg_ax);
					};
		case 0x0E: case 0x19: case 0x2A: case 0x2C: case 0x2D: case 0x30: case 0x36:
		case 0x3E: case 0x4C: case 0x58: case 0x67:
					{
						// reflect to real mode	
						DPMI_Int21Handler();
					};
					break;
		default:	E_Exit("DPMI:MSDOS-API:Missing function %04X",reg_ax);

	};
	return 0;
};


