/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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
#include <math.h>
#include <sys/types.h>
#include <dirent.h>

#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "mapper.h"
#include "mem.h"

/* 
	Thanks to vdmsound for nice simple way to implement this
*/

#define logerror

#ifdef _MSC_VER
  /* Disable recurring warnings */
# pragma warning ( disable : 4018 )
# pragma warning ( disable : 4244 )
#endif

struct __MALLOCPTR {
	void* m_ptr;

	__MALLOCPTR(void) : m_ptr(NULL) { }
	__MALLOCPTR(void* src) : m_ptr(src) { }
	void* operator=(void* rhs) { return (m_ptr = rhs); }
	operator int*() const { return (int*)m_ptr; }
	operator int**() const { return (int**)m_ptr; }
	operator char*() const { return (char*)m_ptr; }
};

namespace OPL2 {
	#define HAS_YM3812 1
	#include "fmopl.c"
	void TimerOver(Bitu val){
		YM3812TimerOver(val>>8,val & 0xff);
	}
	void TimerHandler(int channel,double interval_Sec) {
		if (interval_Sec==0.0) return;
		PIC_AddEvent(TimerOver,(Bitu)(1000000.0*interval_Sec),channel);		
	}
}
#undef OSD_CPU_H
#undef TL_TAB_LEN
namespace THEOPL3 {
	#define HAS_YMF262 1
	#include "ymf262.c"
	void TimerOver(Bitu val){
		YMF262TimerOver(val>>8,val & 0xff);
	}
	void TimerHandler(int channel,double interval_Sec) {
		if (interval_Sec==0.0) return;
		PIC_AddEvent(TimerOver,(Bitu)(1000000.0*interval_Sec),channel);		
	}
}

#define OPL2_INTERNAL_FREQ    3600000   // The OPL2 operates at 3.6MHz
#define OPL3_INTERNAL_FREQ    14400000  // The OPL3 operates at 14.4MHz

#define RAW_SIZE 1024
static struct {
	bool active;
	OPL_Mode mode;
	MIXER_Channel * chan;
	Bit32u last_used;
	Bit16s mixbuf[2][128];
	struct {
		FILE * handle;
		Bit32u start;
		Bit32u last;
		Bit8u index;
		Bit8u buffer[RAW_SIZE+8];
		Bit8u regs[2][256];
		Bit32u used;
		Bit32u done;
		Bit8u cmd[2];
	} raw;
} opl;

static void OPL_CallBack(Bit8u *stream, Bit32u len) {
	/* Check for size to update and check for 1 ms updates to the opl registers */
	/* Calculate teh machine ms we are at now */

	/* update 1 ms of data */
	Bitu i;
	switch(opl.mode) {
	case OPL_opl2:
		OPL2::YM3812UpdateOne(0,(OPL2::INT16 *)stream,len);
		break;
	case OPL_opl3:
		THEOPL3::YMF262UpdateOne(0,(OPL2::INT16 *)stream,len);
		break;
	case OPL_dualopl2:
		OPL2::YM3812UpdateOne(0,(OPL2::INT16 *)opl.mixbuf[0],len);
		OPL2::YM3812UpdateOne(1,(OPL2::INT16 *)opl.mixbuf[1],len);
		for (i=0;i<len;i++) {
			((Bit16u *)stream)[i*2+0]=opl.mixbuf[0][i];
			((Bit16u *)stream)[i*2+1]=opl.mixbuf[1][i];
		}
		break;
	}
	if ((PIC_Ticks-opl.last_used)>5000) {
		MIXER_Enable(opl.chan,false);
		opl.active=false;
	}
}

static Bitu OPL_Read(Bitu port,Bitu iolen) {
	Bitu addr=port & 3;
	switch (opl.mode) {
	case OPL_opl2:
		return OPL2::YM3812Read(0,addr);
	case OPL_dualopl2:
		return OPL2::YM3812Read(addr>>1,addr);
	case OPL_opl3:
		return THEOPL3::YMF262Read(0,addr);
	}
	return 0xff;
}

