/*
 *  Copyright (C) 2002-2011  The DOSBox Team
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

#include "dosbox.h"
#if C_DEBUG || C_GDBSERVER

#include <string.h>
#include <list>
#include <ctype.h>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
using namespace std;

#include "control.h"
#include "debug.h"
#include "cross.h" //snprintf
#include "cpu.h"
#include "video.h"
#include "pic.h"
#include "mapper.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "mixer.h"
#include "timer.h"
#include "paging.h"
#include "support.h"
#include "shell.h"
#include "programs.h"
#include "debug_inc.h"
#include "../cpu/lazyflags.h"
#include "keyboard.h"
#include "setup.h"

// Heavy Debugging Vars for logging
#if C_HEAVY_DEBUG || C_GDBSERVER
ofstream 	DEBUG_cpuLogFile;
bool		DEBUG_cpuLog			= false;
int		DEBUG_cpuLogCounter	= 0;
int		DEBUG_cpuLogType		= 1;	// log detail
bool DEBUG_zeroProtect = false;
bool	DEBUG_logHeavy	= false;
#endif

bool    DEBUG_showExtend = true;

// Display the content of the MCB chain starting with the MCB at the specified segment.
void DEBUG_LogMCBChain(Bit16u mcb_segment) {
	DOS_MCB mcb(mcb_segment);
	char filename[9]; // 8 characters plus a terminating NUL
	const char *psp_seg_note;
	PhysPt dataAddr = PhysMake(DEBUG_dataSeg,DEBUG_dataOfs);// location being viewed in the "Data Overview"

	// loop forever, breaking out of the loop once we've processed the last MCB
	while (true) {
		// verify that the type field is valid
		if (mcb.GetType()!=0x4d && mcb.GetType()!=0x5a) {
			LOG(LOG_MISC,LOG_ERROR)("MCB chain broken at %04X:0000!",mcb_segment);
			return;
		}

		mcb.GetFileName(filename);

		// some PSP segment values have special meanings
		switch (mcb.GetPSPSeg()) {
			case MCB_FREE:
				psp_seg_note = "(free)";
				break;
			case MCB_DOS:
				psp_seg_note = "(DOS)";
				break;
			default:
				psp_seg_note = "";
		}

		LOG(LOG_MISC,LOG_ERROR)("   %04X  %12u     %04X %-7s  %s",mcb_segment,mcb.GetSize() << 4,mcb.GetPSPSeg(), psp_seg_note, filename);

		// print a message if dataAddr is within this MCB's memory range
		PhysPt mcbStartAddr = PhysMake(mcb_segment+1,0);
		PhysPt mcbEndAddr = PhysMake(mcb_segment+1+mcb.GetSize(),0);
		if (dataAddr >= mcbStartAddr && dataAddr < mcbEndAddr) {
			LOG(LOG_MISC,LOG_ERROR)("   (data addr %04hX:%04X is %u bytes past this MCB)",DEBUG_dataSeg,DEBUG_dataOfs,dataAddr - mcbStartAddr);
		}

		// if we've just processed the last MCB in the chain, break out of the loop
		if (mcb.GetType()==0x5a) {
			break;
		}
		// else, move to the next MCB in the chain
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}
}

// Display the content of all Memory Control Blocks.
void DEBUG_LogMCBS(void)
{
	LOG(LOG_MISC,LOG_ERROR)("MCB Seg  Size (bytes)  PSP Seg (notes)  Filename");
	LOG(LOG_MISC,LOG_ERROR)("Conventional memory:");
	DEBUG_LogMCBChain(dos.firstMCB);

	LOG(LOG_MISC,LOG_ERROR)("Upper memory:");
	DEBUG_LogMCBChain(dos_infoblock.GetStartOfUMBChain());
}

void DEBUG_LogGDT(void)
{
	char out1[512];
	Descriptor desc;
	Bitu length = cpu.gdt.GetLimit();
	PhysPt address = cpu.gdt.GetBase();
	PhysPt max	   = address + length;
	Bitu i = 0;
	LOG(LOG_MISC,LOG_ERROR)("GDT Base:%08X Limit:%08X",address,length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08X type: %02X parbg",(i<<3),desc.GetBase(),desc.saved.seg.type);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		sprintf(out1,"      l:%08X dpl : %01X  %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		address+=8; i++;
	};
};

void DEBUG_LogLDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu ldtSelector = cpu.gdt.SLDT();
	if (!cpu.gdt.GetDescriptor(ldtSelector,desc)) return;
	Bitu length = desc.GetLimit();
	PhysPt address = desc.GetBase();
	PhysPt max	   = address + length;
	Bitu i = 0;
	LOG(LOG_MISC,LOG_ERROR)("LDT Base:%08X Limit:%08X",address,length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08X type: %02X parbg",(i<<3)|4,desc.GetBase(),desc.saved.seg.type);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		sprintf(out1,"      l:%08X dpl : %01X  %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		address+=8; i++;
	};
};

void DEBUG_LogIDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu address = 0;
	while (address<256*8) {
		if (cpu.idt.GetDescriptor(address,desc)) {
			sprintf(out1,"%04X: sel:%04X off:%02X",address/8,desc.GetSelector(),desc.GetOffset());
			LOG(LOG_MISC,LOG_ERROR)(out1);
		}
		address+=8;
	};
};

void DEBUG_LogPages(char* selname) {
	char out1[512];
	if (paging.enabled) {
		Bitu sel = DEBUG_GetHexValue(selname,selname);
		if ((sel==0x00) && ((*selname==0) || (*selname=='*'))) {
			for (int i=0; i<0xfffff; i++) {
				Bitu table_addr=(paging.base.page<<12)+(i >> 10)*4;
				X86PageEntry table;
				table.load=phys_readd(table_addr);
				if (table.block.p) {
					X86PageEntry entry;
					Bitu entry_addr=(table.block.base<<12)+(i & 0x3ff)*4;
					entry.load=phys_readd(entry_addr);
					if (entry.block.p) {
						sprintf(out1,"page %05Xxxx -> %04Xxxx  flags [uw] %x:%x::%x:%x [d=%x|a=%x]",
							i,entry.block.base,entry.block.us,table.block.us,
							entry.block.wr,table.block.wr,entry.block.d,entry.block.a);
						LOG(LOG_MISC,LOG_ERROR)(out1);
					}
				}
			}
		} else {
			Bitu table_addr=(paging.base.page<<12)+(sel >> 10)*4;
			X86PageEntry table;
			table.load=phys_readd(table_addr);
			if (table.block.p) {
				X86PageEntry entry;
				Bitu entry_addr=(table.block.base<<12)+(sel & 0x3ff)*4;
				entry.load=phys_readd(entry_addr);
				sprintf(out1,"page %05Xxxx -> %04Xxxx  flags [puw] %x:%x::%x:%x::%x:%x",sel,entry.block.base,entry.block.p,table.block.p,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
				LOG(LOG_MISC,LOG_ERROR)(out1);
			} else {
				sprintf(out1,"pagetable %03X not present, flags [puw] %x::%x::%x",(sel >> 10),table.block.p,table.block.us,table.block.wr);
				LOG(LOG_MISC,LOG_ERROR)(out1);
			}
		}
	}
};

void DEBUG_LogCPUInfo(void) {
	char out1[512];
	sprintf(out1,"cr0:%08X cr2:%08X cr3:%08X  cpl=%x",cpu.cr0,paging.cr2,paging.cr3,cpu.cpl);
	LOG(LOG_MISC,LOG_ERROR)(out1);
	sprintf(out1,"eflags:%08X [vm=%x iopl=%x nt=%x]",reg_flags,GETFLAG(VM)>>17,GETFLAG(IOPL)>>12,GETFLAG(NT)>>14);
	LOG(LOG_MISC,LOG_ERROR)(out1);
	sprintf(out1,"GDT base=%08X limit=%08X",cpu.gdt.GetBase(),cpu.gdt.GetLimit());
	LOG(LOG_MISC,LOG_ERROR)(out1);
	sprintf(out1,"IDT base=%08X limit=%08X",cpu.idt.GetBase(),cpu.idt.GetLimit());
	LOG(LOG_MISC,LOG_ERROR)(out1);

	Bitu sel=CPU_STR();
	Descriptor desc;
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"TR selector=%04X, base=%08X limit=%08X*%X",sel,desc.GetBase(),desc.GetLimit(),desc.saved.seg.g?0x4000:1);
		LOG(LOG_MISC,LOG_ERROR)(out1);
	}
	sel=CPU_SLDT();
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"LDT selector=%04X, base=%08X limit=%08X*%X",sel,desc.GetBase(),desc.GetLimit(),desc.saved.seg.g?0x4000:1);
		LOG(LOG_MISC,LOG_ERROR)(out1);
	}
};

_LogGroup loggrp[LOG_MAX]={{"",true},{0,false}};
FILE* debuglog;

void LOG::operator() (char const* format, ...){
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsprintf(buf,format,msg);
	va_end(msg);

	if (d_type>=LOG_MAX) return;
	if ((d_severity!=LOG_ERROR) && (!loggrp[d_type].enabled)) return;
	DEBUG_ShowMsg("%10u: %s:%s\n",DEBUG_cycle_count,loggrp[d_type].front,buf);
}

static void LOG_Destroy(Section*) {
	if(debuglog) fclose(debuglog);
}

static void LOG_Init(Section * sec) {
	Section_prop * sect=static_cast<Section_prop *>(sec);
	const char * blah=sect->Get_string("logfile");
	if(blah && blah[0] &&(debuglog = fopen(blah,"wt+"))){
	}else{
		debuglog=0;
	}
	sect->AddDestroyFunction(&LOG_Destroy);
	char buf[1024];
	for (Bitu i=1;i<LOG_MAX;i++) {
		strcpy(buf,loggrp[i].front);
		buf[strlen(buf)]=0;
		lowcase(buf);
		loggrp[i].enabled=sect->Get_bool(buf);
	}
}


void LOG_StartUp(void) {
	/* Setup logging groups */
	loggrp[LOG_ALL].front="ALL";
	loggrp[LOG_VGA].front="VGA";
	loggrp[LOG_VGAGFX].front="VGAGFX";
	loggrp[LOG_VGAMISC].front="VGAMISC";
	loggrp[LOG_INT10].front="INT10";
	loggrp[LOG_SB].front="SBLASTER";
	loggrp[LOG_DMACONTROL].front="DMA_CONTROL";
	
	loggrp[LOG_FPU].front="FPU";
	loggrp[LOG_CPU].front="CPU";
	loggrp[LOG_PAGING].front="PAGING";

	loggrp[LOG_FCB].front="FCB";
	loggrp[LOG_FILES].front="FILES";
	loggrp[LOG_IOCTL].front="IOCTL";
	loggrp[LOG_EXEC].front="EXEC";
	loggrp[LOG_DOSMISC].front="DOSMISC";

	loggrp[LOG_PIT].front="PIT";
	loggrp[LOG_KEYBOARD].front="KEYBOARD";
	loggrp[LOG_PIC].front="PIC";

	loggrp[LOG_MOUSE].front="MOUSE";
	loggrp[LOG_BIOS].front="BIOS";
	loggrp[LOG_GUI].front="GUI";
	loggrp[LOG_MISC].front="MISC";

	loggrp[LOG_IO].front="IO";
	loggrp[LOG_PCI].front="PCI";
	
	/* Register the log section */
	Section_prop * sect=control->AddSection_prop("log",LOG_Init);
	Prop_string* Pstring = sect->Add_string("logfile",Property::Changeable::Always,"");
	Pstring->Set_help("file where the log messages will be saved to");
	char buf[1024];
	for (Bitu i=1;i<LOG_MAX;i++) {
		strcpy(buf,loggrp[i].front);
		lowcase(buf);
		Prop_bool* Pbool = sect->Add_bool(buf,Property::Changeable::Always,true);
		Pbool->Set_help("Enable/Disable logging of this type.");
	}
