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

/* $Id: dos_classes.cpp,v 1.32 2003-10-09 13:47:06 finsterr Exp $ */

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


void DOS_ParamBlock::Clear(void) {
	memset(&exec,0,sizeof(exec));
	memset(&overlay,0,sizeof(overlay));
}

void DOS_ParamBlock::LoadData(void) {
	exec.envseg=sGet(sExec,envseg);
	exec.cmdtail=sGet(sExec,cmdtail);
	exec.fcb1=sGet(sExec,fcb1);
	exec.fcb2=sGet(sExec,fcb2);
	exec.initsssp=sGet(sExec,initsssp);
	exec.initcsip=sGet(sExec,initcsip);
	overlay.loadseg=sGet(sOverlay,loadseg);
	overlay.relocation=sGet(sOverlay,relocation);
}

void DOS_ParamBlock::SaveData(void) {
	sSave(sExec,envseg,exec.envseg);
	sSave(sExec,cmdtail,exec.cmdtail);
	sSave(sExec,fcb1,exec.fcb1);
	sSave(sExec,fcb2,exec.fcb2);
	sSave(sExec,initsssp,exec.initsssp);
	sSave(sExec,initcsip,exec.initcsip);
}


void DOS_InfoBlock::SetLocation(Bit16u segment)
{
	seg = segment;
	pt=PhysMake(seg,0);
/* Clear the initual Block */
	for(Bitu i=0;i<sizeof(sDIB);i++) mem_writeb(pt+i,0xff);
}

void DOS_InfoBlock::SetFirstMCB(Bit16u _firstmcb)
{
	sSave(sDIB,firstMCB,_firstmcb);
}

void DOS_InfoBlock::SetfirstFileTable(RealPt _first_table){
	sSave(sDIB,firstFileTable,_first_table);
}

void DOS_InfoBlock::SetBuffers(Bit16u x,Bit16u y) {
	sSave(sDIB,buffers_x,x);
	sSave(sDIB,buffers_y,y);

}

RealPt DOS_InfoBlock::GetPointer(void)
{
	return RealMake(seg,offsetof(sDIB,firstDPB));
}


/* program Segment prefix */

Bit16u DOS_PSP::rootpsp = 0;

