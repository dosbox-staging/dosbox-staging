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
void DOS_FCB::Set_current_record(Bit8u a){
	mem_writeb(off+offsetof(sFCB,current_relative_record_number)+FCB_EXTENDED,a);
}
void DOS_FCB::Set_random_record(Bit32u a){
	mem_writed(off+offsetof(sFCB,rel_record)+FCB_EXTENDED,a);
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
Bit8u DOS_FCB::Get_current_record(void){
	return mem_readb(off+offsetof(sFCB,current_relative_record_number)+FCB_EXTENDED);
}
Bit32u DOS_FCB::Get_random_record(void){
	return mem_readd(off+offsetof(sFCB,rel_record)+FCB_EXTENDED);
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

// * Dos Info Block (list of lists) *

void DOS_InfoBlock::SetLocation(Bit16u segment)
{
	seg = segment;
	dib = (SDosInfoBlock*)HostMake(segment,0);
	Bit16u size = sizeof(SDosInfoBlock);
	memset(dib,0,sizeof(SDosInfoBlock));
};

void DOS_InfoBlock::SetFirstMCB(RealPt pt)
{
	dib->firstMCB = pt;
};

void DOS_InfoBlock::GetDIBPointer(Bit16u& segment, Bit16u& offset)
{
	segment = seg;
	offset	= offsetof(SDosInfoBlock,firstDPB);
};


/* program Segment prefix */

Bit16u DOS_PSP::rootpsp = 0;

void DOS_PSP::NewPt(Bit16u segment) 
{
	seg	= segment;
	pt	= PhysMake(segment,0);
	// debug
	psp = (sPSP*)Phys2Host(pt);
};

void DOS_PSP::MakeNew(Bit16u mem_size) 
{
	/* get previous */
	DOS_PSP prevpsp(dos.psp);
	/* Clear it first */	
	for (Bitu i=0;i<sizeof(sPSP);i++) mem_writeb(pt+i,0);
	// Set size
//	SaveIt(((sPSP*)Phys2Host(pt))->next_seg,0,mem_size);
	sSave(sPSP,next_seg,seg+mem_size);
	/* far call opcode */
	sSave(sPSP,far_call,0xea);
//	sSave(sPSP,cmp_entry
	/* Standard blocks,int 20  and int21 retf */
	sSave(sPSP,exit[0],0xcd);
	sSave(sPSP,exit[1],0x20);
	sSave(sPSP,service[0],0xcd);
	sSave(sPSP,service[1],0x21);
	sSave(sPSP,service[2],0xcb);
	/* psp and psp-parent */
	sSave(sPSP,psp_parent,dos.psp);
	sSave(sPSP,prev_psp,RealMake(dos.psp,0));
	/* terminate 22,break 23,crititcal error 24 address stored */
	SaveVectors();
	/* Memory size */
	sSave(sPSP,next_seg,seg+mem_size);
	/* Process DTA */
	sSave(sPSP,dta,RealMake(seg,128));
	/* FCBs are filled with 0 */
	// ....
	/* Init file pointer and max_files */
	sSave(sPSP,file_table,RealMake(seg,offsetof(sPSP,files[0])));
	sSave(sPSP,max_files,20);
	for (i=0;i<20;i++) SetFileHandle(i,0xff);

	/* User Stack pointer */
	if (prevpsp.GetSegment()!=0) sSave(sPSP,stack,prevpsp.GetStack());

	if (rootpsp==0) rootpsp = seg;
}

Bit8u DOS_PSP::GetFileHandle(Bit16u index) 
{
	if (index>=sGet(sPSP,max_files)) return 0xff;
	PhysPt files=Real2Phys(sGet(sPSP,file_table));
	return mem_readb(files+index);
};

void DOS_PSP::SetFileHandle(Bit16u index, Bit8u handle) 
{
	if (index<sGet(sPSP,max_files)) {
		PhysPt files=Real2Phys(sGet(sPSP,file_table));
		mem_writeb(files+index,handle);
	}
};

Bit16u DOS_PSP::FindFreeFileEntry(void)
{
	PhysPt files=Real2Phys(sGet(sPSP,file_table));
	for (Bit16u i=0;i<sGet(sPSP,max_files);i++) {
		if (mem_readb(files+i)==0xff) return i;
	}	
	return 0xff;
};

void DOS_PSP::CopyFileTable(DOS_PSP* srcpsp)
{
	/* Copy file table from calling process */
	for (Bit16u i=0;i<20;i++) {
		Bit8u handle = srcpsp->GetFileHandle(i);
		SetFileHandle(i,handle);
	}
};

void DOS_PSP::CloseFiles(void)
{
	for (Bit16u i=0;i<sGet(sPSP,max_files);i++) {
		DOS_CloseFile(i);
	}
}

void DOS_PSP::SaveVectors(void)
{
	/* Save interrupt 22,23,24 */
	sSave(sPSP,int_22,RealGetVec(0x22));
	sSave(sPSP,int_23,RealGetVec(0x23));
	sSave(sPSP,int_24,RealGetVec(0x24));
};

void DOS_PSP::RestoreVectors(void)
{
	/* Restore interrupt 22,23,24 */
	RealSetVec(0x22,sGet(sPSP,int_22));
	RealSetVec(0x23,sGet(sPSP,int_23));
	RealSetVec(0x24,sGet(sPSP,int_24));
};

void DOS_PSP::SetCommandTail(RealPt src)
{
	if (src) {	// valid source
		memcpy((void*)(Phys2Host(pt)+offsetof(sPSP,cmdtail)),(void*)Real2Host(src),128);
	} else {	// empty
		sSave(sPSP,cmdtail.count,0x00);
		mem_writeb(pt+offsetof(sPSP,cmdtail.buffer[0]),0x0d);
	};
};

void DOS_PSP::SetFCB1(RealPt src)
{
	if (src) MEM_BlockCopy(PhysMake(seg,offsetof(sPSP,fcb1)),Real2Phys(src),16);
};

void DOS_PSP::SetFCB2(RealPt src)
{
	if (src) MEM_BlockCopy(PhysMake(seg,offsetof(sPSP,fcb2)),Real2Phys(src),16);
};