//	MSG_Add("LOG_CONFIGFILE_HELP","Logging related options for the debugger.\n");
}

#if C_HEAVY_DEBUG || C_GDBSERVER
void DEBUG_LogInstruction(Bit16u segValue, Bit32u eipValue,  ofstream& out) {
	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	PhysPt start = DEBUG_GetAddress(segValue,eipValue);
	char dline[200];Bitu size;
	size = DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (DEBUG_showExtend && (DEBUG_cpuLogType > 0) ) {
		res = DEBUG_AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
		if (reslen<22) for (Bitu i=0; i<22-reslen; i++) res[reslen+i] = ' '; res[22] = 0;
	};
	Bitu len = strlen(dline);
	if (len<30) for (Bitu i=0; i<30-len; i++) dline[len + i] = ' '; dline[30] = 0;

	// Get register values

	if(DEBUG_cpuLogType == 0) {
		out << setw(4) << SegValue(cs) << ":" << setw(4) << reg_eip << "  " << dline;
	} else if (DEBUG_cpuLogType == 1) {
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res;
	} else if (DEBUG_cpuLogType == 2) {
		char ibytes[200]="";	char tmpc[200];
		for (Bitu i=0; i<size; i++) {
			Bit8u value;
			if (mem_readb_checked(start+i,&value)) sprintf(tmpc,"%s","?? ");
			else sprintf(tmpc,"%02X ",value);
			strcat(ibytes,tmpc);
		}
		len = strlen(ibytes);
		if (len<21) { for (Bitu i=0; i<21-len; i++) ibytes[len + i] =' '; ibytes[21]=0;} //NOTE THE BRACKETS
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res << "  " << ibytes;
	}
   
	out << " EAX:" << setw(8) << reg_eax << " EBX:" << setw(8) << reg_ebx 
	    << " ECX:" << setw(8) << reg_ecx << " EDX:" << setw(8) << reg_edx
	    << " ESI:" << setw(8) << reg_esi << " EDI:" << setw(8) << reg_edi 
	    << " EBP:" << setw(8) << reg_ebp << " ESP:" << setw(8) << reg_esp 
	    << " DS:"  << setw(4) << SegValue(ds)<< " ES:"  << setw(4) << SegValue(es);

	if(DEBUG_cpuLogType == 0) {
		out << " SS:"  << setw(4) << SegValue(ss) << " C"  << (get_CF()>0)  << " Z"   << (get_ZF()>0)  
		    << " S" << (get_SF()>0) << " O"  << (get_OF()>0) << " I"  << GETFLAGBOOL(IF);
	} else {
		out << " FS:"  << setw(4) << SegValue(fs) << " GS:"  << setw(4) << SegValue(gs)
		    << " SS:"  << setw(4) << SegValue(ss)
		    << " CF:"  << (get_CF()>0)  << " ZF:"   << (get_ZF()>0)  << " SF:"  << (get_SF()>0)
		    << " OF:"  << (get_OF()>0)  << " AF:"   << (get_AF()>0)  << " PF:"  << (get_PF()>0)
		    << " IF:"  << GETFLAGBOOL(IF);
	}
	if(DEBUG_cpuLogType == 2) {
		out << " TF:" << GETFLAGBOOL(TF) << " VM:" << GETFLAGBOOL(VM) <<" FLG:" << setw(8) << reg_flags 
		    << " CR0:" << setw(8) << cpu.cr0;	
	}
	out << endl;
};

