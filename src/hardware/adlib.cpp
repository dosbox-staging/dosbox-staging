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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "timer.h"
#include "hardware.h"
#include "setup.h"
/* 
	Thanks to vdmsound for nice simple way to implement this
*/

namespace MAME {
  /* Defines */
# define logerror(x)
  /* Disable recurring warnings */
#pragma warning ( disable : 4018 )
#pragma warning ( disable : 4244 )

  /* Bring in Tatsuyuki Satoh's OPL emulation */
#define HAS_YM3812 1
#include "fmopl.c"
}

struct OPLTimer_t {
	bool isEnabled;
	bool isMasked;
	bool isOverflowed;
	Bit32u count;
	Bit32u base;
};

static OPLTimer_t timer1,timer2;
static MAME::FM_OPL * myopl;
static Bit8u regsel;
#define OPL_INTERNAL_FREQ     3600000   // The OPL operates at 3.6MHz
static MIXER_Channel * adlib_chan;

static void ADLIB_CallBack(Bit8u *stream, Bit32u len) {
	/* Check for size to update and check for 1 ms updates to the opl registers */
	/* Calculate teh machine ms we are at now */

	/* update 1 ms of data */
	MAME::YM3812UpdateOne(myopl,(MAME::INT16 *)stream,len);
}

static Bit8u read_p388(Bit32u port) {
	Bit8u ret=0;
	Bit32u new_ticks=GetTicks();
	if (timer1.isEnabled) {
		if ((new_ticks-timer1.base)>timer1.count) {
			timer1.isOverflowed=true;
			timer1.base=new_ticks;
		}
		if (timer1.isOverflowed || !timer1.isMasked) {
			ret|=0xc0;
		}
	}
	if (timer2.isEnabled) {
		if ((new_ticks-timer2.base)>timer2.count) {
			timer2.isOverflowed=true;
			timer2.base=new_ticks;
		}
		if (timer2.isOverflowed || !timer2.isMasked) {
			ret|=0xA0;
		}
	}
	return ret;
}

static void write_p388(Bit32u port,Bit8u val) {
	regsel=val;
}

static void write_p389(Bit32u port,Bit8u val) {
	switch (regsel) {
		case 0x02:	/* Timer 1 */
			timer1.count=(val*80/1000);
			return;
		case 0x03:	/* Timer 2 */
			timer2.count=(val*320/1000);
			return;
		case 0x04:	/* IRQ clear / mask and Timer enable */
			if (val&0x80) {
				timer1.isOverflowed=false;
				timer2.isOverflowed=false;
				return;
			}
			if (val&0x40) {
				timer1.isMasked=true;
			} else {
				timer1.isMasked=false;
				timer1.isEnabled=((val&1)>0);
				timer1.base=GetTicks();
			}
			if (val&0x20) {
				timer2.isMasked=true;
			} else {
				timer2.isMasked=false;
				timer2.isEnabled=((val&2)>0);
				timer2.base=GetTicks();
			}
			return;
		default:		/* Normal OPL call queue it */
			MAME::OPLWriteReg(myopl,regsel,val);	
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
	myopl=MAME::OPLCreate(0,OPL_INTERNAL_FREQ,ADLIB_FREQ);

	adlib_chan=MIXER_AddChannel(ADLIB_CallBack,ADLIB_FREQ,"ADLIB");
	MIXER_SetMode(adlib_chan,MIXER_16MONO);
	ADLIB_Enable(true);	
};

