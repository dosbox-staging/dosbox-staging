/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

/* $Id: timer.cpp,v 1.32 2005-03-25 11:52:32 qbix79 Exp $ */

#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"

static INLINE void BIN2BCD(Bit16u& val) {
	Bit16u temp=val%10 + (((val/10)%10)<<4)+ (((val/100)%10)<<8) + (((val/1000)%10)<<12);
	val=temp;
}

static INLINE void BCD2BIN(Bit16u& val) {
	Bit16u temp= (val&0x0f) +((val>>4)&0x0f) *10 +((val>>8)&0x0f) *100 +((val>>12)&0x0f) *1000;
	val=temp;
}

struct PIT_Block {
	Bitu cntr;
	float delay;
	double start;

	Bit16u read_latch;
	Bit16u write_latch;

	Bit8u mode;
	Bit8u latch_mode;
	Bit8u read_state;
	Bit8u write_state;

	bool bcd;
	bool go_read_latch;
	bool new_mode;
};

static PIT_Block pit[3];

static void PIT0_Event(Bitu val) {
	PIC_ActivateIRQ(0);
	if (pit[0].mode!=0) PIC_AddEvent(PIT0_Event,pit[0].delay);
}

static bool counter_output(Bitu counter) {
	PIT_Block * p=&pit[counter];
	double index=PIC_FullIndex()-p->start;
	switch (p->mode) {
	case 0:
		if (p->new_mode) return false;
		if (index>p->delay) return true;
		else return false;
		break;
	case 2:
		if (p->new_mode) return true;
		index=fmod(index,(double)p->delay);
		return index>0;
	case 3:
		if (p->new_mode) return true;
		index=fmod(index,(double)p->delay);
		return index*2<p->delay;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading output",p->mode);
		return true;
	}
}

static void counter_latch(Bitu counter) {
	/* Fill the read_latch of the selected counter with current count */
	PIT_Block * p=&pit[counter];
	p->go_read_latch=false;
	double index=PIC_FullIndex()-p->start;
	switch (p->mode) {
	case 4:		/* Software Triggered Strobe */
	case 0:		/* Interrupt on Terminal Count */
		/* Counter keeps on counting after passing terminal count */
		if (index>p->delay) {
			index-=p->delay;
			index=fmod(index,(1000.0/PIT_TICK_RATE)*0x1000);
			p->read_latch=(Bit16u)(0xffff-index*0xffff);
		} else {
			p->read_latch=(Bit16u)(p->cntr-index*(PIT_TICK_RATE/1000.0));
		}
		break;
	case 2:		/* Rate Generator */
		index=fmod(index,(double)p->delay);
		p->read_latch=(Bit16u)(p->cntr - (index/p->delay)*p->cntr);
		break;
	case 3:		/* Square Wave Rate Generator */
		index=fmod(index,(double)p->delay);
		index*=2;
		if (index>p->delay) index-=p->delay;
		p->read_latch=(Bit16u)(p->cntr - (index/p->delay)*p->cntr);
		break;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading counter %d",p->mode,counter);
		p->read_latch=0xffff;
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
	if (p->bcd==true) BCD2BIN(p->write_latch);
   	if (p->write_state != 0) {
		if (p->write_latch == 0) {
			if (p->bcd == false) p->cntr = 0x10000;
			else p->cntr=9999;
		} else p->cntr = p->write_latch;
		p->start=PIC_FullIndex();
		p->delay=(1000.0f/((float)PIT_TICK_RATE/(float)p->cntr));
		switch (counter) {
		case 0x00:			/* Timer hooked to IRQ 0 */
			if (p->new_mode || p->mode == 0 ) {
				p->new_mode=false;			
				PIC_AddEvent(PIT0_Event,p->delay);
			} else LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer set without new control word");
			LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer at %.2f Hz mode %d",1000.0/p->delay,p->mode);
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
		if (val & 1) {
			if(pit[latch].cntr>=9999) pit[latch].cntr=9999;
		}

		if ((val & 0x30) == 0) {
			/* Counter latch command */
			counter_latch(latch);
		} else {
			pit[latch].read_state  = (val >> 4) & 0x03;
			pit[latch].write_state = (val >> 4) & 0x03;
			pit[latch].mode        = (val >> 1) & 0x07;
			if (pit[latch].mode>5)
				pit[latch].mode-=4; //6,7 become 2 and 3
			if (latch==0) {
				PIC_RemoveEvents(PIT0_Event);
				if (!counter_output(0) && pit[latch].mode)
					PIC_ActivateIRQ(0);
			}
			pit[latch].new_mode	   = true;
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


class TIMER:public Module_base{
private:
	IO_ReadHandleObject ReadHandler[4];
	IO_WriteHandleObject WriteHandler[4];
public:
	TIMER(Section* configuration):Module_base(configuration){
		WriteHandler[0].Install(0x40,write_latch,IO_MB);
	//	WriteHandler[1].Install(0x41,write_latch,IO_MB);
		WriteHandler[2].Install(0x42,write_latch,IO_MB);
		WriteHandler[3].Install(0x43,write_p43,IO_MB);
		ReadHandler[0].Install(0x40,read_latch,IO_MB);
		ReadHandler[1].Install(0x41,read_latch,IO_MB);
		ReadHandler[2].Install(0x42,read_latch,IO_MB);
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
		pit[1].write_state = 1;
		pit[1].read_state = 1;
		pit[1].go_read_latch = true;
		pit[1].cntr = 18;
		pit[1].mode = 2;
		pit[1].write_state = 3;   
	
		pit[2].read_latch=0;	/* MadTv1 */
		pit[2].write_state = 3; /* Chuck Yeager */
		pit[2].read_state = 3;
		pit[2].mode=3;
		pit[2].bcd=false;   
		pit[2].cntr=1320;
		pit[2].go_read_latch=true;
	
		pit[0].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[0].cntr));
		pit[1].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[1].cntr));
		pit[2].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[2].cntr));
	
		PIC_AddEvent(PIT0_Event,pit[0].delay);
	}
	~TIMER(){
		PIC_RemoveEvents(PIT0_Event);
	}
};
static TIMER* test;

void TIMER_Destroy(Section* sec){
	delete test;
}
void TIMER_Init(Section* sec) {
	test = new TIMER(sec);
	sec->AddDestroyFunction(&TIMER_Destroy);
}
