/*
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

#include "dosbox.h"

#include <string.h>
#include <ctype.h>

#include "cpu.h"
#include "callback.h"
#include "debug.h"
#include "dos_inc.h"
#include "mem.h"
#include "programs.h"
#include "regs.h"
#include "string_utils.h"

const char * RunningProgram="DOSBOX";

#ifdef _MSC_VER
#pragma pack(1)
#endif
struct EXE_Header {
	uint16_t signature;					/* EXE Signature MZ or ZM */
	uint16_t extrabytes;					/* Bytes on the last page */
	uint16_t pages;						/* Pages in file */
	uint16_t relocations;					/* Relocations in file */
	uint16_t headersize;					/* Paragraphs in header */
	uint16_t minmemory;					/* Minimum amount of memory */
	uint16_t maxmemory;					/* Maximum amount of memory */
	uint16_t initSS;
	uint16_t initSP;
	uint16_t checksum;
	uint16_t initIP;
	uint16_t initCS;
	uint16_t reloctable;
	uint16_t overlay;
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif

#define MAGIC1 0x5a4d
#define MAGIC2 0x4d5a
#define MAXENV 32768u
#define ENV_KEEPFREE 83				 /* keep unallocated by environment variables */
#define LOADNGO 0
#define LOAD    1
#define OVERLAY 3


extern void GFX_SetTitle(int32_t cycles,int frameskip,bool paused);
void DOS_UpdatePSPName(void) {
	DOS_MCB mcb(dos.psp()-1);
	static char name[9];
	mcb.GetFileName(name);
	name[8] = 0;
	if (!strlen(name)) strcpy(name,"DOSBOX");
	for(Bitu i = 0;i < 8;i++) { //Don't put garbage in the title bar. Mac OS X doesn't like it
		if (name[i] == 0) break;
		if ( !isprint(*reinterpret_cast<unsigned char*>(&name[i])) ) name[i] = '?';
	}
	RunningProgram = name;
	GFX_SetTitle(-1,-1,false);
}

void DOS_Terminate(uint16_t pspseg,bool tsr,uint8_t exitcode) {

	dos.return_code=exitcode;
	dos.return_mode=(tsr)?(uint8_t)RETURN_TSR:(uint8_t)RETURN_EXIT;
	
	DOS_PSP curpsp(pspseg);
	if (pspseg==curpsp.GetParent()) return;
	/* Free Files owned by process */
	if (!tsr) curpsp.CloseFiles();
	
	/* Get the termination address */
	RealPt old22 = curpsp.GetInt22();
	/* Restore vector 22,23,24 */
	curpsp.RestoreVectors();
	/* Set the parent PSP */
	dos.psp(curpsp.GetParent());
	DOS_PSP parentpsp(curpsp.GetParent());

	/* Restore the SS:SP to the previous one */
	SegSet16(ss,RealSeg(parentpsp.GetStack()));
	reg_sp = RealOff(parentpsp.GetStack());		
	/* Restore registers */
	reg_ax = real_readw(SegValue(ss),reg_sp+ 0);
	reg_bx = real_readw(SegValue(ss),reg_sp+ 2);
	reg_cx = real_readw(SegValue(ss),reg_sp+ 4);
	reg_dx = real_readw(SegValue(ss),reg_sp+ 6);
	reg_si = real_readw(SegValue(ss),reg_sp+ 8);
	reg_di = real_readw(SegValue(ss),reg_sp+10);
	reg_bp = real_readw(SegValue(ss),reg_sp+12);
	SegSet16(ds,real_readw(SegValue(ss),reg_sp+14));
	SegSet16(es,real_readw(SegValue(ss),reg_sp+16));
	reg_sp+=18;
	/* Set the CS:IP stored in int 0x22 back on the stack */
	real_writew(SegValue(ss),reg_sp+0,RealOff(old22));
	real_writew(SegValue(ss),reg_sp+2,RealSeg(old22));
	/* set IOPL=3 (Strike Commander), nested task set,
	   interrupts enabled, test flags cleared */
	real_writew(SegValue(ss),reg_sp+4,0x7202);
	// Free memory owned by process
	if (!tsr) DOS_FreeProcessMemory(pspseg);
	DOS_UpdatePSPName();

	if ((!(CPU_AutoDetermineMode>>CPU_AUTODETERMINE_SHIFT)) || (cpu.pmode)) return;

	CPU_AutoDetermineMode>>=CPU_AUTODETERMINE_SHIFT;
	if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES) {
		CPU_CycleAutoAdjust=false;
		CPU_CycleLeft=0;
		CPU_Cycles=0;
		CPU_CycleMax=CPU_OldCycleMax;
		GFX_SetTitle(CPU_OldCycleMax,-1,false);
	} else {
		GFX_SetTitle(-1,-1,false);
	}
