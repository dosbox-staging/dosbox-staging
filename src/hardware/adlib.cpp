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
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
/* 
	Thanks to vdmsound for nice simple way to implement this
*/

namespace MAME {
  /* Defines */
# define logerror(x)

#ifdef _MSC_VER
  /* Disable recurring warnings */
# pragma warning ( disable : 4018 )
# pragma warning ( disable : 4244 )
#endif

  /* Work around ANSI compliance problem (see driver.h) */
  struct __MALLOCPTR {
    void* m_ptr;

    __MALLOCPTR(void) : m_ptr(NULL) { }
    __MALLOCPTR(void* src) : m_ptr(src) { }
    void* operator=(void* rhs) { return (m_ptr = rhs); }
    operator int*() const { return (int*)m_ptr; }
    operator int**() const { return (int**)m_ptr; }
    operator char*() const { return (char*)m_ptr; }
  };

  /* Bring in the MAME OPL emulation */
# define HAS_YM3812 1
# include "fmopl.c"

}


struct OPLTimer_t {
	bool isEnabled;
	bool isMasked;
	bool isOverflowed;
	Bit64u count;
	Bit64u base;
};

static OPLTimer_t timer1,timer2;
static Bit8u regsel;

#define OPL_INTERNAL_FREQ     3600000   // The OPL operates at 3.6MHz
#define OPL_NUM_CHIPS         1         // Number of OPL chips
#define OPL_CHIP0             0

static MIXER_Channel * adlib_chan;

static void ADLIB_CallBack(Bit8u *stream, Bit32u len) {
	/* Check for size to update and check for 1 ms updates to the opl registers */
	/* Calculate teh machine ms we are at now */

	/* update 1 ms of data */
	MAME::YM3812UpdateOne(0,(MAME::INT16 *)stream,len);
}

static Bit8u read_p388(Bit32u port) {
	Bit8u ret=0;
	Bit64u micro=PIC_MicroCount();
	if (timer1.isEnabled) {
		if ((micro-timer1.base)>timer1.count) {
			timer1.isOverflowed=true;
			timer1.base=micro;
		}
		if (timer1.isOverflowed || !timer1.isMasked) {
			ret|=0xc0;
		}
	}
	if (timer2.isEnabled) {
		if ((micro-timer2.base)>timer2.count) {
			timer2.isOverflowed=true;
			timer2.base=micro;
		}
		if (timer2.isOverflowed || !timer2.isMasked) {
			ret|=0xA0;
		}
	}
	return ret;
}

static void write_p388(Bit32u port,Bit8u val) {
	regsel=val;

	// The following writes this value to ultrasounds equivalent register.
	// I don't know of any other way to do this
	IO_Write(0x248,val);
}

static void write_p389(Bit32u port,Bit8u val) {
	switch (regsel) {
		case 0x02:	/* Timer 1 */
			timer1.count=val*80;
			return;
		case 0x03:	/* Timer 2 */
			timer2.count=val*320;
			return;
		case 0x04:	/* IRQ clear / mask and Timer enable */
			if (val&0x80) {
				timer1.isOverflowed=false;
				timer2.isOverflowed=false;
				return;
			}
			if (val&0x40) timer1.isMasked=true;
			else timer1.isMasked=false;

			if (val&1) {
				timer1.isEnabled=true;
				timer1.base=PIC_MicroCount();
			} else timer1.isEnabled=false;
			if (val&0x20) timer2.isMasked=true;
			else timer2.isMasked=false;
			if (val&2) {
				timer2.isEnabled=true;
				timer2.base=PIC_MicroCount();
			} else timer2.isEnabled=false;
			return;
		default:		/* Normal OPL call queue it */
			/* Use a little hack to directly write to the register */
			MAME::OPLWriteReg(MAME::OPL_YM3812[0],regsel,val);
	}
}

static bool adlib_enabled;

static void ADLIB_Enable(bool enable) {
	if (enable) {
		adlib_enabled=true;
		MIXER_Enable(adlib_chan,true);
		IO_RegisterWriteHandler(0x388,write_p388,"ADLIB Register select");
		IO_RegisterWriteHandler(0x389,write_p389,"ADLIB Data Write");
		IO_RegisterReadHandler(0x388,read_p388,"ADLIB Status");

		IO_RegisterWriteHandler(0x220,write_p388,"ADLIB Register select");
		IO_RegisterWriteHandler(0x221,write_p389,"ADLIB Data Write");
		IO_RegisterReadHandler(0x220,read_p388,"ADLIB Status");
	} else {
		adlib_enabled=false;
		MIXER_Enable(adlib_chan,false);
		IO_FreeWriteHandler(0x220);
		IO_FreeWriteHandler(0x221);
		IO_FreeReadHandler(0x220);
		IO_FreeWriteHandler(0x388);
		IO_FreeWriteHandler(0x389);
		IO_FreeReadHandler(0x388);
	}
}


void ADLIB_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("adlib")) return;

	timer1.isMasked=true;
	timer1.base=0;
	timer1.count=0;
	timer1.isEnabled=false;
	timer1.isOverflowed=false;

	timer2.isMasked=true;
	timer2.base=0;
	timer2.count=0;
	timer2.isEnabled=false;
	timer2.isOverflowed=false;

	#define ADLIB_FREQ 22050
	if (MAME::YM3812Init(OPL_NUM_CHIPS,OPL_INTERNAL_FREQ,ADLIB_FREQ)) {
		E_Exit("Can't create adlib OPL Emulator");	
	};


	adlib_chan=MIXER_AddChannel(ADLIB_CallBack,ADLIB_FREQ,"ADLIB");
	MIXER_SetMode(adlib_chan,MIXER_16MONO);
	ADLIB_Enable(true);	
};