void DOS_PSP::MakeNew(Bit16u mem_size) 
{
	/* get previous */
	DOS_PSP prevpsp(dos.psp);
	/* Clear it first */
	Bitu i;
	for (i=0;i<sizeof(sPSP);i++) mem_writeb(pt+i,0);
	// Set size
	sSave(sPSP,next_seg,seg+mem_size);
	/* far call opcode */
	sSave(sPSP,far_call,0xea);
	// far call to interrupt 0x21 - faked for bill & ted 
	// lets hope nobody really uses this address
	sSave(sPSP,cpm_entry,RealMake(0xDEAD,0xFFFF));
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
	/* Process DTA */
	sSave(sPSP,dta,RealMake(seg,128));
	/* FCBs are filled with 0 */
	// ....
	/* Init file pointer and max_files */
	sSave(sPSP,file_table,RealMake(seg,offsetof(sPSP,files[0])));
	sSave(sPSP,max_files,20);
	for (i=0;i<20;i++) SetFileHandle(i,0xff);

	/* User Stack pointer */
//	if (prevpsp.GetSegment()!=0) sSave(sPSP,stack,prevpsp.GetStack());

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

Bit16u DOS_PSP::FindEntryByHandle(Bit8u handle)
{
	PhysPt files=Real2Phys(sGet(sPSP,file_table));
	for (Bit16u i=0;i<sGet(sPSP,max_files);i++) {
		if (mem_readb(files+i)==handle) return i;
	}	
	return 0xFF;
};

void DOS_PSP::CopyFileTable(DOS_PSP* srcpsp,bool createchildpsp)
{
	/* Copy file table from calling process */
	for (Bit16u i=0;i<20;i++) {
		Bit8u handle = srcpsp->GetFileHandle(i);
		if(createchildpsp)
		{	//copy obeying not inherit flag.(but dont duplicate them)
			bool allowCopy = (handle==0) || ((handle>0) && (FindEntryByHandle(handle)==0xff));
			if((handle<DOS_FILES) && Files[handle] && !(Files[handle]->flags & DOS_NOT_INHERIT) && allowCopy)
			{   
				Files[handle]->AddRef();
				SetFileHandle(i,handle);
			}
			else
			{
				SetFileHandle(i,0xff);
			}
		}
		else
		{	//normal copy so don't mind the inheritance
			SetFileHandle(i,handle);
		}
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
		MEM_BlockCopy(pt+offsetof(sPSP,cmdtail),Real2Phys(src),128);
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

bool DOS_PSP::SetNumFiles(Bit16u fileNum)
{
	if (fileNum>20) {
		// Allocate needed paragraphs
		fileNum+=2;	// Add a few more files for safety
		Bit16u para = (fileNum/16)+((fileNum%16)>0);
		RealPt data	= RealMake(DOS_GetMemory(para),0);
		sSave(sPSP,file_table,data);
		sSave(sPSP,max_files,fileNum);
		Bit16u i;
		for (i=0; i<20; i++)		SetFileHandle(i,sGet(sPSP,files[i]));
		for (i=20; i<fileNum; i++)	SetFileHandle(i,0xFF);
	} else {
		sSave(sPSP,max_files,fileNum);
	};
	return true;
};


void DOS_DTA::SetupSearch(Bit8u _sdrive,Bit8u _sattr,char * pattern) {
	sSave(sDTA,sdrive,_sdrive);
	sSave(sDTA,sattr,_sattr);
	/* Fill with spaces */
	Bitu i;
	for (i=0;i<11;i++) mem_writeb(pt+offsetof(sDTA,sname)+i,' ');
	char * find_ext;
	find_ext=strchr(pattern,'.');
	if (find_ext) {
		Bitu size=find_ext-pattern;if (size>8) size=8;
		MEM_BlockWrite(pt+offsetof(sDTA,sname),pattern,size);
		find_ext++;
		MEM_BlockWrite(pt+offsetof(sDTA,sext),find_ext,(strlen(find_ext)>3) ? 3 : strlen(find_ext));
	} else {
		MEM_BlockWrite(pt+offsetof(sDTA,sname),pattern,(strlen(pattern) > 8) ? 8 : strlen(pattern));
	}
}

void DOS_DTA::SetResult(const char * _name,Bit32u _size,Bit16u _date,Bit16u _time,Bit8u _attr) {
	MEM_BlockWrite(pt+offsetof(sDTA,name),(void *)_name,DOS_NAMELENGTH_ASCII);
	sSave(sDTA,size,_size);
	sSave(sDTA,date,_date);
	sSave(sDTA,time,_time);
	sSave(sDTA,attr,_attr);
}


void DOS_DTA::GetResult(char * _name,Bit32u & _size,Bit16u & _date,Bit16u & _time,Bit8u & _attr) {
	MEM_BlockRead(pt+offsetof(sDTA,name),_name,DOS_NAMELENGTH_ASCII);
	_size=sGet(sDTA,size);
	_date=sGet(sDTA,date);
	_time=sGet(sDTA,time);
	_attr=sGet(sDTA,attr);
}

Bit8u DOS_DTA::GetSearchDrive(void) {
	return sGet(sDTA,sdrive);
}

void DOS_DTA::GetSearchParams(Bit8u & attr,char * pattern) {
	attr=sGet(sDTA,sattr);
	char temp[11];
	MEM_BlockRead(pt+offsetof(sDTA,sname),temp,11);
	memcpy(pattern,temp,8);
	pattern[8]='.';
	memcpy(&pattern[9],&temp[8],3);
	pattern[12]=0;

}

DOS_FCB::DOS_FCB(Bit16u seg,Bit16u off) { 
	SetPt(seg,off); 
	real_pt=pt;
	if (sGet(sFCB,drive)==0xff) {
		pt+=7;
		extended=true;
	} else extended=false;
}

bool DOS_FCB::Extended(void) {
	return extended;
}

void DOS_FCB::Create(bool _extended) {
	Bitu fill;
	if (_extended) fill=36+7;
	else fill=36;
	Bitu i;
	for (i=0;i<fill;i++) mem_writeb(real_pt+i,0);
	pt=real_pt;
	if (_extended) {
		mem_writeb(real_pt,0xff);
		pt+=7;
		extended=true;
	} else extended=false;
}

void DOS_FCB::SetName(Bit8u _drive,char * _fname,char * _ext) {
	sSave(sFCB,drive,_drive);
	MEM_BlockWrite(pt+offsetof(sFCB,filename),_fname,8);
	MEM_BlockWrite(pt+offsetof(sFCB,ext),_ext,3);
}

void DOS_FCB::SetSizeDateTime(Bit32u _size,Bit16u _date,Bit16u _time) {
	sSave(sFCB,filesize,_size);
	sSave(sFCB,date,_date);
	sSave(sFCB,time,_time);
}

void DOS_FCB::GetSizeDateTime(Bit32u & _size,Bit16u & _date,Bit16u & _time) {
	_size=sGet(sFCB,filesize);
	_date=sGet(sFCB,date);
	_time=sGet(sFCB,time);
}

void DOS_FCB::GetRecord(Bit16u & _cur_block,Bit8u & _cur_rec) {
	_cur_block=sGet(sFCB,cur_block);
	_cur_rec=sGet(sFCB,cur_rec);

}

void DOS_FCB::SetRecord(Bit16u _cur_block,Bit8u _cur_rec) {
	sSave(sFCB,cur_block,_cur_block);
	sSave(sFCB,cur_rec,_cur_rec);
}

void DOS_FCB::GetSeqData(Bit8u & _fhandle,Bit16u & _rec_size) {
	_fhandle=sGet(sFCB,file_handle);
	_rec_size=sGet(sFCB,rec_size);
}


void DOS_FCB::GetRandom(Bit32u & _random) {
	_random=sGet(sFCB,rndm);
}

void DOS_FCB::SetRandom(Bit32u _random) {
	sSave(sFCB,rndm,_random);
}

void DOS_FCB::FileOpen(Bit8u _fhandle) {
	sSave(sFCB,drive,GetDrive()+1);
	sSave(sFCB,file_handle,_fhandle);
	sSave(sFCB,cur_block,0);
	sSave(sFCB,rec_size,128);
	sSave(sFCB,rndm,0);
	Bit8u temp=RealHandle(_fhandle);
	sSave(sFCB,filesize,Files[temp]->size);
	sSave(sFCB,time,Files[temp]->time);
	sSave(sFCB,date,Files[temp]->date);
}

void DOS_FCB::FileClose(Bit8u & _fhandle) {
	_fhandle=sGet(sFCB,file_handle);
	sSave(sFCB,file_handle,0xff);
}

Bit8u DOS_FCB::GetDrive(void) {
	Bit8u drive=sGet(sFCB,drive);
	if (!drive) return dos.current_drive;
	else return drive-1;
}

void DOS_FCB::GetName(char * fillname) {
	fillname[0]=GetDrive()+'A';
	fillname[1]=':';
	MEM_BlockRead(pt+offsetof(sFCB,filename),&fillname[2],8);
	fillname[10]='.';
	MEM_BlockRead(pt+offsetof(sFCB,ext),&fillname[11],3);
	fillname[14]=0;
}