static void OPL_RawAdd(Bitu index,Bitu val);
void OPL_Write(Bitu port,Bitu val,Bitu iolen) {
	opl.last_used=PIC_Ticks;
	if (!opl.active) {
		opl.active=true;
		MIXER_Enable(opl.chan,true);
	}
	port&=3;
	if (port&1) {
		Bitu index=port>>1;
		opl.raw.regs[index][opl.raw.cmd[index]]=val;
		if (opl.raw.handle) OPL_RawAdd(index,val);
	} else opl.raw.cmd[port>>1]=val;
	if (!port) adlib_commandreg=val;
	switch (opl.mode) {
	case OPL_opl2:
		OPL2::YM3812Write(0,port,val);
		break;
	case OPL_opl3:
		THEOPL3::YMF262Write(0,port,val);
		break;
	case OPL_dualopl2:
		OPL2::YM3812Write(port>>1,port,val);
		break;
	}
}

static Bit8u dro_header[]={
	'D','B','R','A',		/* Bit32u ID */
	'W','O','P','L',		/* Bit32u ID */
	0x0,0x0,0x0,0x0,		/* Bit32u total milliseconds */
	0x0,0x0,0x0,0x0,		/* Bit32u total data */
	0x0,					/* Type 0=opl2,1=opl3,2=dual-opl2 */
};
/* Commands 
	0x00 Bit8u, millisecond delay+1
	0x01 Bit16u, millisecond delay+1
	0x02 none, Use the low index/data pair
	0x03 none, Use the high index/data pair
	0xxx Bit8u, send command and data to current index/data pair
*/ 

static void OPL_RawEmptyBuffer(void) {
	fwrite(opl.raw.buffer,1,opl.raw.used,opl.raw.handle);
	opl.raw.done+=opl.raw.used;
	opl.raw.used=0;
}

#define ADDBUF(_VAL_) opl.raw.buffer[opl.raw.used++]=_VAL_;
static void OPL_RawAdd(Bitu index,Bitu val) {
	/* Check if we have yet to start */
	if (!opl.raw.start) {
		Bitu i;
		opl.raw.last=PIC_Ticks;	
		opl.raw.start=PIC_Ticks;
		/* Check the registers to add */
		for (i=4;i<256;i++) {
			if (!opl.raw.regs[0][i]) continue;
			if ((i &0xe0)==0xb0) continue;
			ADDBUF((Bit8u)i);
			ADDBUF(opl.raw.regs[0][i]);
		}
		bool donesecond=false;
		for (i=4;i<256;i++) {
			if (!opl.raw.regs[0][i]) continue;
			if ((i&0xe0)==0xb0) continue;
			if (!donesecond) {
				donesecond=true;
				ADDBUF(0x3);
			}
			ADDBUF((Bit8u)i);
			ADDBUF(opl.raw.regs[0][i]);
		}
		if (donesecond) ADDBUF(0x2);
	}
	/* Check how much time has passed, Allow an extra 5 milliseconds? */
	if (PIC_Ticks>(opl.raw.last+5)) {
		Bitu passed=PIC_Ticks-opl.raw.last;
		opl.raw.last=PIC_Ticks;
		while (passed) {
			passed-=1;
			if (passed<256) {
				ADDBUF(0x00);					//8bit delay
				ADDBUF((Bit8u)passed);			//8bit delay
				passed=0;
			} else if (passed<0x10000) {
				ADDBUF(0x01);					//16bit delay
				ADDBUF((Bit8u)(passed & 0xff));	//lo-byte
				ADDBUF((Bit8u)(passed >> 8));	//hi-byte
				passed=0;
			} else {
				ADDBUF(0x01);					//16bit delay
				ADDBUF(0xff);					//lo-byte
				ADDBUF(0xff);					//hi-byte
				passed-=0xffff;
			}
		}
	}
	/* Check if we're still sending to the correct index */
	if (opl.raw.index != index) {
		opl.raw.index=index;
		ADDBUF(opl.raw.index ? 0x3 : 0x2);
	}
	ADDBUF(opl.raw.cmd[opl.raw.index]);
	ADDBUF(val);
	if (opl.raw.used>=RAW_SIZE) OPL_RawEmptyBuffer();
}

