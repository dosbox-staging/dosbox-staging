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

#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "cpu.h"
#include "callback.h"

#pragma pack(1)
struct EXE_Header {
	Bit16u signature;					/* EXE Signature MZ or ZM */
	Bit16u extrabytes;					/* Bytes on the last page */
	Bit16u pages;						/* Pages in file */
	Bit16u relocations;					/* Relocations in file */
	Bit16u headersize;					/* Paragraphs in header */
	Bit16u minmemory;					/* Minimum amount of memory */
	Bit16u maxmemory;					/* Maximum amount of memory */
	Bit16u initSS;
	Bit16u initSP;
	Bit16u checksum;
	Bit16u initIP;
	Bit16u initCS;
	Bit16u reloctable;
	Bit16u overlay;
} GCC_ATTRIBUTE(packed);
#pragma pack()

#define MAGIC1 0x5a4d
#define MAGIC2 0x4d5a
#define MAXENV 32768u
#define ENV_KEEPFREE 83				 /* keep unallocated by environment variables */
	/* The '65' added to nEnvSize does not cover the additional stuff:
	   + 2 bytes: number of strings
	   + 80 bytes: maximum absolute filename
	   + 1 byte: '\0'
	   -- 1999/04/21 ska */
#define LOADNGO 0
#define LOAD    1
#define OVERLAY 3



static void SaveRegisters(void) {
	reg_sp-=20;
	mem_writew(SegPhys(ss)+reg_sp+ 0,reg_ax);
	mem_writew(SegPhys(ss)+reg_sp+ 2,reg_cx);
	mem_writew(SegPhys(ss)+reg_sp+ 4,reg_dx);
	mem_writew(SegPhys(ss)+reg_sp+ 6,reg_bx);
	mem_writew(SegPhys(ss)+reg_sp+ 8,reg_si);
	mem_writew(SegPhys(ss)+reg_sp+10,reg_di);
	mem_writew(SegPhys(ss)+reg_sp+12,reg_bp);
	mem_writew(SegPhys(ss)+reg_sp+14,SegValue(ds));
	mem_writew(SegPhys(ss)+reg_sp+16,SegValue(es));
}

static void RestoreRegisters(void) {
	reg_ax=mem_readw(SegPhys(ss)+reg_sp+ 0);
	reg_cx=mem_readw(SegPhys(ss)+reg_sp+ 2);
	reg_dx=mem_readw(SegPhys(ss)+reg_sp+ 4);
	reg_bx=mem_readw(SegPhys(ss)+reg_sp+ 6);
	reg_si=mem_readw(SegPhys(ss)+reg_sp+ 8);
	reg_di=mem_readw(SegPhys(ss)+reg_sp+10);
	reg_bp=mem_readw(SegPhys(ss)+reg_sp+12);
	SegSet16(ds,mem_readw(SegPhys(ss)+reg_sp+14));
	SegSet16(es,mem_readw(SegPhys(ss)+reg_sp+16));
	reg_sp+=20;
}


bool DOS_Terminate(bool tsr) {

	dos.return_code=reg_al;
	dos.return_mode=RETURN_EXIT;
	
	Bit16u mempsp = dos.psp;
	DOS_PSP curpsp(dos.psp);
	if (dos.psp==curpsp.GetParent()) return true;

	/* Free Files owned by process */
	if (!tsr) curpsp.CloseFiles();	
	/* Get the termination address */
	RealPt old22 = curpsp.GetInt22();
	/* Restore vector 22,23,24 */
	curpsp.RestoreVectors();
	/* Set the parent PSP */
	dos.psp = curpsp.GetParent();
	DOS_PSP parentpsp(curpsp.GetParent());
	/* Restore the DTA of the parent psp */
	dos.dta = parentpsp.GetDTA();
	/* Restore the SS:SP to the previous one */
	SegSet16(ss,RealSeg(parentpsp.GetStack()));
	reg_sp = RealOff(parentpsp.GetStack());		
	/* Restore the old CS:IP from int 22h */
	RestoreRegisters();
	/* Set the CS:IP stored in int 0x22 back on the stack */
	mem_writew(SegPhys(ss)+reg_sp+0,RealOff(old22));
	mem_writew(SegPhys(ss)+reg_sp+2,RealSeg(old22));
	// Free memory owned by process
	if (!tsr) DOS_FreeProcessMemory(mempsp);
	return true;
}



