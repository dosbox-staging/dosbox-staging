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
#if defined (_MSC_VER)
#pragma pack(1)
#endif

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
}
#if defined (_MSC_VER)
;
#pragma pack()
#else
__attribute__ ((packed));
#endif

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


bool DOS_Terminate(bool tsr) {
	PSP * psp=(PSP *)real_off(dos.psp,0);
	if (!tsr) {
		/* Free Files owned by process */
		for (Bit16u i=0;i<psp->max_files;i++) {
			DOS_CloseFile(i);
		}
		DOS_FreeProcessMemory(dos.psp);
	};
	dos.psp=psp->psp_parent;
	PSP * oldpsp=(PSP *)real_off(dos.psp,0);
	/* Restore the DTA */
	dos.dta=psp->dta;
	/* Restore the old CS:IP from int 22h */
	RealPt old22;
	old22=RealGetVec(0x22);
	SetSegment_16(cs,RealSeg(old22));
	reg_ip=RealOff(old22);
	/* Restore the SS:SP to the previous one */
	SetSegment_16(ss,RealSeg(oldpsp->stack));
	reg_sp=RealOff(oldpsp->stack);
	/* Restore interrupt 22,23,24 */
	RealSetVec(0x22,psp->int_22);
	RealSetVec(0x23,psp->int_23);
	RealSetVec(0x24,psp->int_24);

	return true;
}



static bool MakeEnv(char * name,Bit16u * segment) {

	/* If segment to copy environment is 0 copy the caller's environment */
	PSP * psp=(PSP *)real_off(dos.psp,0);
	Bit8u * envread,*envwrite;
	Bit16u envsize=1;
	bool parentenv=true;

	if (*segment==0) {
		if (!psp->environment) parentenv=false;				//environment seg=0
		envread=real_off(psp->environment,0);
	} else {
		if (!*segment) parentenv=false;						//environment seg=0
		envread=real_off(*segment,0);
	}

	//TODO Make a good DOS first psp 
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
	envwrite=real_off(*segment,0);
	if (parentenv) {
		bmemcpy(envwrite,envread,envsize);
		envwrite+=envsize;
	} else {
		*envwrite++=0;
	}
	*((Bit16u *) envwrite)=1;
	envwrite+=2;
	//TODO put the filename here 
	return DOS_Canonicalize(name,envwrite);
};

bool DOS_NewPSP(Bit16u pspseg) {
	PSP * newpsp=(PSP *)real_off(pspseg,0);
	PSP * prevpsp=(PSP *)real_off(dos.psp,0);

	memset((void *)newpsp,0,sizeof(PSP));
	newpsp->exit[0]=0xcd;newpsp->exit[1]=0x20;
	newpsp->service[0]=0xcd;newpsp->service[0]=0x21;newpsp->service[0]=0xcb;

	newpsp->mem_size=prevpsp->mem_size;
	newpsp->environment=0;

	newpsp->int_22=RealGetVec(0x22);
	newpsp->int_23=RealGetVec(0x23);
	newpsp->int_24=RealGetVec(0x24);
	
	newpsp->psp_parent=dos.psp;
	newpsp->prev_psp=0xFFFFFFFF;

	Bit32u i;
	Bit8u * prevfile=Real2Host(prevpsp->file_table);
	for (i=0;i<20;i++) newpsp->files[i]=prevfile[i];

	newpsp->max_files=20;
	newpsp->file_table=RealMake(pspseg,offsetof(PSP,files));
	/* Save the old DTA in this psp */
	newpsp->dta=dos.dta;
	/* Setup the DTA */
	dos.dta=RealMake(pspseg,0x80);
	return true;
};

static void SetupPSP(Bit16u pspseg,Bit16u memsize,Bit16u envseg) {
	
	PSP * psp=(PSP *)real_off(pspseg,0);
	/* Fix the PSP index of this MCB */
	MCB * pspmcb=(MCB *)real_off(pspseg-1,0);
	pspmcb->psp_segment=pspseg;
	MCB * envmcb=(MCB *)real_off(envseg-1,0);
	envmcb->psp_segment=pspseg;

	memset((void *)psp,0,sizeof(PSP));
	Bit32u i;

	psp->exit[0]=0xcd;psp->exit[1]=0x20;
	psp->mem_size=memsize+pspseg;
	psp->environment=envseg;

	psp->int_22=RealGetVec(0x22);
	psp->int_23=RealGetVec(0x23);
	psp->int_24=RealGetVec(0x24);
	
	psp->service[0]=0xcd;psp->service[0]=0x21;psp->service[0]=0xcb;
	
	psp->psp_parent=dos.psp;
	psp->prev_psp=RealMake(dos.psp,0);

	for (i=0;i<20;i++) psp->files[i]=0xff;
	psp->files[STDIN]=DOS_FindDevice("CON");
	psp->files[STDOUT]=DOS_FindDevice("CON");
	psp->files[STDERR]=DOS_FindDevice("CON");
	psp->files[STDAUX]=DOS_FindDevice("CON");
	psp->files[STDNUL]=DOS_FindDevice("CON");
	psp->files[STDPRN]=DOS_FindDevice("CON");

	psp->max_files=20;
	psp->file_table=RealMake(pspseg,offsetof(PSP,files));
	/* Save old DTA in psp */
	psp->dta=dos.dta;

	/* Setup the DTA */
	dos.dta=RealMake(pspseg,0x80);
}

