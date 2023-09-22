/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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


#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "dma.h"
#include "pic.h"
#include "paging.h"
#include "setup.h"
#include "../save_state.h"

DmaController *DmaControllers[2]={NULL};
unsigned char dma_extra_page_registers[16]={0}; /* 0x80-0x8F */
bool enable_dma_extra_page_registers = true;
bool dma_page_register_writeonly = false;

#define EMM_PAGEFRAME4K	((0xE000*16)/4096)
Bit32u ems_board_mapping[LINK_START];

static Bit32u dma_wrapping = 0xffff;

bool enable_1st_dma = true;
bool enable_2nd_dma = true;
bool allow_decrement_mode = true;

static void UpdateEMSMapping(void) {
	/* if EMS is not present, this will result in a 1:1 mapping */
	Bitu i;
	for (i=0;i<0x10;i++) {
		ems_board_mapping[EMM_PAGEFRAME4K+i]=paging.firstmb[EMM_PAGEFRAME4K+i];
	}
}

/* read a block from physical memory */
static void DMA_BlockRead(PhysPt spage,PhysPt offset,void * data,Bitu size,Bit8u dma16) {
	Bit8u * write=(Bit8u *) data;
	Bitu highpart_addr_page = spage>>12;
	size <<= dma16;
	offset <<= dma16;
	Bit32u dma_wrap = ((0xffff<<dma16)+dma16) | dma_wrapping;
	for ( ; size ; size--, offset++) {
		offset &= dma_wrap;
		Bitu page = highpart_addr_page+(offset >> 12);
		/* care for EMS pageframe etc. */
		if (page < EMM_PAGEFRAME4K) page = paging.firstmb[page];
		else if (page < EMM_PAGEFRAME4K+0x10) page = ems_board_mapping[page];
		else if (page < LINK_START) page = paging.firstmb[page];
		*write++=phys_readb(page*4096 + (offset & 4095));
	}
}

/* decrement mode. Needed for EMF Internal Damage and other weird demo programs that like to transfer
 * audio data backwards to the sound card.
 *
 * NTS: Don't forget, from 8237 datasheet: The DMA chip transfers a byte (or word if 16-bit) of data,
 *      and THEN increments or decrements the address. So in decrement mode, "address" is still the
 *      first byte before decrementing. */
static void DMA_BlockReadBackwards(PhysPt spage,PhysPt offset,void * data,Bitu size,Bit8u dma16) {
	Bit8u * write=(Bit8u *) data;
	Bitu highpart_addr_page = spage>>12;

	size <<= dma16;
	offset <<= dma16;
	Bit32u dma_wrap = ((0xffff<<dma16)+dma16) | dma_wrapping;

	if (dma16) {
		/* I'm going to assume by how ISA DMA works that you can't just copy bytes backwards,
		 * because things are transferred in 16-bit WORDs. So you would copy each pair of bytes
		 * in normal order, writing the pairs backwards [*1]. I know of no software that would
		 * actually want to transfer 16-bit DMA backwards, so, it's not implemented.
		 *
		 * Like this:
		 *
		 * 0x1234 0x5678 0x9ABC 0xDEF0
		 *
		 * becomes:
		 *
		 * 0xDEF0 0x9ABC 0x5678 0x1234
		 *
		 * it does NOT become:
		 *
		 * 0xF0DE 0xBC9A 0x7856 0x3412
		 *
		 * */
		LOG(LOG_DMACONTROL,LOG_WARN)("16-bit decrementing DMA not implemented");
	}
	else {
		for ( ; size ; size--, offset--) {
			offset &= dma_wrap;
			Bitu page = highpart_addr_page+(offset >> 12);
			/* care for EMS pageframe etc. */
			if (page < EMM_PAGEFRAME4K) page = paging.firstmb[page];
			else if (page < EMM_PAGEFRAME4K+0x10) page = ems_board_mapping[page];
			else if (page < LINK_START) page = paging.firstmb[page];
			*write++=phys_readb(page*4096 + (offset & 4095));
		}
	}
}

