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

/* 
	Work in progress, making classes for handling certain internal memory structures in dos
	This should make it somewhat easier for porting to other endian machines and make
	dos work a bit easier.
*/


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
	
};



class DOS_PSP {
public:
	DOS_PSP(Bit16u segment);
	
	void MakeNew(Bit16u env,Bit16u memsize);


	Bit16u base_seg;

private:
	PhysPt off;
};

DOS_PSP::DOS_PSP(Bit16u segment) {
	base_seg=segment;
	off=Real2Phys(RealMake(segment,0));
};

void DOS_PSP::MakeNew(Bit16u env,Bit16u next_para) {
	Bitu i;
	for (i=0;i<256;i++) mem_writeb(off+i,0);
/* Standard blocks */
	mem_writeb(off+offsetof(sPSP,exit[0]),0xcd);
	mem_writeb(off+offsetof(sPSP,exit[1]),0x20);

	mem_writeb(off+offsetof(sPSP,service[0]),0xcd);
	mem_writeb(off+offsetof(sPSP,service[1]),0x21);
	mem_writeb(off+offsetof(sPSP,service[2]),0xcb);

	mem_writew(off+offsetof(sPSP,next_seg),next_para);


//	mem_writew(off+offsetof(sPSP,psp_parent),dos.psp->base_seg);

	/* Setup initial file table */
	mem_writed(off+offsetof(sPSP,int_22),RealGetVec(0x22));
	mem_writed(off+offsetof(sPSP,int_23),RealGetVec(0x23));
	mem_writed(off+offsetof(sPSP,int_24),RealGetVec(0x24));


#if 0

	newpsp->mem_size=prevpsp->mem_size;
	newpsp->environment=0;

	newpsp->int_22.full=real_getvec(0x22);
	newpsp->int_23.full=real_getvec(0x23);
	newpsp->int_24.full=real_getvec(0x24);
	
	newpsp->psp_parent=dos.psp;
	newpsp->prev_psp.full=0xFFFFFFFF;

	Bit32u i;
	Bit8u * prevfile=real_off(prevpsp->file_table.seg,prevpsp->file_table.off);
	for (i=0;i<20;i++) newpsp->files[i]=prevfile[i];

	newpsp->max_files=20;
	newpsp->file_table.seg=pspseg;
	newpsp->file_table.off=offsetof(PSP,files);
	/* Save the old DTA in this psp */
	newpsp->dta.seg=dos.dta.seg;
	newpsp->dta.off=dos.dta.off;
	/* Setup the DTA */
	dos.dta.seg=pspseg;
	dos.dta.off=0x80;
	return;
#endif


}


void DOS_FCB::Set_drive(Bit8u a){
	mem_writeb(off+offsetof(FCB,drive),a);
}
void DOS_FCB::Set_filename(char * a){
	MEM_BlockWrite(off+offsetof(FCB,filename),a,8);
}
void DOS_FCB::Set_ext(char * a) {
	MEM_BlockWrite(off+offsetof(FCB,ext),a,3);
}
void DOS_FCB::Set_current_block(Bit16u a){
	mem_writew(off+offsetof(FCB,current_block),a);
}
void DOS_FCB::Set_record_size(Bit16u a){
	mem_writew(off+offsetof(FCB,record_size),a);
}
void DOS_FCB::Set_filesize(Bit32u a){
	mem_writed(off+offsetof(FCB,filesize),a);
}
void DOS_FCB::Set_date(Bit16u a){
	mem_writew(off+offsetof(FCB,date),a);
}
void DOS_FCB::Set_time(Bit16u a){
	mem_writew(off+offsetof(FCB,time),a);
}
Bit8u DOS_FCB::Get_drive(void){
	return mem_readb(off+offsetof(FCB,drive));
}
void DOS_FCB::Get_filename(char * a){
	MEM_BlockRead(off+offsetof(FCB,filename),a,8);
}
void DOS_FCB::Get_ext(char * a){
	MEM_BlockRead(off+offsetof(FCB,ext),a,3);
}
Bit16u DOS_FCB::Get_current_block(void){
	return mem_readw(off+offsetof(FCB,current_block));
}
Bit16u DOS_FCB::Get_record_size(void){
	return mem_readw(off+offsetof(FCB,record_size));
}
Bit32u DOS_FCB::Get_filesize(void){
	return mem_readd(off+offsetof(FCB,filesize));
}
Bit16u DOS_FCB::Get_date(void){
	return mem_readw(off+offsetof(FCB,date));
}
Bit16u DOS_FCB::Get_time(void){
	return mem_readw(off+offsetof(FCB,time));
}
