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
#include <stdlib.h>
#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "support.h"

/* 
	Work in progress, making classes for handling certain internal memory structures in dos
	This should make it somewhat easier for porting to other endian machines and make
	dos work a bit easier.
*/

#pragma pack (1)

struct sPSP {
	Bit8u exit[2];				/* CP/M-like exit poimt */
	Bit16u next_seg;			/* Segment of first byte beyond memory allocated or program */
	Bit8u  fill_1;				/* single char fill */

/* CPM Stuff dunno what this is*/
//TODO Add some checks for people using this i think
	Bit8u  far_call;			/* far call opcode */
	RealPt cpm_entry;			/* CPM Service Request address*/
	RealPt int_22;				/* Terminate Address */
	RealPt int_23;				/* Break Address */
	RealPt int_24;				/* Critical Error Address */
	Bit16u psp_parent;			/* Parent PSP Segment */
	Bit8u  files[20];			/* File Table - 0xff is unused */
	Bit16u environment;			/* Segment of evironment table */
	RealPt stack;				/* SS:SP Save point for int 0x21 calls */
	Bit16u max_files;			/* Maximum open files */
	RealPt file_table;			/* Pointer to File Table PSP:0x18 */
	RealPt prev_psp;			/* Pointer to previous PSP */
	RealPt dta;				/* Pointer to current Process DTA */
	Bit8u fill_2[16];			/* Lot's of unused stuff i can't care aboue */
	Bit8u service[3];			/* INT 0x21 Service call int 0x21;retf; */
	Bit8u fill_3[45];			/* This has some blocks with FCB info */
	CommandTail cmdtail;
} GCC_ATTRIBUTE(packed);

union sParamBlock {
	struct {
		Bit16u loadseg;
		Bit16u relocation;
	} overlay;
	struct {
		Bit16u envseg;
		RealPt cmdtail;
		RealPt fcb1;
		RealPt fcb2;
		RealPt initsssp;
		RealPt initcsip;
	} exec;
} GCC_ATTRIBUTE(packed);

struct sFCB {
	Bit8u drive;            //0 is current drive. when opened 0 is replaced by drivenumber
	Bit8u filename[8];      //spacepadded to fit
	Bit8u ext[3];           //spacepadded to fit
	Bit16u current_block;   // set to 0 by open
	Bit16u record_size;     // used by reads Set to 80h by OPEN function
	Bit32u filesize;        //in bytes In this field, the first word is the low-order part of the size
	Bit16u date;
	Bit16u time;
	Bit8u reserved[8];
	Bit8u  current_relative_record_number; //open doesn't set this
	Bit32u rel_record;      //open does not handle this
} GCC_ATTRIBUTE(packed);

#pragma pack ()

#define sGet(s,m) GetIt(((s *)0)->m,(PhysPt)&(((s *)0)->m))
#define sSave(s,m,val) SaveIt(((s *)0)->m,(PhysPt)&(((s *)0)->m),val)


class MemStruct {
public:
	Bit8u GetIt(Bit8u,PhysPt addr) {
		return mem_readb(pt+addr);
	};
	Bit16u GetIt(Bit16u,PhysPt addr) {
		return mem_readw(pt+addr);
	};
	Bit32u GetIt(Bit32u,PhysPt addr) {
		return mem_readd(pt+addr);
	};
	void SaveIt(Bit8u,PhysPt addr,Bit8u val) {
		mem_writeb(pt+addr,val);
	};
	void SaveIt(Bit16u,PhysPt addr,Bit16u val) {
		mem_writew(pt+addr,val);
	};
	void SaveIt(Bit32u,PhysPt addr,Bit32u val) {
		mem_writed(pt+addr,val);
	};
	

private:
PhysPt pt;

};


class DOS_PSP :public MemStruct {
public:
	DOS_PSP(Bit16u segment){NewPt(segment);};
	void NewPt(Bit16u segment);
	void MakeNew(Bit16u mem_size);
	Bit8u GetFileHandle(Bitu index);

private:
	Bit16u seg;
	PhysPt pt;
};

void DOS_PSP::NewPt(Bit16u segment) {
	seg=segment;
	pt=PhysMake(segment,0);
};



void DOS_PSP::MakeNew(Bit16u mem_size) {
	Bitu i;
	/* Clear it first */	
	for (i=0;i<256;i++) mem_writeb(pt+i,0);
	/* Standard blocks,int 20  and int21 retf */
	sGet(sPSP,max_files);
	sSave(sPSP,exit[0],0xcd);
	sSave(sPSP,exit[1],0x20);
	sSave(sPSP,service[0],0xcd);
	sSave(sPSP,service[1],0x21);
	sSave(sPSP,service[2],0xcb);
	/* psp and psp-parent */
	sSave(sPSP,psp_parent,dos.psp);
	sSave(sPSP,prev_psp,RealMake(dos.psp,0));
	/* terminate 22,break 23,crititcal error 24 address stored */
	sSave(sPSP,int_22,RealGetVec(0x22));
	sSave(sPSP,int_23,RealGetVec(0x23));
	sSave(sPSP,int_24,RealGetVec(0x24));
	/* Memory size */
	sSave(sPSP,next_seg,seg+mem_size);
	/* Process DTA */
	sSave(sPSP,dta,RealMake(seg,128));
	/* User Stack pointer */
	//Copy from previous psp
	//	mem_writed(pt+offsetof(sPSP,stack),

	/* Init file pointer and max_files */
	sSave(sPSP,file_table,RealMake(seg,offsetof(sPSP,files[0])));
	sSave(sPSP,max_files,20);
	/* Copy file table from calling process */
	for (i=0;i<20;i++) {
		Bit8u handle=0;
		//		Bitu handle=dos.psp.GetFileHandle(i);
		sSave(sPSP,files[i],handle);
	}
}

