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

/* $Id: timer.cpp,v 1.23 2004-04-03 19:34:10 harekiet Exp $ */

#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "bios.h"
#include "mem.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"

static INLINE void BIN2BCD(Bit16u& val) {
	Bit16u temp=val%10 + (((val/10)%10)<<4)+ (((val/100)%10)<<8) + (((val/1000)%10)<<12);
	val=temp;
}

static INLINE void BCD2BIN(Bit16u& val) {
	Bit16u temp= (val&0x0f) +((val>>4)&0x0f) *10 +((val>>8)&0x0f) *100 +((val>>12)&0x0f) *1000;
	val=temp;
}


struct PIT_Block {
	Bit8u mode;								/* Current Counter Mode */
	
	Bitu cntr;
	Bits micro;
	Bit64u start;
	
	Bit8u latch_mode;
	Bit8u read_state;
	Bit16u read_latch;
	Bit8u write_state;
	Bit16u write_latch;
   bool bcd;
   bool go_read_latch;
};

static PIT_Block pit[3];

static void PIT0_Event(Bitu val) {
	PIC_ActivateIRQ(0);
	if (pit[0].mode!=0) PIC_AddEvent(PIT0_Event,pit[0].micro);
}

static void counter_latch(Bitu counter) {
	/* Fill the read_latch of the selected counter with current count */
	PIT_Block * p=&pit[counter];
	p->go_read_latch=false;

	Bit64s micro=PIC_MicroCount()-p->start;
	
	switch (p->mode) {
	case 0:		/* Interrupt on Terminal Count */
		/* Counter keeps on counting after passing terminal count */
		if (micro>p->micro) {
			micro-=p->micro;
			micro%=(Bit64u)(1000000/((float)PIT_TICK_RATE/(float)0x10000));
			p->read_latch=(Bit16u)(0x10000-(((double)micro/(double)p->micro)*(double)0x10000));
		} else {
			p->read_latch=(Bit16u)(p->cntr-(((double)micro/(double)p->micro)*(double)p->cntr));
		}
		break;
	case 2:		/* Rate Generator */
		micro%=p->micro;
		p->read_latch=(Bit16u)(p->cntr-(((double)micro/(double)p->micro)*(double)p->cntr));
		break;
	case 3:		/* Square Wave Rate Generator */
		micro%=p->micro;
		micro*=2;
		if (micro>p->micro) micro-=p->micro;
		p->read_latch=(Bit16u)(p->cntr-(((double)micro/(double)p->micro)*(double)p->cntr));
		break;
	case 4:		/* Software Triggered Strobe */
		if (micro>p->micro) p->read_latch=p->write_latch;
		else p->read_latch=(Bit16u)(p->cntr-(((double)micro/(double)p->micro)*(double)p->cntr));
		break;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading counter %d",p->mode,counter);
		micro%=p->micro;
		p->read_latch=(Bit16u)(p->cntr-(((double)micro/(double)p->micro)*(double)p->cntr));
		break;
	}
}


static void write_latch(Bitu port,Bitu val,Bitu iolen) {
	Bitu counter=port-0x40;
	PIT_Block * p=&pit[counter];
	if(p->bcd == true) BIN2BCD(p->write_latch);
   
	switch (p->write_state) {
		case 0:
			p->write_latch = p->write_latch | ((val & 0xff) << 8);
			p->write_state = 3;
			break;
		case 3:
			p->write_latch = val & 0xff;
			p->write_state = 0;
			break;
		case 1:
			p->write_latch = val & 0xff;
			break;
		case 2:
			p->write_latch = (val & 0xff) << 8;
		break;
	}
	if(p->bcd==true) BCD2BIN(p->write_latch);
   
	if (p->write_state != 0) {
		if (p->write_latch == 0) {
		   if(p->bcd == false) {p->cntr = 0x10000;} else {p->cntr=9999;}
		}
	   
		else p->cntr = p->write_latch;
		p->start=PIC_MicroCount();
		p->micro=(Bits)(1000000/((float)PIT_TICK_RATE/(float)p->cntr));
		switch (counter) {
		case 0x00:			/* Timer hooked to IRQ 0 */
			PIC_RemoveEvents(PIT0_Event);
			PIC_AddEvent(PIT0_Event,p->micro);
			LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer at %.3g Hz mode %d",PIT_TICK_RATE/(double)p->cntr,(Bit32u)p->mode);
			break;
		case 0x02:			/* Timer hooked to PC-Speaker */
//			LOG(LOG_PIT,"PIT 2 Timer at %.3g Hz mode %d",PIT_TICK_RATE/(double)p->cntr,p->mode);
			PCSPEAKER_SetCounter(p->cntr,p->mode);
			break;
		default:
			LOG(LOG_PIT,LOG_ERROR)("PIT:Illegal timer selected for writing");
		}
    }
}