static void SetupCMDLine(Bit16u pspseg,ParamBlock * block) {
	PSP * psp=(PSP *)real_off(pspseg,0);

	if (block->exec.cmdtail) {
		memcpy((void *)&psp->cmdtail,(void *)Real2Host(block->exec.cmdtail),128);
	} else {
		char temp[]="";
		psp->cmdtail.count=strlen(temp);
		strcpy((char *)&psp->cmdtail.buffer,temp);
		psp->cmdtail.buffer[0]=0x0d;

	}
}




static bool COM_Load(char * name,ParamBlock * block,Bit8u flag) {
	Bit16u fhandle;
	Bit16u size;Bit16u readsize;
	Bit16u envseg,comseg;
	Bit32u pos;

	PSP * callpsp=(PSP *)real_off(dos.psp,0);

	if (!DOS_OpenFile(name,OPEN_READ,&fhandle)) return false;
	if (flag!=OVERLAY) {
		/* Allocate a new Environment */
		envseg=block->exec.envseg;
		if (!MakeEnv(name,&envseg)) return false;
		/* Allocate max memory for COM file and PSP */
		size=0xffff;
		DOS_AllocateMemory(&comseg,&size);
		//TODO Errors check for minimun of 64kb in pages
		if (Bit32u(size <<4)<0x1000) { 
			DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
			DOS_FreeMemory(envseg);
			return false;
		}
		DOS_AllocateMemory(&comseg,&size);
	} else {
		comseg=block->overlay.loadseg;
	}
	/* Memory allocated now load the program */
	/* Now copy the File into allocated memory */
	pos=0;
	DOS_SeekFile(fhandle,&pos,0);	
	readsize=0xffff-256;
	if (flag==OVERLAY) {
		DOS_ReadFile(fhandle,real_host(comseg,0),&readsize);
	} else {
		DOS_ReadFile(fhandle,real_host(comseg,256),&readsize);
	}
	DOS_CloseFile(fhandle);
	if (flag==OVERLAY) /* Everything what should be done for Overlays */
		return true;		
	SetupPSP(comseg,size,envseg);
	SetupCMDLine(comseg,block);
	/* Setup termination Address */
	RealSetVec(0x22,RealMake(Segs[cs].value,reg_ip));
	/* Everything setup somewhat setup CS:IP and SS:SP */
	/* First save the SS:SP of program that called execute */
	callpsp->stack=RealMake(Segs[ss].value,reg_sp);
	/* Clear out first Stack entry to point to int 20h at psp:0 */
	real_writew(comseg,0xfffe,0);
	dos.psp=comseg;
	switch (flag) {
	case LOADNGO:
		SetSegment_16(cs,comseg);
		SetSegment_16(ss,comseg);
		SetSegment_16(ds,comseg);
		SetSegment_16(es,comseg);
		flags.intf=true;
		reg_ip=0x100;
		reg_sp=0xFFFE;
		reg_ax=0;	
		reg_bx=reg_cx=reg_dx=reg_si=reg_di=reg_bp=0;
		return true;
	case LOAD:
		block->exec.initsssp=RealMake(comseg,0xfffe);
		block->exec.initcsip=RealMake(comseg,0x100);
		return true;
	}
	return false;

}