/* write a block into physical memory */
static void DMA_BlockWrite(PhysPt spage,PhysPt offset,void * data,Bitu size,Bit8u dma16) {
	Bit8u * read=(Bit8u *) data;
	Bitu highpart_addr_page = spage>>12;
	size <<= dma16;
	offset <<= dma16;
	Bit32u dma_wrap = ((0xffff<<dma16)+dma16) | dma_wrapping;
	for ( ; size ; size--, offset++) {
		if (offset>(dma_wrapping<<dma16)) {
			LOG_MSG("DMA segbound wrapping (write): %x:%x size %x [%x] wrap %x",spage,offset,size,dma16,dma_wrapping);
		}
		offset &= dma_wrap;
		Bitu page = highpart_addr_page+(offset >> 12);
		/* care for EMS pageframe etc. */
		if (page < EMM_PAGEFRAME4K) page = paging.firstmb[page];
		else if (page < EMM_PAGEFRAME4K+0x10) page = ems_board_mapping[page];
		else if (page < LINK_START) page = paging.firstmb[page];
		phys_writeb(page*4096 + (offset & 4095), *read++);
	}
}

DmaChannel * GetDMAChannel(Bit8u chan) {
	if (chan<4) {
		/* channel on first DMA controller */
		if (DmaControllers[0]) return DmaControllers[0]->GetChannel(chan);
	} else if (chan<8) {
		/* channel on second DMA controller */
		if (DmaControllers[1]) return DmaControllers[1]->GetChannel(chan-4);
	}
	return NULL;
}

/* remove the second DMA controller (ports are removed automatically) */
void CloseSecondDMAController(void) {
	if (DmaControllers[1]) {
		delete DmaControllers[1];
		DmaControllers[1]=NULL;
	}
}

/* check availability of second DMA controller, needed for SB16 */
bool SecondDMAControllerAvailable(void) {
	if (DmaControllers[1]) return true;
	else return false;
}

static void DMA_Write_Port(Bitu port,Bitu val,Bitu /*iolen*/) {
	if (port<0x10) {
		/* write to the first DMA controller (channels 0-3) */
		DmaControllers[0]->WriteControllerReg(port,val,1);
	} else if (port>=0xc0 && port <=0xdf) {
		/* write to the second DMA controller (channels 4-7) */
		DmaControllers[1]->WriteControllerReg((port-0xc0) >> 1,val,1);
	} else {
		UpdateEMSMapping();
		dma_extra_page_registers[port&0xF] = val;
		switch (port) {
			/* write DMA page register */
			case 0x81:GetDMAChannel(2)->SetPage((Bit8u)val);break;
			case 0x82:GetDMAChannel(3)->SetPage((Bit8u)val);break;
			case 0x83:GetDMAChannel(1)->SetPage((Bit8u)val);break;
			case 0x87:GetDMAChannel(0)->SetPage((Bit8u)val);break;
			case 0x89:GetDMAChannel(6)->SetPage((Bit8u)val);break;
			case 0x8a:GetDMAChannel(7)->SetPage((Bit8u)val);break;
			case 0x8b:GetDMAChannel(5)->SetPage((Bit8u)val);break;
			case 0x8f:GetDMAChannel(4)->SetPage((Bit8u)val);break;
			default:
				  if (!enable_dma_extra_page_registers)
					  LOG(LOG_DMACONTROL,LOG_NORMAL)("Trying to write undefined DMA page register %x",port);
				  break;
		}
	}
}

static Bitu DMA_Read_Port(Bitu port,Bitu iolen) {
	if (port<0x10) {
		/* read from the first DMA controller (channels 0-3) */
		return DmaControllers[0]->ReadControllerReg(port,iolen);
	} else if (port>=0xc0 && port <=0xdf) {
		/* read from the second DMA controller (channels 4-7) */
		return DmaControllers[1]->ReadControllerReg((port-0xc0) >> 1,iolen);
	} else {
		/* if we're emulating PC/XT DMA controller behavior, then the page registers
		 * are write-only and cannot be read */
		if (dma_page_register_writeonly)
			return ~0;

		switch (port) {
			/* read DMA page register */
			case 0x81:return GetDMAChannel(2)->pagenum;
			case 0x82:return GetDMAChannel(3)->pagenum;
			case 0x83:return GetDMAChannel(1)->pagenum;
			case 0x87:return GetDMAChannel(0)->pagenum;
			case 0x89:return GetDMAChannel(6)->pagenum;
			case 0x8a:return GetDMAChannel(7)->pagenum;
			case 0x8b:return GetDMAChannel(5)->pagenum;
			case 0x8f:return GetDMAChannel(4)->pagenum;
			default:
				  if (enable_dma_extra_page_registers)
					return dma_extra_page_registers[port&0xF];
 
				  LOG(LOG_DMACONTROL,LOG_NORMAL)("Trying to read undefined DMA page register %x",port);
				  break;
		}
	}
	return ~0;
}