#if (C_DYNAMIC_X86) || (C_DYNREC)
	if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CORE) {
		cpudecoder=&CPU_Core_Normal_Run;
		CPU_CycleLeft=0;
		CPU_Cycles=0;
	}
#endif

	return;
}

static bool MakeEnv(char * name,uint16_t * segment) {
	/* If segment to copy environment is 0 copy the caller's environment */
	DOS_PSP psp(dos.psp());
	PhysPt envread,envwrite;
	uint16_t envsize=1;
	bool parentenv=true;

	if (*segment==0) {
		if (!psp.GetEnvironment()) parentenv=false;				//environment seg=0
		envread=PhysMake(psp.GetEnvironment(),0);
	} else {
		if (!*segment) parentenv=false;						//environment seg=0
		envread=PhysMake(*segment,0);
	}

	if (parentenv) {
		for (envsize=0; ;envsize++) {
			if (envsize>=MAXENV - ENV_KEEPFREE) {
				DOS_SetError(DOSERR_ENVIRONMENT_INVALID);
				return false;
			}
			if (mem_readw(envread+envsize)==0) break;
		}
		envsize += 2;									/* account for trailing \0\0 */
	}
	uint16_t size=long2para(envsize+ENV_KEEPFREE);
	if (!DOS_AllocateMemory(segment,&size)) return false;
	envwrite=PhysMake(*segment,0);
	if (parentenv) {
		MEM_BlockCopy(envwrite,envread,envsize);
//		mem_memcpy(envwrite,envread,envsize);
		envwrite+=envsize;
	} else {
		mem_writeb(envwrite++,0);
	}
	mem_writew(envwrite,1);
	envwrite+=2;
	char namebuf[DOS_PATHLENGTH];
	if (DOS_Canonicalize(name,namebuf)) {
		MEM_BlockWrite(envwrite,namebuf,(Bitu)(safe_strlen(namebuf)+1));
		return true;
	} else return false;
}

bool DOS_NewPSP(uint16_t segment, uint16_t size) {
	DOS_PSP psp(segment);
	psp.MakeNew(size);
	uint16_t parent_psp_seg=psp.GetParent();
	DOS_PSP psp_parent(parent_psp_seg);
	psp.CopyFileTable(&psp_parent,false);
	// copy command line as well (Kings Quest AGI -cga switch)
	psp.SetCommandTail(RealMake(parent_psp_seg,0x80));
	return true;
}

bool DOS_ChildPSP(uint16_t segment, uint16_t size) {
	DOS_PSP psp(segment);
	psp.MakeNew(size);
	uint16_t parent_psp_seg = psp.GetParent();
	DOS_PSP psp_parent(parent_psp_seg);
	psp.CopyFileTable(&psp_parent,true);
	psp.SetCommandTail(RealMake(parent_psp_seg,0x80));
	psp.SetFCB1(RealMake(parent_psp_seg,0x5c));
	psp.SetFCB2(RealMake(parent_psp_seg,0x6c));
	psp.SetEnvironment(psp_parent.GetEnvironment());
	psp.SetStack(psp_parent.GetStack());
	psp.SetSize(size);
	return true;
}

static void SetupPSP(uint16_t pspseg,uint16_t memsize,uint16_t envseg) {
	/* Fix the PSP for psp and environment MCB's */
	DOS_MCB mcb((uint16_t)(pspseg-1));
	mcb.SetPSPSeg(pspseg);
	mcb.SetPt((uint16_t)(envseg-1));
	mcb.SetPSPSeg(pspseg);

	DOS_PSP psp(pspseg);
	psp.MakeNew(memsize);
	psp.SetEnvironment(envseg);

	/* Copy file handles */
	DOS_PSP oldpsp(dos.psp());
	psp.CopyFileTable(&oldpsp,true);

}