const Bit32u LOGCPUMAX = 20000;

static Bit32u logCount = 0;

struct TLogInst {
	Bit16u s_cs;
	Bit32u eip;
	Bit32u eax;
	Bit32u ebx;
	Bit32u ecx;
	Bit32u edx;
	Bit32u esi;
	Bit32u edi;
	Bit32u ebp;
	Bit32u esp;
	Bit16u s_ds;
	Bit16u s_es;
	Bit16u s_fs;
	Bit16u s_gs;
	Bit16u s_ss;
	bool c;
	bool z;
	bool s;
	bool o;
	bool a;
	bool p;
	bool i;
	char dline[31];
	char res[23];
};

TLogInst logInst[LOGCPUMAX];

void DEBUG_HeavyLogInstruction(void) {

	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	PhysPt start = DEBUG_GetAddress(SegValue(cs),reg_eip);
	char dline[200];
	DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (DEBUG_showExtend) {
		res = DEBUG_AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
		if (reslen<22) for (Bitu i=0; i<22-reslen; i++) res[reslen+i] = ' '; res[22] = 0;
	};

	Bitu len = strlen(dline);
	if (len < 30) for (Bitu i=0; i < 30-len; i++) dline[len+i] = ' ';
	dline[30] = 0;

	TLogInst & inst = logInst[logCount];
	strcpy(inst.dline,dline);
	inst.s_cs = SegValue(cs);
	inst.eip  = reg_eip;
	strcpy(inst.res,res);
	inst.eax  = reg_eax;
	inst.ebx  = reg_ebx;
	inst.ecx  = reg_ecx;
	inst.edx  = reg_edx;
	inst.esi  = reg_esi;
	inst.edi  = reg_edi;
	inst.ebp  = reg_ebp;
	inst.esp  = reg_esp;
	inst.s_ds = SegValue(ds);
	inst.s_es = SegValue(es);
	inst.s_fs = SegValue(fs);
	inst.s_gs = SegValue(gs);
	inst.s_ss = SegValue(ss);
	inst.c    = get_CF()>0;
	inst.z    = get_ZF()>0;
	inst.s    = get_SF()>0;
	inst.o    = get_OF()>0;
	inst.a    = get_AF()>0;
	inst.p    = get_PF()>0;
	inst.i    = GETFLAGBOOL(IF);

	if (++logCount >= LOGCPUMAX) logCount = 0;
};