void DmaController::WriteControllerReg(Bitu reg,Bitu val,Bitu /*len*/) {
	DmaChannel * chan;
	switch (reg) {
	/* set base address of DMA transfer (1st byte low part, 2nd byte high part) */
	case 0x0:case 0x2:case 0x4:case 0x6:
		UpdateEMSMapping();
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			chan->baseaddr=(chan->baseaddr&0xff00)|val;
			chan->curraddr=(chan->curraddr&0xff00)|val;
		} else {
			chan->baseaddr=(chan->baseaddr&0x00ff)|(val << 8);
			chan->curraddr=(chan->curraddr&0x00ff)|(val << 8);
		}
		break;
	/* set DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:case 0x3:case 0x5:case 0x7:
		UpdateEMSMapping();
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			chan->basecnt=(chan->basecnt&0xff00)|val;
			chan->currcnt=(chan->currcnt&0xff00)|val;
		} else {
			chan->basecnt=(chan->basecnt&0x00ff)|(val << 8);
			chan->currcnt=(chan->currcnt&0x00ff)|(val << 8);
		}
		break;
	case 0x8:		/* Comand reg not used */
		break;
	case 0x9:		/* Request registers, memory to memory */
		//TODO Warning?
		break;
	case 0xa:		/* Mask Register */
		if ((val & 0x4)==0) UpdateEMSMapping();
		chan=GetChannel(val & 3);
		chan->SetMask((val & 0x4)>0);
		break;
	case 0xb:		/* Mode Register */
		UpdateEMSMapping();
		chan=GetChannel(val & 3);
		chan->autoinit=(val & 0x10) > 0;
		chan->increment=(!allow_decrement_mode) || ((val & 0x20) == 0); /* 0=increment 1=decrement */
		//TODO Maybe other bits? Like bits 6-7 to select demand/single/block/cascade mode? */
		break;
	case 0xc:		/* Clear Flip/Flip */
		flipflop=false;
		break;
	case 0xd:		/* Master Clear/Reset */
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(true);
			chan->tcount=false;
		}
		flipflop=false;
		break;
	case 0xe:		/* Clear Mask register */		
		UpdateEMSMapping();
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(false);
		}
		break;
	case 0xf:		/* Multiple Mask register */
		UpdateEMSMapping();
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(val & 1);
			val>>=1;
		}
		break;
	}
}

Bitu DmaController::ReadControllerReg(Bitu reg,Bitu /*len*/) {
	DmaChannel * chan;Bitu ret;
	switch (reg) {
	/* read base address of DMA transfer (1st byte low part, 2nd byte high part) */
	case 0x0:case 0x2:case 0x4:case 0x6:
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			return chan->curraddr & 0xff;
		} else {
			return (chan->curraddr >> 8) & 0xff;
		}
	/* read DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:case 0x3:case 0x5:case 0x7:
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			return chan->currcnt & 0xff;
		} else {
			return (chan->currcnt >> 8) & 0xff;
		}
	case 0x8:		/* Status Register */
		ret=0;
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			if (chan->tcount) ret|=1 << ct;
			chan->tcount=false;
			if (chan->request) ret|=1 << (4+ct);
		}
		return ret;
	case 0xc:		/* Clear Flip/Flip (apparently most motherboards will treat read OR write as reset) */
		flipflop=false;
		break;
	default:
		LOG(LOG_DMACONTROL,LOG_NORMAL)("Trying to read undefined DMA port %x",reg);
		break;
	}
	return 0xffffffff;
}

