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




#include <list>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "bios.h"
#include "mem.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"



struct PIT_Block {
	Bit8u mode;								/* Current Counter Mode */
	
	Bitu cntr;
	Bits micro;
	Bit64u start;
	
	Bit8u latch_mode;
	Bit8u read_state;
	Bit16s read_latch;
	Bit8u write_state;
	Bit16u write_latch;
};

static PIT_Block pit[3];

static void PIT0_Event(void) {
	PIC_ActivateIRQ(0);
	PIC_AddEvent(PIT0_Event,pit[0].micro);
}

static void counter_latch(Bitu counter) {
	/* Fill the read_latch of the selected counter with current count */
	PIT_Block * p=&pit[counter];
	
	Bit64s micro=PIC_MicroCount()-p->start;
	switch (p->mode) {
	case 0:		/* Interrupt on Terminal Count */
		/* Counter keeps on counting after passing terminal count */
		micro%=p->micro;
		p->read_latch=(Bit16u)(p->cntr-(((float)micro/(float)p->micro)*(float)p->cntr));
		break;
	case 2:		/* Rate Generator */
		micro%=p->micro;
		p->read_latch=(Bit16u)(p->cntr-(((float)micro/(float)p->micro)*(float)p->cntr));
		break;
	case 3:		/* Square Wave Rate Generator */
		micro%=p->micro;
		micro*=2;
		if (micro>p->micro) micro-=p->micro;
		p->read_latch=(Bit16u)(p->cntr-(((float)micro/(float)p->micro)*(float)p->cntr));
		break;
	case 4:		/* Software Triggered Strobe */
		if (micro>p->micro) p->read_latch=p->write_latch;
		else p->read_latch=(Bit16u)(p->cntr-(((float)micro/(float)p->micro)*(float)p->cntr));
		break;
	default:
		LOG(LOG_ERROR|LOG_PIT,"Illegal Mode %d for reading counter %d",p->mode,counter);
		micro%=p->micro;
		p->read_latch=(Bit16u)(p->cntr-(((float)micro/(float)p->micro)*(float)p->cntr));
		break;
	}
}


static void write_latch(Bit32u port,Bit8u val) {
	Bitu counter=port-0x40;
	PIT_Block * p=&pit[counter];
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
	if (p->write_state != 0) {
		if (p->write_latch == 0) p->cntr = 0x10000;
		else p->cntr = p->write_latch;
		p->start=PIC_MicroCount();
		p->micro=1000000/((float)PIT_TICK_RATE/(float)p->cntr);
		switch (counter) {
		case 0x00:			/* Timer hooked to IRQ 0 */
			PIC_RemoveEvents(PIT0_Event);
			PIC_AddEvent(PIT0_Event,p->micro);
			LOG(LOG_PIT,"PIT 0 Timer at %.3g Hz mode %d",PIT_TICK_RATE/(double)p->cntr,p->mode);
			break;
		case 0x02:			/* Timer hooked to PC-Speaker */
//			LOG(LOG_PIT,"PIT 2 Timer at %.3g Hz mode %d",PIT_TICK_RATE/(double)p->cntr,p->mode);
			PCSPEAKER_SetCounter(p->cntr,p->mode);
			break;
		default:
			LOG(LOG_ERROR|LOG_PIT,"PIT:Illegal timer selected for writing");
		}
    }
}

static Bit8u read_latch(Bit32u port) {
	Bit32u counter=port-0x40;
	if (pit[counter].read_latch == -1) 
		counter_latch(counter);
	Bit8u ret;
	switch (pit[counter].read_state) {
    case 0: /* read MSB & return to state 3 */
      ret=(pit[counter].read_latch >> 8) & 0xff;
      pit[counter].read_state = 3;
      pit[counter].read_latch = -1;
      break;
    case 3: /* read LSB followed by MSB */
      ret = (pit[counter].read_latch & 0xff);
      if (pit[counter].mode & 0x80) pit[counter].mode &= 7;	/* moved here */
        else
      pit[counter].read_state = 0;
      break;
    case 1: /* read MSB */
      ret = (pit[counter].read_latch >> 8) & 0xff;
      pit[counter].read_latch = -1;
      break;
    case 2: /* read LSB */
      ret = (pit[counter].read_latch & 0xff);
      pit[counter].read_latch = -1;
      break;
	 default:
	   ret=0;
	   E_Exit("Timer.cpp: error in readlatch");
	   break;
  }
  return ret;
}