static void SetupCMDLine(uint16_t pspseg,DOS_ParamBlock & block) {
	DOS_PSP psp(pspseg);
	// if cmdtail==0 it will inited as empty in SetCommandTail
	psp.SetCommandTail(block.exec.cmdtail);
}

bool DOS_Execute(char * name,PhysPt block_pt,uint8_t flags) {
	EXE_Header head;Bitu i;
	uint16_t fhandle;uint16_t len;uint32_t pos;
	uint16_t pspseg,envseg,loadseg,memsize,readsize;
	PhysPt loadaddress;RealPt relocpt;
	Bitu headersize=0,imagesize=0;
	DOS_ParamBlock block(block_pt);

	block.LoadData();
	//Remove the loadhigh flag for the moment!
	if(flags&0x80) LOG(LOG_EXEC,LOG_ERROR)("using loadhigh flag!!!!!. dropping it");
	flags &= 0x7f;
	if (flags!=LOADNGO && flags!=OVERLAY && flags!=LOAD) {
		DOS_SetError(DOSERR_FORMAT_INVALID);
		return false;
//		E_Exit("DOS:Not supported execute mode %d for file %s",flags,name);
	}
	/* Check for EXE or COM File */
	bool iscom=false;
	if (!DOS_OpenFile(name,OPEN_READ,&fhandle)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
	len=sizeof(EXE_Header);
	if (!DOS_ReadFile(fhandle,(uint8_t *)&head,&len)) {
		DOS_CloseFile(fhandle);
		return false;
	}
	if (len<sizeof(EXE_Header)) {
		if (len==0) {
			/* Prevent executing zero byte files */
			DOS_SetError(DOSERR_ACCESS_DENIED);
			DOS_CloseFile(fhandle);
			return false;
		}
		/* Otherwise must be a .com file */
		iscom=true;
	} else {
		/* Convert the header to correct endian, i hope this works */
		HostPt endian=(HostPt)&head;
		for (i=0;i<sizeof(EXE_Header)/2;i++) {
			*((uint16_t *)endian)=host_readw(endian);
			endian+=2;
		}
		if ((head.signature!=MAGIC1) && (head.signature!=MAGIC2)) iscom=true;
		else {
			if(head.pages & ~0x07ff) /* 1 MB dos maximum address limit. Fixes TC3 IDE (kippesoep) */
				LOG(LOG_EXEC,LOG_NORMAL)("Weird header: head.pages > 1 MB");
			head.pages&=0x07ff;
			headersize = head.headersize*16;
			imagesize = head.pages*512-headersize; 
			if (imagesize+headersize<512) imagesize = 512-headersize;
		}
	}
	uint8_t *loadbuf = new uint8_t[0x10000];
	if (flags!=OVERLAY) {
		/* Create an environment block */
		envseg=block.exec.envseg;
		if (!MakeEnv(name,&envseg)) {
			DOS_CloseFile(fhandle);
			delete [] loadbuf;
			return false;
		}
		/* Get Memory */		
		uint16_t minsize,maxsize;uint16_t maxfree=0xffff;DOS_AllocateMemory(&pspseg,&maxfree);
		if (iscom) {
			minsize=0x1000;maxsize=0xffff;
			if (machine==MCH_PCJR) {
				/* try to load file into memory below 96k */ 
				pos=0;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
				uint16_t dataread=0x1800;
				DOS_ReadFile(fhandle,loadbuf,&dataread);
				if (dataread<0x1800) maxsize=((dataread+0x10)>>4)+0x20;
				if (minsize>maxsize) minsize=maxsize;
			}
		} else {	/* Exe size calculated from header */
			minsize=long2para(imagesize+(head.minmemory<<4)+256);
			if (head.maxmemory!=0) maxsize=long2para(imagesize+(head.maxmemory<<4)+256);
			else maxsize=0xffff;
		}
		if (maxfree<minsize) {
			if (iscom) {
				/* Reduce minimum of needed memory size to filesize */
				pos=0;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
				uint16_t dataread=0xf800;
				DOS_ReadFile(fhandle,loadbuf,&dataread);
				if (dataread<0xf800) minsize=((dataread+0x10)>>4)+0x20;
			}
			if (maxfree<minsize) {
				DOS_CloseFile(fhandle);
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				DOS_FreeMemory(envseg);
				delete [] loadbuf;
				return false;
			}
		}
		if (maxfree<maxsize) memsize=maxfree;
		else memsize=maxsize;
		if (!DOS_AllocateMemory(&pspseg,&memsize)) E_Exit("DOS:Exec error in memory");
		if (iscom && (machine==MCH_PCJR) && (pspseg<0x2000)) {
			maxsize=0xffff;
			/* resize to full extent of memory block */
			DOS_ResizeMemory(pspseg,&maxsize);
			memsize=maxsize;
		}
		loadseg=pspseg+16;
		if (!iscom) {
			/* Check if requested to load program into upper part of allocated memory */
			if ((head.minmemory == 0) && (head.maxmemory == 0))
				loadseg = (uint16_t)(((pspseg+memsize)*0x10-imagesize)/0x10);
		}
	} else loadseg=block.overlay.loadseg;
	/* Load the executable */
	loadaddress=PhysMake(loadseg,0);

	if (iscom) {	/* COM Load 64k - 256 bytes max */
		pos=0;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
		readsize=0xffff-256;
		DOS_ReadFile(fhandle,loadbuf,&readsize);
		MEM_BlockWrite(loadaddress,loadbuf,readsize);
	} else {	/* EXE Load in 32kb blocks and then relocate */
		pos=headersize;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
		while (imagesize>0x7FFF) {
			readsize=0x8000;DOS_ReadFile(fhandle,loadbuf,&readsize);
			MEM_BlockWrite(loadaddress,loadbuf,readsize);
//			if (readsize!=0x8000) LOG(LOG_EXEC,LOG_NORMAL)("Illegal header");
			loadaddress+=0x8000;imagesize-=0x8000;
		}
		if (imagesize>0) {
			readsize=(uint16_t)imagesize;DOS_ReadFile(fhandle,loadbuf,&readsize);
			MEM_BlockWrite(loadaddress,loadbuf,readsize);
//			if (readsize!=imagesize) LOG(LOG_EXEC,LOG_NORMAL)("Illegal header");
		}
		/* Relocate the exe image */
		uint16_t relocate;
		if (flags==OVERLAY) relocate=block.overlay.relocation;
		else relocate=loadseg;
		pos=head.reloctable;DOS_SeekFile(fhandle,&pos,0);
		for (i=0;i<head.relocations;i++) {
			readsize=4;DOS_ReadFile(fhandle,(uint8_t *)&relocpt,&readsize);
			relocpt=host_readd((HostPt)&relocpt);		//Endianize
			PhysPt address=PhysMake(RealSeg(relocpt)+loadseg,RealOff(relocpt));
			mem_writew(address,mem_readw(address)+relocate);
		}
	}
	delete [] loadbuf;
	DOS_CloseFile(fhandle);

	/* Setup a psp */
	if (flags!=OVERLAY) {
		// Create psp after closing exe, to avoid dead file handle of exe in copied psp
		SetupPSP(pspseg,memsize,envseg);
		SetupCMDLine(pspseg,block);
	};
	CALLBACK_SCF(false);		/* Carry flag cleared for caller if successfull */
	if (flags==OVERLAY) {
		/* Changed registers */
		reg_ax=0;
		reg_dx=0;
		return true;			/* Everything done for overlays */
	}
	RealPt csip,sssp;
	if (iscom) {
		csip=RealMake(pspseg,0x100);
		if (memsize<0x1000) {
			LOG(LOG_EXEC,LOG_WARN)("COM format with only %X paragraphs available",memsize);
			sssp=RealMake(pspseg,(memsize<<4)-2);
		} else sssp=RealMake(pspseg,0xfffe);
		mem_writew(Real2Phys(sssp),0);
	} else {
		csip=RealMake(loadseg+head.initCS,head.initIP);
		sssp=RealMake(loadseg+head.initSS,head.initSP);
		if (head.initSP<4) LOG(LOG_EXEC,LOG_ERROR)("stack underflow/wrap at EXEC");
		if ((pspseg+memsize)<(RealSeg(sssp)+(RealOff(sssp)>>4)))
			LOG(LOG_EXEC,LOG_ERROR)("stack outside memory block at EXEC");

		Program::ResetLastWrittenChar('\0'); // triggers newline injection after DOS programs
	}

	if ((flags==LOAD) || (flags==LOADNGO)) {
		/* Get Caller's program CS:IP of the stack and set termination address to that */
		RealSetVec(0x22,RealMake(real_readw(SegValue(ss),reg_sp+2),real_readw(SegValue(ss),reg_sp)));
		reg_sp-=18;
		DOS_PSP callpsp(dos.psp());
		/* Save the SS:SP on the PSP of calling program */
		callpsp.SetStack(RealMakeSeg(ss,reg_sp));
		/* Switch the psp's and set new DTA */
		dos.psp(pspseg);
		DOS_PSP newpsp(dos.psp());
		dos.dta(RealMake(newpsp.GetSegment(),0x80));
		/* save vectors */
		newpsp.SaveVectors();
		/* copy fcbs */
		newpsp.SetFCB1(block.exec.fcb1);
		newpsp.SetFCB2(block.exec.fcb2);
		/* Save the SS:SP on the PSP of new program */
		newpsp.SetStack(RealMakeSeg(ss,reg_sp));

		/* Setup bx, contains a 0xff in bl and bh if the drive in the fcb is not valid */
		DOS_FCB fcb1(RealSeg(block.exec.fcb1),RealOff(block.exec.fcb1));
		DOS_FCB fcb2(RealSeg(block.exec.fcb2),RealOff(block.exec.fcb2));
		uint8_t d1 = fcb1.GetDrive(); //depends on 0 giving the dos.default drive
		if ( (d1>=DOS_DRIVES) || !Drives[d1] ) reg_bl = 0xFF; else reg_bl = 0;
		uint8_t d2 = fcb2.GetDrive();
		if ( (d2>=DOS_DRIVES) || !Drives[d2] ) reg_bh = 0xFF; else reg_bh = 0;

		/* Write filename in new program MCB */
		char stripname[8]= { 0 };Bitu index=0;
		while (char chr=*name++) {
			switch (chr) {
			case ':':case '\\':case '/':index=0;break;
			default:if (index<8) stripname[index++]=(char)toupper(chr);
			}
		}
		index=0;
		while (index<8) {
			if (stripname[index]=='.') break;
			if (!stripname[index]) break;	
			index++;
		}
		memset(&stripname[index],0,8-index);
		DOS_MCB pspmcb(dos.psp()-1);
		pspmcb.SetFileName(stripname);
		DOS_UpdatePSPName();
	}

	if (flags==LOAD) {
		/* First word on the stack is the value ax should contain on startup */
		real_writew(RealSeg(sssp-2),RealOff(sssp-2),reg_bx);
		/* Write initial CS:IP and SS:SP in param block */
		block.exec.initsssp = sssp-2;
		block.exec.initcsip = csip;
		block.SaveData();
		/* Changed registers */
		reg_sp+=18;
		reg_ax=RealOff(csip);
		reg_bx=memsize;
		reg_dx=0;
		return true;
	}

	if (flags==LOADNGO) {
		if ((reg_sp>0xfffe) || (reg_sp<18)) LOG(LOG_EXEC,LOG_ERROR)("stack underflow/wrap at EXEC");
		/* Set the stack for new program */
		SegSet16(ss,RealSeg(sssp));reg_sp=RealOff(sssp);
		/* Add some flags and CS:IP on the stack for the IRET */
		CPU_Push16(RealSeg(csip));
		CPU_Push16(RealOff(csip));
		/* DOS starts programs with a RETF, so critical flags
		 * should not be modified (IOPL in v86 mode);
		 * interrupt flag is set explicitly, test flags cleared */
		reg_flags=(reg_flags&(~FMASK_TEST))|FLAG_IF;
		//Jump to retf so that we only need to store cs:ip on the stack
		reg_ip++;
		/* Setup the rest of the registers */
		reg_ax=reg_bx;
		reg_cx=0xff;
		reg_dx=pspseg;
		reg_si=RealOff(csip);
		reg_di=RealOff(sssp);
		reg_bp=0x91c;	/* DOS internal stack begin relict */
		SegSet16(ds,pspseg);SegSet16(es,pspseg);
#if C_DEBUG
		/* Started from debug.com, then set breakpoint at start */
		DEBUG_CheckExecuteBreakpoint(RealSeg(csip),RealOff(csip));
#endif
		return true;
	}
	return false;
}