static bool MakeEnv(char * name,Bit16u * segment) {

	/* If segment to copy environment is 0 copy the caller's environment */
	DOS_PSP psp(dos.psp);
	Bit8u * envread,*envwrite;
	Bit16u envsize=1;
	bool parentenv=true;

	if (*segment==0) {
		if (!psp.GetEnvironment()) parentenv=false;				//environment seg=0
		envread=HostMake(psp.GetEnvironment(),0);
	} else {
		if (!*segment) parentenv=false;						//environment seg=0
		envread=HostMake(*segment,0);
	}

	if (parentenv) {
		for (envsize=0; ;envsize++) {
			if (envsize>=MAXENV - ENV_KEEPFREE) {
				DOS_SetError(DOSERR_ENVIRONMENT_INVALID);
				return false;
			}
			if (readw(envread+envsize)==0) break;
		}
		envsize += 2;									/* account for trailing \0\0 */
	}
	Bit16u size=long2para(envsize+ENV_KEEPFREE);
	if (!DOS_AllocateMemory(segment,&size)) return false;
	envwrite=HostMake(*segment,0);
	if (parentenv) {
		bmemcpy(envwrite,envread,envsize);
		envwrite+=envsize;
	} else {
		*envwrite++=0;
	}
	*((Bit16u *) envwrite)=1;
	envwrite+=2;

	return DOS_Canonicalize(name,(char *)envwrite);
};

bool DOS_NewPSP(Bit16u segment, Bit16u size)
{
	DOS_PSP psp(segment);
	psp.MakeNew(size);
	psp.CopyFileTable(&DOS_PSP(psp.GetParent()));
	return true;
};

static void SetupPSP(Bit16u pspseg,Bit16u memsize,Bit16u envseg) {
	
	/* Fix the PSP for psp and environment MCB's */
	MCB * pspmcb=(MCB *)HostMake(pspseg-1,0);
	pspmcb->psp_segment=pspseg;
	MCB * envmcb=(MCB *)HostMake(envseg-1,0);
	envmcb->psp_segment=pspseg;

	DOS_PSP psp(pspseg);
	psp.MakeNew(memsize);
	psp.SetEnvironment(envseg);
	psp.SetFileHandle(STDIN ,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDOUT,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDERR,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDAUX,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDNUL,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDPRN,DOS_FindDevice("CON"));
	/* Save old DTA in psp */
	psp.SetDTA(dos.dta);
	/* Setup the DTA */
	dos.dta=RealMake(pspseg,0x80);
}

static void SetupCMDLine(Bit16u pspseg,ParamBlock * block) 
{
	DOS_PSP psp(pspseg);
	// if cmdtail==0 it will inited as empty in SetCommandTail
	psp.SetCommandTail(block->exec.cmdtail);
}

