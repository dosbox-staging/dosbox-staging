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
	Bit32s cntr;
	Bit8u latch_mode;
	Bit8u read_state;
	Bit16s read_latch;
	Bit8u write_state;
	Bit16u write_latch;
	Bit32u last_ticks;
};


static PIT_Block pit[3];
static Bit32u pit_ticks;		/* The amount of pit ticks one host tick is bit shifted */
static Bit32u timer_ticks;		/* The amount of pit ticks bitshifted one timer cycle needs */
static Bit32u timer_buildup;		/* The amount of pit ticks waiting */
#define PIT_TICK_RATE 1193182
#define PIT_SHIFT 9
#define MAX_PASSED ((PIT_TICK_RATE/4) << PIT_SHIFT)		/* Alow 1/4 second of timer build up */


static void counter_latch(Bitu counter) {
	/* Fill the read_latch of the selected counter with current count */
	PIT_Block * p=&pit[counter];
//TODO Perhaps make it a bit64u for accuracy :)
	Bit32u ticks=(((LastTicks - p->last_ticks) * pit_ticks) >> PIT_SHIFT) % p->cntr ;
	switch (p->mode) {
	case 2:
	case 3:
		p->read_latch=(Bit16u)ticks;
		break;
	default:
		LOG_ERROR("PIT:Illegal Mode %d for reading counter %d",p->mode,counter);
		p->read_latch=(Bit16u)ticks;
		break;
	}
}



static void write_latch(Bit32u port,Bit8u val) {
	Bit32u counter=port-0x40;
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
		p->last_ticks=LastTicks;
		switch (counter) {
		case 0x00:			/* Timer hooked to IRQ 0 */
			PIC_DeActivateIRQ(0);
			timer_ticks=p->cntr << PIT_SHIFT;
			timer_buildup=0;
			LOG_DEBUG("PIT 0 Timer at %.3g Hz mode %d",PIT_TICK_RATE/(double)p->cntr,p->mode);
			break;
		case 0x02:			/* Timer hooked to PC-Speaker */
			PCSPEAKER_SetFreq(PIT_TICK_RATE/p->cntr);
			break;
		default:
			LOG_ERROR("PIT:Illegal timer selected for writing");
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
	if (val & 1) {
		E_Exit("PIT:BCD Counter not supported");
	}
	Bitu latch=(val >> 6) & 0x03;
	switch (latch) {
	case 0:
	case 1:
	case 2:
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
		E_Exit("Special PIT Latch Read out thing");
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
			Bitu count;
			Bitu total;
			TIMER_MicroHandler handler;
		} micro;
		struct {
			Bitu end;
			TIMER_DelayHandler handler;
		} delay;
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
	new_timer->micro.total=micro;
	new_timer->micro.count=0;
	Timers.push_front(new_timer);
	return (TIMER_Block *)new_timer;
}

TIMER_Block * TIMER_RegisterDelayHandler(TIMER_DelayHandler handler,Bitu delay) {
//Todo maybe check for a too long delay 
	Timer *	new_timer=new(Timer);
	new_timer->type=T_DELAY;
	new_timer->delay.handler=handler;
	new_timer->delay.end=LastTicks+delay;
	Timers.push_front(new_timer);
	return (TIMER_Block *)new_timer;

}
void TIMER_SetNewMicro(TIMER_Block * block,Bitu micro) {	
	Timer *	timer=(Timer *)block;	
	if (timer->type!=T_MICRO) E_Exit("TIMER:Illegal handler type");
	timer->micro.count=0;
	timer->micro.total=micro;
}

void TIMER_AddTicks(Bit32u ticks) {
/* This will run through registered handlers and handle the PIT ticks */
	timer_buildup+=ticks*pit_ticks;
	if (timer_buildup>MAX_PASSED) timer_buildup=MAX_PASSED;
	Bitu add_micro=ticks*1000;
	std::list<Timer *>::iterator i;
	for(i=Timers.begin(); i != Timers.end(); ++i) {
		Timer * timer=(*i);
		switch (timer->type) {
		case T_TICK:
			timer->tick.handler(ticks);
			break;
		case T_MICRO:
			timer->micro.count+=add_micro;
			if (timer->micro.count>=timer->micro.total) {
				timer->micro.count-=timer->micro.total;
				timer->micro.handler();
			}
			break;
		case T_DELAY:
			/* Also unregister the timer handler from the list */
			if (LastTicks>timer->delay.end) {
				std::list<Timer *>::iterator remove;
				timer->delay.handler();
				remove=i++;
				Timers.erase(remove);
			}
			break;
		default:
			E_Exit("TIMER:Illegal handler type");
		};
	
	};
}


void TIMER_CheckPIT(void) {
	if (timer_buildup>timer_ticks) {
		timer_buildup-=timer_ticks;
		PIC_ActivateIRQ(0);
		return;
	}
}


void TIMER_Init(void) {
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
	timer_ticks=pit[0].cntr << PIT_SHIFT;
	timer_buildup=0;
//	first_timer=0;
	pit_ticks=(PIT_TICK_RATE << PIT_SHIFT)/1000;
	PIC_RegisterIRQ(0,&TIMER_CheckPIT,"PIT 0 Timer");
}