void DEBUG_HeavyWriteLogInstruction(void) {
	if (!DEBUG_logHeavy) return;
	DEBUG_logHeavy = false;
	
	DEBUG_ShowMsg("DEBUG: Creating cpu log LOGCPU_INT_CD.TXT\n");

	ofstream out("LOGCPU_INT_CD.TXT");
	if (!out.is_open()) {
		DEBUG_ShowMsg("DEBUG: Failed.\n");	
		return;
	}
	out << hex << noshowbase << setfill('0') << uppercase;
	Bit32u startLog = logCount;
	do {
		// Write Intructions
		TLogInst & inst = logInst[startLog];
		out << setw(4) << inst.s_cs << ":" << setw(8) << inst.eip << "  " 
		    << inst.dline << "  " << inst.res << " EAX:" << setw(8)<< inst.eax
		    << " EBX:" << setw(8) << inst.ebx << " ECX:" << setw(8) << inst.ecx
		    << " EDX:" << setw(8) << inst.edx << " ESI:" << setw(8) << inst.esi
		    << " EDI:" << setw(8) << inst.edi << " EBP:" << setw(8) << inst.ebp
		    << " ESP:" << setw(8) << inst.esp << " DS:"  << setw(4) << inst.s_ds
		    << " ES:"  << setw(4) << inst.s_es<< " FS:"  << setw(4) << inst.s_fs
		    << " GS:"  << setw(4) << inst.s_gs<< " SS:"  << setw(4) << inst.s_ss
		    << " CF:"  << inst.c  << " ZF:"   << inst.z  << " SF:"  << inst.s
		    << " OF:"  << inst.o  << " AF:"   << inst.a  << " PF:"  << inst.p
		    << " IF:"  << inst.i  << endl;

/*		fprintf(f,"%04X:%08X   %s  %s  EAX:%08X EBX:%08X ECX:%08X EDX:%08X ESI:%08X EDI:%08X EBP:%08X ESP:%08X DS:%04X ES:%04X FS:%04X GS:%04X SS:%04X CF:%01X ZF:%01X SF:%01X OF:%01X AF:%01X PF:%01X IF:%01X\n",
			logInst[startLog].s_cs,logInst[startLog].eip,logInst[startLog].dline,logInst[startLog].res,logInst[startLog].eax,logInst[startLog].ebx,logInst[startLog].ecx,logInst[startLog].edx,logInst[startLog].esi,logInst[startLog].edi,logInst[startLog].ebp,logInst[startLog].esp,
		        logInst[startLog].s_ds,logInst[startLog].s_es,logInst[startLog].s_fs,logInst[startLog].s_gs,logInst[startLog].s_ss,
		        logInst[startLog].c,logInst[startLog].z,logInst[startLog].s,logInst[startLog].o,logInst[startLog].a,logInst[startLog].p,logInst[startLog].i);*/
		if (++startLog >= LOGCPUMAX) startLog = 0;
	} while (startLog != logCount);
	
	out.close();
	DEBUG_ShowMsg("DEBUG: Done.\n");	
};

#endif

#endif