static bool EXE_Load(char * name,ParamBlock * block,Bit8u flag) {

	EXE_Header header;
	Bit16u fhandle;Bit32u i;
	Bit16u size,minsize,maxsize,freesize;Bit16u readsize;
	Bit16u envseg,pspseg,exeseg;
	Bit32u imagesize,headersize;

	PSP * callpsp=(PSP *)real_off(dos.psp,0);

	if (!DOS_OpenFile(name,OPEN_READ,&fhandle)) return false;
	if (flag!=OVERLAY) {
		/* Allocate a new Environment */
		envseg=block->exec.envseg;
		if (!MakeEnv(name,&envseg)) return false;
	};

	/* First Read the EXE Header */
	readsize=sizeof(EXE_Header);
	DOS_ReadFile(fhandle,(Bit8u*)&header,&readsize);
	/* Calculate the size of the image to load */
	headersize=header.headersize*16;
	imagesize=header.pages*512-headersize;
	if (flag!=OVERLAY) { 
		minsize=long2para(imagesize+(header.minmemory<<4)+256);
		if (header.maxmemory!=0) maxsize=long2para(imagesize+(header.maxmemory<<4)+256);
		else maxsize=0xffff;
		freesize=0xffff;
		/* Check for enough free memory */
		DOS_AllocateMemory(&exeseg,&freesize);
		if (minsize>freesize) {
			DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
			DOS_FreeMemory(envseg);
			return false;
		}
		if (maxsize>freesize) {
			size=freesize;		
		} else size=maxsize;
		if ((header.minmemory|header.maxmemory)==0) {
			size=freesize;
			E_Exit("Special case exe header max and min=0");
		}
		if (!DOS_AllocateMemory(&pspseg,&size)) {
			DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
			DOS_FreeMemory(envseg);
			return false;
		}
		SetupPSP(pspseg,size,envseg);
		SetupCMDLine(pspseg,block);
		exeseg=pspseg+16;
	} else {
		/* For OVERLAY */
		exeseg=block->overlay.loadseg;
	}
	/* Load the image in 32k blocks */
	DOS_SeekFile(fhandle,&headersize,0);
	Bit8u * imageoff=real_off(exeseg,0);
//TODO File size checking and remove size
	// Remove psp size
//	imagesize=256;
	// Maybe remove final page and add last bytes on page
	if (header.extrabytes) {
		imagesize-=512;
		imagesize+=header.extrabytes;
	};
	while (imagesize>0x7FFF) {
		readsize=0x8000;
		DOS_ReadFile(fhandle,imageoff,&readsize);
		if (readsize!=0x8000) {
			E_Exit("Illegal header");
		}
		imageoff+=0x8000;
		imagesize-=0x8000;
	}
	if (imagesize>0) {
		readsize=(Bit16u) imagesize;
		DOS_ReadFile(fhandle,imageoff,&readsize);
	}
	headersize=header.reloctable;
	DOS_SeekFile(fhandle,&headersize,0);
	RealPt reloc;
	for (i=0;i<header.relocations;i++) {
	 	readsize=4;
		DOS_ReadFile(fhandle,(Bit8u *)&reloc,&readsize);
		PhysPt address=Real2Phys(RealMake(RealSeg(reloc)+exeseg,RealOff(reloc)));
		Bit16u change=mem_readw(address);
		if (flag==OVERLAY) {
			change+=block->overlay.relocation;
		} else {
			change+=exeseg;
		};
		mem_writew(address,change);

	};
	DOS_CloseFile(fhandle);
	if (flag==OVERLAY) return true;

	/* Setup termination Address */
	RealSetVec(0x22,RealMake(Segs[cs].value,reg_ip));
	/* Start up the actual EXE if we need to */
	//TODO check for load and return
	callpsp->stack=RealMake(Segs[ss].value,reg_sp);
	dos.psp=pspseg;
	SetSegment_16(cs,exeseg+header.initCS);
	SetSegment_16(ss,exeseg+header.initSS);
	SetSegment_16(ds,pspseg);
	SetSegment_16(es,pspseg);
	reg_ip=header.initIP;
	reg_sp=header.initSP;
	reg_ax=0;	
	reg_bx=reg_cx=reg_dx=reg_si=reg_di=reg_bp=0;
	flags.intf=true;
	return true;
};


bool DOS_Execute(char * name,ParamBlock * block,Bit8u flags) {

	EXE_Header head;
	Bit16u fhandle;
	Bit16u size;
	bool iscom=false;
	if (!DOS_OpenFile(name,OPEN_READ,&fhandle)) return false;
	
	size=sizeof(EXE_Header);
	if (!DOS_ReadFile(fhandle,(Bit8u *)&head,&size)) {
		DOS_CloseFile(fhandle);
		return false;
	}
	if (!DOS_CloseFile(fhandle)) return false;
	if (size<sizeof(EXE_Header)) iscom=true;	
	if ((head.signature!=MAGIC1) && (head.signature!=MAGIC2)) iscom=true;

	if (iscom) {
		return COM_Load(name,block,flags);

	} else {
		return EXE_Load(name,block,flags);
	}
	
}