Bit8u DOS_PSP::GetFileHandle(Bitu index) {
	if (index>=sGet(sPSP,max_files)) return 0xff;
	PhysPt files=Real2Phys(sGet(sPSP,file_table));
	return mem_readb(files+index);
};

#define FCB_EXTENDED (mem_readb(off)==0xFF ? 7:0)

void DOS_FCB::Set_drive(Bit8u a){
	mem_writeb(off+offsetof(sFCB,drive)+FCB_EXTENDED,a);
}
void DOS_FCB::Set_filename(char * a){
	MEM_BlockWrite(off+offsetof(sFCB,filename)+FCB_EXTENDED,a,8);
}
void DOS_FCB::Set_ext(char * a) {
	MEM_BlockWrite(off+offsetof(sFCB,ext)+FCB_EXTENDED,a,3);
}
void DOS_FCB::Set_current_block(Bit16u a){
	mem_writew(off+offsetof(sFCB,current_block)+FCB_EXTENDED,a);
}
void DOS_FCB::Set_record_size(Bit16u a){
	mem_writew(off+offsetof(sFCB,record_size)+FCB_EXTENDED,a);
}
void DOS_FCB::Set_filesize(Bit32u a){
	mem_writed(off+offsetof(sFCB,filesize)+FCB_EXTENDED,a);
}
void DOS_FCB::Set_date(Bit16u a){
	mem_writew(off+offsetof(sFCB,date)+FCB_EXTENDED,a);
}
void DOS_FCB::Set_time(Bit16u a){
	mem_writew(off+offsetof(sFCB,time)+FCB_EXTENDED,a);
}
Bit8u DOS_FCB::Get_drive(void){
	return mem_readb(off+offsetof(sFCB,drive)+FCB_EXTENDED);
}
void DOS_FCB::Get_filename(char * a){
	MEM_BlockRead(off+offsetof(sFCB,filename)+FCB_EXTENDED,a,8);
}
void DOS_FCB::Get_ext(char * a){
	MEM_BlockRead(off+offsetof(sFCB,ext)+FCB_EXTENDED,a,3);
}
Bit16u DOS_FCB::Get_current_block(void){
	return mem_readw(off+offsetof(sFCB,current_block)+FCB_EXTENDED);
}
Bit16u DOS_FCB::Get_record_size(void){
	return mem_readw(off+offsetof(sFCB,record_size)+FCB_EXTENDED);
}
Bit32u DOS_FCB::Get_filesize(void){
	return mem_readd(off+offsetof(sFCB,filesize)+FCB_EXTENDED);
}
Bit16u DOS_FCB::Get_date(void){
	return mem_readw(off+offsetof(sFCB,date)+FCB_EXTENDED);
}
Bit16u DOS_FCB::Get_time(void){
	return mem_readw(off+offsetof(sFCB,time)+FCB_EXTENDED);
}


void DOS_ParamBlock::InitExec(RealPt cmdtail) {
	mem_writew(off+offsetof(sParamBlock,exec.envseg),0);
	mem_writed(off+offsetof(sParamBlock,exec.fcb1),0);
	mem_writed(off+offsetof(sParamBlock,exec.fcb2),0);
	mem_writed(off+offsetof(sParamBlock,exec.cmdtail),cmdtail);
}

Bit16u DOS_ParamBlock::loadseg(void) {
	return mem_readw(off+offsetof(sParamBlock,overlay.loadseg));
}
Bit16u DOS_ParamBlock::relocation(void){
	return mem_readw(off+offsetof(sParamBlock,overlay.loadseg));
}
Bit16u DOS_ParamBlock::envseg(void){
	return mem_readw(off+offsetof(sParamBlock,exec.envseg));
}
RealPt DOS_ParamBlock::initsssp(void){
	return mem_readd(off+offsetof(sParamBlock,exec.initsssp));
}
RealPt DOS_ParamBlock::initcsip(void){
	return mem_readd(off+offsetof(sParamBlock,exec.initcsip));
}
RealPt DOS_ParamBlock::fcb1(void){
	return mem_readd(off+offsetof(sParamBlock,exec.fcb1));
}
RealPt DOS_ParamBlock::fcb2(void){
	return mem_readd(off+offsetof(sParamBlock,exec.fcb2));
}
RealPt DOS_ParamBlock::cmdtail(void){
	return mem_readd(off+offsetof(sParamBlock,exec.cmdtail));
}