static Bitu read_latch(Bitu port,Bitu iolen) {
	Bit32u counter=port-0x40;
	if (pit[counter].go_read_latch == true) 
		counter_latch(counter);
	Bit8u ret;
	if( pit[counter].bcd == true) BIN2BCD(pit[counter].read_latch);
   
	switch (pit[counter].read_state) {
    case 0: /* read MSB & return to state 3 */
      ret=(pit[counter].read_latch >> 8) & 0xff;
      pit[counter].read_state = 3;
      pit[counter].go_read_latch = true;
      break;
    case 3: /* read LSB followed by MSB */
      ret = (pit[counter].read_latch & 0xff);
      if (pit[counter].mode & 0x80) pit[counter].mode &= 7;	/* moved here */
        else
      pit[counter].read_state = 0;
      break;
    case 1: /* read LSB */
      ret = (pit[counter].read_latch & 0xff);
      pit[counter].go_read_latch = true;
      break;
    case 2: /* read MSB */
      ret = (pit[counter].read_latch >> 8) & 0xff;
      pit[counter].go_read_latch = true;
      break;
	 default:
	   ret=0;
	   E_Exit("Timer.cpp: error in readlatch");
	   break;
	}
	if( pit[counter].bcd == true) BCD2BIN(pit[counter].read_latch);
   
  return ret;
}

static void write_p43(Bitu port,Bitu val,Bitu iolen) {
	Bitu latch=(val >> 6) & 0x03;
	switch (latch) {
	case 0:
	case 1:
	case 2:
		pit[latch].bcd = (val&1)>0;   
		if (val & 1) if(pit[latch].cntr>=9999) pit[latch].cntr=9999;
		
	   
		if ((val & 0x30) == 0) {
			/* Counter latch command */
			counter_latch(latch);
		} else {
			pit[latch].read_state  = (val >> 4) & 0x03;
			pit[latch].write_state = (val >> 4) & 0x03;
			pit[latch].mode        = (val >> 1) & 0x07;
		}
		break;
    case 3:
		if ((val & 0x20)==0) {	/* Latch multiple pit counters */
			if (val & 0x02) counter_latch(0);
			if (val & 0x04) counter_latch(1);
			if (val & 0x08) counter_latch(2);
		} else if ((val & 0x10)==0) {	/* Latch status words */
			LOG(LOG_PIT,LOG_ERROR)("Unsupported Latch status word call");
		} else LOG(LOG_PIT,LOG_ERROR)("Unhandled command:%X",val);
		break;
	}
}


void TIMER_Init(Section* sect) {
	IO_RegisterWriteHandler(0x40,write_latch,IO_MB);
	IO_RegisterWriteHandler(0x42,write_latch,IO_MB);
	IO_RegisterWriteHandler(0x43,write_p43,IO_MB);
	IO_RegisterReadHandler(0x40,read_latch,IO_MB);
//	IO_RegisterReadHandler(0x41,read_p41,IO_MB);
	IO_RegisterReadHandler(0x42,read_latch,IO_MB);
	/* Setup Timer 0 */
	pit[0].cntr=0x10000;
	pit[0].write_state = 3;
	pit[0].read_state = 3;
	pit[0].read_latch=0;
	pit[0].write_latch=0;
	pit[0].mode=3;
	pit[0].bcd = false;
	pit[0].go_read_latch = true;

	pit[1].bcd = false;
	pit[1].go_read_latch = true;
	pit[1].mode = 3;
	pit[1].write_state = 3;   

	pit[0].micro=(Bits)(1000000/((float)PIT_TICK_RATE/(float)pit[0].cntr));
	pit[2].micro=100;
	pit[2].read_latch=0;	/* MadTv1 */
	pit[2].write_state = 3; /* Chuck Yeager */
	pit[2].mode=3;
	pit[2].bcd=false;   
	pit[2].go_read_latch=true;

	PIC_AddEvent(PIT0_Event,pit[0].micro);
}