DmaChannel::DmaChannel(Bit8u num, bool dma16) {
	masked = true;
	callback = NULL;
	if(num == 4) return;
	channum = num;
	DMA16 = dma16 ? 0x1 : 0x0;
	pagenum = 0;
	pagebase = 0;
	baseaddr = 0;
	curraddr = 0;
	basecnt = 0;
	currcnt = 0;
	increment = true;
	autoinit = false;
	tcount = false;
	request = false;
}

Bitu DmaChannel::Read(Bitu want, Bit8u * buffer) {
	Bitu done=0;
	curraddr &= dma_wrapping;
again:
	Bitu left=(currcnt+1);
	if (want<left) {
		if (increment) {
			DMA_BlockRead(pagebase,curraddr,buffer,want,DMA16);
			curraddr+=want;
		}
		else {
			DMA_BlockReadBackwards(pagebase,curraddr,buffer,want,DMA16);
			curraddr-=want;
		}

		currcnt-=want;
		done+=want;
	} else {
		if (increment)
			DMA_BlockRead(pagebase,curraddr,buffer,want,DMA16);
		else
			DMA_BlockReadBackwards(pagebase,curraddr,buffer,want,DMA16);

		buffer+=left << DMA16;
		want-=left;
		done+=left;
		ReachedTC();
		if (autoinit) {
			currcnt=basecnt;
			curraddr=baseaddr;
			if (want) goto again;
			UpdateEMSMapping();
		} else {
			if (increment) curraddr+=left;
			else curraddr-=left;
			currcnt=0xffff;
			masked=true;
			UpdateEMSMapping();
			DoCallBack(DMA_TRANSFEREND);
		}
	}
	return done;
}

Bitu DmaChannel::Write(Bitu want, Bit8u * buffer) {
	Bitu done=0;
	curraddr &= dma_wrapping;

	/* TODO: Implement DMA_BlockWriteBackwards() if you find a DOS program, any program, that
	 *       transfers data backwards into system memory */
	if (!increment) {
		LOG(LOG_DMACONTROL,LOG_WARN)("DMA decrement mode (writing) not implemented");
		return 0;
	}

again:
	Bitu left=(currcnt+1);
	if (want<left) {
		DMA_BlockWrite(pagebase,curraddr,buffer,want,DMA16);
		done+=want;
		curraddr+=want;
		currcnt-=want;
	} else {
		DMA_BlockWrite(pagebase,curraddr,buffer,left,DMA16);
		buffer+=left << DMA16;
		want-=left;
		done+=left;
		ReachedTC();
		if (autoinit) {
			currcnt=basecnt;
			curraddr=baseaddr;
			if (want) goto again;
			UpdateEMSMapping();
		} else {
			curraddr+=left;
			currcnt=0xffff;
			masked=true;
			UpdateEMSMapping();
			DoCallBack(DMA_TRANSFEREND);
		}
	}
	return done;
}

class DMA:public Module_base{
public:
	DMA(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);
		Bitu i;

		enable_2nd_dma = section->Get_bool("enable 2nd dma controller");
		enable_1st_dma = enable_2nd_dma || section->Get_bool("enable 1st dma controller");
		enable_dma_extra_page_registers = section->Get_bool("enable dma extra page registers");
		dma_page_register_writeonly = section->Get_bool("dma page registers write-only");
		allow_decrement_mode = section->Get_bool("allow dma address decrement");

		if (enable_1st_dma) DmaControllers[0] = new DmaController(0);
		else DmaControllers[0] = NULL;
		if (enable_2nd_dma) DmaControllers[1] = new DmaController(1);
		else DmaControllers[1] = NULL;
	