static void write_p43(Bit32u port,Bit8u val) {
	Bitu latch=(val >> 6) & 0x03;
	switch (latch) {
	case 0:
	case 1:
	case 2:
		if (val & 1) E_Exit("PIT:Timer %d set to unsupported bcd mode",latch);
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
		} else E_Exit("PIT:Latch Timer Status %X",val);
		break;
	}
}

/* The TIMER Part */

enum { T_TICK,T_MICRO,T_DELAY};

struct Timer {
	Bitu type;
	union {
		struct {
			TIMER_TickHandler handler;
		} tick;
		struct{
			Bits left;
			Bits total;
			TIMER_MicroHandler handler;
		} micro;
	};
};

static Timer * first_timer=0;
static std::list<Timer *> Timers;

TIMER_Block * TIMER_RegisterTickHandler(TIMER_TickHandler handler) {
	Timer *	new_timer=new(Timer);
	new_timer->type=T_TICK;
	new_timer->tick.handler=handler;
	Timers.push_front(new_timer);
	return (TIMER_Block *)new_timer;
}

TIMER_Block * TIMER_RegisterMicroHandler(TIMER_MicroHandler handler,Bitu micro) {
	Timer *	new_timer=new(Timer);
	new_timer->type=T_MICRO;
	new_timer->micro.handler=handler;
	Timers.push_front(new_timer);
	TIMER_SetNewMicro(new_timer,micro);
	return (TIMER_Block *)new_timer;
}

void TIMER_SetNewMicro(TIMER_Block * block,Bitu micro) {	
	Timer *	timer=(Timer *)block;	
	if (timer->type!=T_MICRO) E_Exit("TIMER:Illegal handler type");
	timer->micro.total=micro;
	Bitu index=PIC_Index();
	while ((1000-index)>micro) {
		PIC_AddEvent(timer->micro.handler,micro);
		micro+=micro;
		index+=micro;
	}
	timer->micro.left=timer->micro.total-(1000-index);
}

void TIMER_AddTick(void) {
	Bits index;
	/* Check if there are timer handlers that need to be called */
	std::list<Timer *>::iterator i;
	for(i=Timers.begin(); i != Timers.end(); ++i) {
		Timer * timers=(*i);
		switch (timers->type) {
		case T_TICK:
			timers->tick.handler(1);
			break;
		case T_MICRO:
			index=1000;
			while (index>=timers->micro.left) {
				PIC_AddEvent(timers->micro.handler,timers->micro.left);
				index-=timers->micro.left;
				timers->micro.left=timers->micro.total;
			}
			timers->micro.left-=index;
			break;
		default:
			E_Exit("TIMER:Illegal handler type");
		}
	
	}
}

void TIMER_Init(Section* sect) {
	IO_RegisterWriteHandler(0x40,write_latch,"PIT Timer 0");
	IO_RegisterWriteHandler(0x42,write_latch,"PIT Timer 2");
	IO_RegisterWriteHandler(0x43,write_p43,"PIT Mode Control");
	IO_RegisterReadHandler(0x40,read_latch,"PIT Timer 0");
//	IO_RegisterReadHandler(0x41,read_p41,"PIT Timer	1");
	IO_RegisterReadHandler(0x42,read_latch,"PIT Timer 2");
	/* Setup Timer 0 */
	pit[0].cntr=0x10000;
	pit[0].write_state = 3;
	pit[0].read_state = 3;
	pit[0].read_latch=-1;
	pit[0].write_latch=0;
	pit[0].mode=3;

	pit[0].micro=1000000/((float)PIT_TICK_RATE/(float)pit[0].cntr);
	pit[2].micro=100;
	PIC_AddEvent(PIT0_Event,pit[0].micro);
}