static void OPL_SaveRawEvent(void) {
	/* Check for previously opened wave file */
	if (opl.raw.handle) {
		LOG_MSG("Stopping Raw OPL capturing.");
		OPL_RawEmptyBuffer();
		/* Fill in the header with useful information */
		host_writed(&dro_header[0x08],opl.raw.last-opl.raw.start);
		host_writed(&dro_header[0x0c],opl.raw.done);
		switch (opl.mode) {
		case OPL_opl2:host_writeb(&dro_header[0x10],0x0);break;
		case OPL_opl3:host_writeb(&dro_header[0x10],0x1);break;
		case OPL_dualopl2:host_writeb(&dro_header[0x10],0x2);break;
		}
		fseek(opl.raw.handle,0,0);
		fwrite(dro_header,1,sizeof(dro_header),opl.raw.handle);
		fclose(opl.raw.handle);
		opl.raw.handle=0;
		return;
	}
	opl.raw.handle=OpenCaptureFile("Raw Opl",".dro");
	opl.raw.index=0;
	opl.raw.used=0;
	opl.raw.done=0;
	opl.raw.start=0;
	opl.raw.last=0;
	memset(opl.raw.buffer,0,sizeof(opl.raw.buffer));
	fwrite(dro_header,1,sizeof(dro_header),opl.raw.handle);
}

static void OPL_Stop(Section* sec) {
	if (opl.raw.handle) OPL_SaveRawEvent();
}

void OPL_Init(Section* sec,Bitu base,OPL_Mode oplmode,Bitu rate) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	if (OPL2::YM3812Init(2,OPL2_INTERNAL_FREQ,rate)) {
		E_Exit("Can't create OPL2 Emulator");	
	};
	OPL2::YM3812SetTimerHandler(0,OPL2::TimerHandler,0);
	OPL2::YM3812SetTimerHandler(1,OPL2::TimerHandler,256);
	if (THEOPL3::YMF262Init(1,OPL3_INTERNAL_FREQ,rate)) {
		E_Exit("Can't create OPL3 Emulator");	
	};
	THEOPL3::YMF262SetTimerHandler(0,THEOPL3::TimerHandler,0);
	IO_RegisterWriteHandler(0x388,OPL_Write,IO_MB,4);
	IO_RegisterReadHandler(0x388,OPL_Read,IO_MB,4);
	if (oplmode>=OPL_dualopl2) {
		IO_RegisterWriteHandler(base,OPL_Write,IO_MB,4);
		IO_RegisterReadHandler(base,OPL_Read,IO_MB,4);
	}

	IO_RegisterWriteHandler(base+8,OPL_Write,IO_MB,2);
	IO_RegisterReadHandler(base+8,OPL_Read,IO_MB,2);

	opl.active=false;
	opl.last_used=0;
	opl.mode=oplmode;
	memset(opl.raw.regs,0,sizeof(opl.raw.regs));
	opl.chan=MIXER_AddChannel(OPL_CallBack,rate,"FM");
	MIXER_SetMode(opl.chan,(opl.mode>OPL_opl2) ? MIXER_16STEREO : MIXER_16MONO);
	MIXER_Enable(opl.chan,false);
	MAPPER_AddHandler(OPL_SaveRawEvent,MK_f7,MMOD1|MMOD2,"caprawopl","Cap OPL");
	sec->AddDestroyFunction(&OPL_Stop);
};