bool DOS_Execute(char * name,ParamBlock * _block,Bit8u flags) {
	EXE_Header head;Bitu i;
	Bit16u fhandle;Bit16u len;Bit32u pos;
	Bit16u pspseg,envseg,loadseg,memsize,readsize;
	HostPt loadaddress;RealPt relocpt;
	
	Bitu headersize,imagesize;
	if (flags!=LOADNGO && flags!=OVERLAY) {
		E_Exit("DOS:Not supported execute mode %d for file %s",flags,name); 	
	}
	
	// Parameter block may be overwritten on load, so save these values !
	ParamBlock block;
	memcpy(&block,_block,sizeof(ParamBlock));

	/* Check for EXE or COM File */
	bool iscom=false;
	if (!DOS_OpenFile(name,OPEN_READ,&fhandle)) return false;
	len=sizeof(EXE_Header);
	if (!DOS_ReadFile(fhandle,(Bit8u *)&head,&len)) {
		DOS_CloseFile(fhandle);
		return false;
	}
	if (len<sizeof(EXE_Header)) iscom=true;	
	if ((head.signature!=MAGIC1) && (head.signature!=MAGIC2)) iscom=true;
	else {
		headersize=head.headersize*16;
		imagesize=(head.pages-1)*512-headersize+head.extrabytes;
	}
	if (flags!=OVERLAY) {
		/* Create an environment block */
		envseg=block.exec.envseg;
		if (!MakeEnv(name,&envseg)) {
			DOS_CloseFile(fhandle);
			return false;
		}
		/* Get Memory */		
		Bit16u minsize,maxsize;Bit16u maxfree=0xffff;DOS_AllocateMemory(&pspseg,&maxfree);
		if (iscom) {
			minsize=0x1000;maxsize=0xffff;
		} else {	/* Exe size calculated from header */
			minsize=long2para(imagesize+(head.minmemory<<4)+256);
			if (head.maxmemory!=0) maxsize=long2para(imagesize+(head.maxmemory<<4)+256);
			else maxsize=0xffff;
		}
		if (maxfree<minsize) {
			DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
			DOS_FreeMemory(envseg);
			return false;
		}
		if (maxfree<maxsize) memsize=maxfree;
		else memsize=maxsize;
		if (!DOS_AllocateMemory(&pspseg,&memsize)) E_Exit("DOS:Exec error in memory");
		loadseg=pspseg+16;
		/* Setup a psp */
		SetupPSP(pspseg,memsize,envseg);
		SetupCMDLine(pspseg,&block);
	} else loadseg=block.overlay.loadseg,0;
	/* Load the executable */
	loadaddress=HostMake(loadseg,0);
	if (iscom) {	/* COM Load 64k - 256 bytes max */
		pos=0;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
		readsize=0xffff-256;
		DOS_ReadFile(fhandle,loadaddress,&readsize);
	} else {		/* EXE Load in 32kb blocks and then relocate */
		pos=headersize;DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET);	
		while (imagesize>0x7FFF) {
			readsize=0x8000;DOS_ReadFile(fhandle,loadaddress,&readsize);
			if (readsize!=0x8000) E_Exit("Illegal header");
			loadaddress+=0x8000;imagesize-=0x8000;
		}
		if (imagesize>0) {
			readsize=(Bit16u)imagesize;DOS_ReadFile(fhandle,loadaddress,&readsize);
			if (readsize!=imagesize) E_Exit("Illegal header");
		}
		/* Relocate the exe image */
		Bit16u relocate;
		if (flags==OVERLAY) relocate=block.overlay.relocation;
		else relocate=loadseg;
		pos=head.reloctable;DOS_SeekFile(fhandle,&pos,0);
		for (i=0;i<head.relocations;i++) {
			readsize=4;DOS_ReadFile(fhandle,(Bit8u *)&relocpt,&readsize);
			relocpt=readd((Bit8u *)&relocpt);		//Endianize
			PhysPt address=PhysMake(RealSeg(relocpt)+loadseg,RealOff(relocpt));
			mem_writew(address,mem_readw(address)+relocate);
		}
	}
	DOS_CloseFile(fhandle);
	CALLBACK_SCF(false);		/* Carry flag cleared for caller if successfull */
	if (flags==OVERLAY) return true;			/* Everything done for overlays */
	RealPt csip,sssp;
	if (iscom) {
		csip=RealMake(pspseg,0x100);
		sssp=RealMake(pspseg,0xfffe);
		mem_writew(PhysMake(pspseg,0xfffe),0);
	} else {
		csip=RealMake(loadseg+head.initCS,head.initIP);
		sssp=RealMake(loadseg+head.initSS,head.initSP);
	}
	if (flags==LOADNGO) {
		/* Get Caller's program CS:IP of the stack and set termination address to that */
		RealSetVec(0x22,RealMake(mem_readw(SegPhys(ss)+reg_sp+2),mem_readw(SegPhys(ss)+reg_sp)));
		SaveRegisters();
		DOS_PSP callpsp(dos.psp);
		/* Save the SS:SP on the PSP of calling program */
		callpsp.SetStack(RealMakeSeg(ss,reg_sp));
		/* Switch the psp's and set new DTA */
		dos.psp=pspseg;
		DOS_PSP newpsp(dos.psp);
		dos.dta=newpsp.GetDTA();
		/* save vectors */
		newpsp.SaveVectors();
		/* copy fcbs */
		newpsp.SetFCB1(block.exec.fcb1);
		newpsp.SetFCB2(block.exec.fcb2);
		/* Set the stack for new program */
		SegSet16(ss,RealSeg(sssp));reg_sp=RealOff(sssp);
		/* Add some flags and CS:IP on the stack for the IRET */
		reg_sp-=6;
		mem_writew(SegPhys(ss)+reg_sp+0,RealOff(csip));
		mem_writew(SegPhys(ss)+reg_sp+2,RealSeg(csip));
		mem_writew(SegPhys(ss)+reg_sp+4,0x200);
		/* Setup the rest of the registers */
		reg_ax=0;
		reg_cx=reg_dx=reg_bx=reg_si=reg_di=reg_bp=0;
		SegSet16(ds,pspseg);SegSet16(es,pspseg);
		return true;
	}
	return false;
}