		for (i=0;i<0x10;i++) {
			Bitu mask=IO_MB;
			if (i<8) mask|=IO_MW;
			if (enable_1st_dma) {
				/* install handler for first DMA controller ports */
				DmaControllers[0]->DMA_WriteHandler[i].Install(i,DMA_Write_Port,mask);
				DmaControllers[0]->DMA_ReadHandler[i].Install(i,DMA_Read_Port,mask);
			}
			if (enable_2nd_dma) {
				/* install handler for second DMA controller ports */
				DmaControllers[1]->DMA_WriteHandler[i].Install(0xc0+i*2,DMA_Write_Port,mask);
				DmaControllers[1]->DMA_ReadHandler[i].Install(0xc0+i*2,DMA_Read_Port,mask);
			}
		}

		if (enable_1st_dma) {
			/* install handlers for ports 0x81-0x83 (on the first DMA controller) */
			DmaControllers[0]->DMA_WriteHandler[0x10].Install(0x80,DMA_Write_Port,IO_MB,8);
			DmaControllers[0]->DMA_ReadHandler[0x10].Install(0x80,DMA_Read_Port,IO_MB,8);
		}

		if (enable_2nd_dma) {
			/* install handlers for ports 0x81-0x83 (on the second DMA controller) */
			DmaControllers[1]->DMA_WriteHandler[0x10].Install(0x88,DMA_Write_Port,IO_MB,8);
			DmaControllers[1]->DMA_ReadHandler[0x10].Install(0x88,DMA_Read_Port,IO_MB,8);
		}
	}
	~DMA(){
		if (DmaControllers[0]) {
			delete DmaControllers[0];
			DmaControllers[0]=NULL;
		}
		if (DmaControllers[1]) {
			delete DmaControllers[1];
			DmaControllers[1]=NULL;
		}
	}
};

void DMA_SetWrapping(Bitu wrap) {
	dma_wrapping = wrap;
}

static DMA* test;

void DMA_Destroy(Section* /*sec*/){
	if (test != NULL) {
		delete test;
		test = NULL;
	}
}
void DMA_Init(Section* sec) {
	DMA_SetWrapping(0xffff);
	test = new DMA(sec);
	sec->AddDestroyFunction(&DMA_Destroy);
	Bitu i;
	for (i=0;i<LINK_START;i++) {
		ems_board_mapping[i]=i;
	}
}



//save state support
extern void *GUS_DMA_Callback_Func;
extern void *SB_DSP_DMA_CallBack_Func;
extern void *SB_DSP_ADC_CallBack_Func;
extern void *SB_DSP_E2_DMA_CallBack_Func;
extern void *TandyDAC_DMA_CallBack_Func;


const void *dma_state_callback_table[] = {
	NULL,
	GUS_DMA_Callback_Func,
	SB_DSP_DMA_CallBack_Func,
	SB_DSP_ADC_CallBack_Func,
	SB_DSP_E2_DMA_CallBack_Func,
	TandyDAC_DMA_CallBack_Func
};


Bit8u POD_State_Find_DMA_Callback( Bit32u addr )
{
	Bit8u size;

	size = sizeof(dma_state_callback_table) / sizeof(Bit32u);
	for( int lcv=0; lcv<size; lcv++ ) {
		if( (Bit32u) dma_state_callback_table[lcv] == addr ) return lcv;
	}


	// ERROR! Set debug breakpoint
	return 0xff;
}


Bit32u POD_State_Index_DMA_Callback( Bit8u index )
{
	return (Bit32u) dma_state_callback_table[index];
}


void DmaChannel::SaveState( std::ostream& stream )
{
	Bit8u dma_callback;


	dma_callback = POD_State_Find_DMA_Callback( (Bit32u) (callback) );

	//******************************************
	//******************************************
	//******************************************

	// - pure data
	WRITE_POD( &pagebase, pagebase );
	WRITE_POD( &baseaddr, baseaddr );
	WRITE_POD( &curraddr, curraddr );
	WRITE_POD( &basecnt, basecnt );
	WRITE_POD( &currcnt, currcnt );
	WRITE_POD( &channum, channum );
	WRITE_POD( &pagenum, pagenum );
	WRITE_POD( &DMA16, DMA16 );
	WRITE_POD( &increment, increment );
	WRITE_POD( &autoinit, autoinit );
	WRITE_POD( &trantype, trantype );
	WRITE_POD( &masked, masked );
	WRITE_POD( &tcount, tcount );
	WRITE_POD( &request, request );

	//******************************************
	//******************************************
	//******************************************

	// - reloc ptr (!!!)
	WRITE_POD( &dma_callback, dma_callback );
}


void DmaChannel::LoadState( std::istream& stream )
{
	Bit8u dma_callback;


	dma_callback = POD_State_Find_DMA_Callback( (Bit32u) (callback) );

	//******************************************
	//******************************************
	//******************************************

	// - pure data
	READ_POD( &pagebase, pagebase );
	READ_POD( &baseaddr, baseaddr );
	READ_POD( &curraddr, curraddr );
	READ_POD( &basecnt, basecnt );
	READ_POD( &currcnt, currcnt );
	READ_POD( &channum, channum );
	READ_POD( &pagenum, pagenum );
	READ_POD( &DMA16, DMA16 );
	READ_POD( &increment, increment );
	READ_POD( &autoinit, autoinit );
	READ_POD( &trantype, trantype );
	READ_POD( &masked, masked );
	READ_POD( &tcount, tcount );
	READ_POD( &request, request );

	//********************************
	//********************************
	//********************************

	// - reloc func ptr
	READ_POD( &dma_callback, dma_callback );


	callback = (DMA_CallBack) POD_State_Index_DMA_Callback( dma_callback );
}


void DmaController::SaveState( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &ctrlnum, ctrlnum );
	WRITE_POD( &flipflop, flipflop );

	for( int lcv=0; lcv<4; lcv++ ) {
		DmaChannels[lcv]->SaveState(stream);
	}
}


void DmaController::LoadState( std::istream& stream )
{
	// - pure data
	READ_POD( &ctrlnum, ctrlnum );
	READ_POD( &flipflop, flipflop );

	for( int lcv=0; lcv<4; lcv++ ) {
		DmaChannels[lcv]->LoadState(stream);
	}
}


namespace
{
class SerializeDMA : public SerializeGlobalPOD
{
public:
	SerializeDMA() : SerializeGlobalPOD("DMA")
	{}

private:
	virtual void getBytes(std::ostream& stream)
	{
		SerializeGlobalPOD::getBytes(stream);


		// - pure data
		WRITE_POD( &dma_wrapping, dma_wrapping );


		for( int lcv=0; lcv<2; lcv++ ) {
			// cga, tandy, pcjr = no 2nd controller
			if( !DmaControllers[lcv] ) continue;

			DmaControllers[lcv]->SaveState(stream);
		}


		// - pure data
		WRITE_POD( &ems_board_mapping, ems_board_mapping );
	}

	virtual void setBytes(std::istream& stream)
	{
		SerializeGlobalPOD::setBytes(stream);


		// - pure data
		READ_POD( &dma_wrapping, dma_wrapping );


		for( int lcv=0; lcv<2; lcv++ ) {
			// cga, tandy, pcjr = no 2nd controller
			if( !DmaControllers[lcv] ) continue;
			
			DmaControllers[lcv]->LoadState(stream);
		}


		// - pure data
		READ_POD( &ems_board_mapping, ems_board_mapping );
	}
} dummy;
}



/*
ykhwong svn-daum 2012-02-20


static globals:


// - pure data
static Bit32u dma_wrapping;


// - static class 'new' ptr (constructor)
DmaController *DmaControllers[2];

// - pure data
-	Bit8u ctrlnum;
-	bool flipflop;


// - static class 'new' ptr (constructor)
-	DmaChannel *DmaChannels[4];

	// - pure data
	- Bit32u pagebase;
	- Bit16u baseaddr;
	- Bit32u curraddr;
	- Bit16u basecnt;
	- Bit16u currcnt;
	- Bit8u channum;
	- Bit8u pagenum;
	- Bit8u DMA16;
	- bool increment;
	- bool autoinit;
	- Bit8u trantype;
	- bool masked;
	- bool tcount;
	- bool request;

	// - reloc func ptr
	- DMA_CallBack callback;


// - static func ptr
- IO_ReadHandleObject DMA_ReadHandler[0x11];
- IO_WriteHandleObject DMA_WriteHandler[0x11];


Bit32u ems_board_mapping[LINK_START];
*/
