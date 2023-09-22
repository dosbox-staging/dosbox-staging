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


#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"

enum {
	PIT_HACK_NONE=0,

	PIT_HACK_PROJECT_ANGEL_DEMO,
	PIT_HACK_PC_SPEAKER_AS_TIMER
};

static int pit_hack_mode = PIT_HACK_NONE;

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
	bool counterstatus_set;
	bool counting;
	bool update_count;
};

static PIT_Block pit[3];
static bool gate2;

static Bit8u latched_timerstatus;
// the timer status can not be overwritten until it is read or the timer was 
// reprogrammed.
static bool latched_timerstatus_locked;

static void PIT0_Event(Bitu /*val*/) {
	PIC_ActivateIRQ(0);
	if (pit[0].mode != 0) {
		pit[0].start += pit[0].delay;

		if (GCC_UNLIKELY(pit[0].update_count)) {
			pit[0].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[0].cntr));
			pit[0].update_count=false;
		}
		// regression to r3533 fixes flight simulator 5.0
 		double error = 	pit[0].start - PIC_FullIndex();
 		PIC_AddEvent(PIT0_Event,(float)(pit[0].delay + error));			
//		PIC_AddEvent(PIT0_Event,pit[0].delay); // r3534
	}
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
	case 4:
		//Only low on terminal count
		// if(fmod(index,(double)p->delay) == 0) return false; //Maybe take one rate tick in consideration
		//Easiest solution is to report always high (Space marines uses this mode)
		return true;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading output",p->mode);
		return true;
	}
}
static void status_latch(Bitu counter) {
	// the timer status can not be overwritten until it is read or the timer was 
	// reprogrammed.
	if(!latched_timerstatus_locked)	{
		PIT_Block * p=&pit[counter];
		latched_timerstatus=0;
		// Timer Status Word
		// 0: BCD 
		// 1-3: Timer mode
		// 4-5: read/load mode
		// 6: "NULL" - this is 0 if "the counter value is in the counter" ;)
		// should rarely be 1 (i.e. on exotic modes)
		// 7: OUT - the logic level on the Timer output pin
		if(p->bcd)latched_timerstatus|=0x1;
		latched_timerstatus|=((p->mode&7)<<1);
		if((p->read_state==0)||(p->read_state==3)) latched_timerstatus|=0x30;
		else if(p->read_state==1) latched_timerstatus|=0x10;
		else if(p->read_state==2) latched_timerstatus|=0x20;
		if(counter_output(counter)) latched_timerstatus|=0x80;
		if(p->new_mode) latched_timerstatus|=0x40;
		// The first thing that is being read from this counter now is the
		// counter status.
		p->counterstatus_set=true;
		latched_timerstatus_locked=true;
	}
}
static void counter_latch(Bitu counter) {
	/* Fill the read_latch of the selected counter with current count */
	PIT_Block * p=&pit[counter];
	p->go_read_latch=false;

	//If gate2 is disabled don't update the read_latch
	if (counter == 2 && !gate2 && p->mode !=1) return;
	if (GCC_UNLIKELY(p->new_mode)) {
		double passed_time = PIC_FullIndex() - p->start;
		Bitu ticks_since_then = (Bitu)(passed_time / (1000.0/PIT_TICK_RATE));
		//if (p->mode==3) ticks_since_then /= 2; // TODO figure this out on real hardware
		p->read_latch -= ticks_since_then;
		return;
	}
	double index=PIC_FullIndex()-p->start;
	switch (p->mode) {
	case 4:		/* Software Triggered Strobe */
	case 0:		/* Interrupt on Terminal Count */
		if (counter == 2 && pit_hack_mode == PIT_HACK_PC_SPEAKER_AS_TIMER) {
			/* Needed for Impact Studios "Legend". Perhaps the need for this hack is
			 * a sign that some parts of DOSBox's PIT emulation still needs work. */
			double f;

			index *= 4; /* <- FIXME: Why does this work? Seems best setting when cycles=10000 */
			/* ^ NTS: Setting too high makes the ball bounce TOO fast, and some parts
			 *        sporadically fast-forward. */

			f = index*(PIT_TICK_RATE/1000.0);
			if (f > p->cntr) f = p->cntr;
			p->read_latch = p->cntr - (Bit16u)f;
		}
		else {
			/* Counter keeps on counting after passing terminal count */
			if(p->bcd) {
				index = fmod(index,(1000.0/PIT_TICK_RATE)*10000.0);
				p->read_latch = (Bit16u)(((unsigned long)(p->cntr-index*(PIT_TICK_RATE/1000.0))) % 10000UL);
			} else {
				index = fmod(index,(1000.0/PIT_TICK_RATE)*(double)0x10000);
				p->read_latch = (Bit16u)(p->cntr-index*(PIT_TICK_RATE/1000.0));
			}
		}
		break;
	case 1: // countdown
		if(p->counting) {
			if (index>p->delay) { // has timed out
				p->read_latch = 0xffff; //unconfirmed
			} else {
				p->read_latch=(Bit16u)(p->cntr-index*(PIT_TICK_RATE/1000.0));
			}
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
		// In mode 3 it never returns odd numbers LSB (if odd number is written 1 will be
		// subtracted on first clock and then always 2)
		// fixes "Corncob 3D"
		p->read_latch&=0xfffe;
		break;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading counter %d",p->mode,counter);
		p->read_latch=0xffff;
		break;
	}
}


static void write_latch(Bitu port,Bitu val,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port %X write:%X state:%X",port,val,pit[port-0x40].write_state);
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
		Bitu prev_cntr = p->cntr;

		if (p->write_latch == 0) {
			if (p->bcd == false) p->cntr = 0x10000;
			else p->cntr=9999;
		} else p->cntr = p->write_latch;

		if ((!p->new_mode) && (p->mode == 2) && (counter == 0)) {
			// In mode 2 writing another value has no direct effect on the count
			// until the old one has run out. This might apply to other modes too.
			// This is not fixed for PIT2 yet!!
			p->update_count=true;
			return;
		}
		p->start=PIC_FullIndex();
		p->delay=(1000.0f/((float)PIT_TICK_RATE/(float)p->cntr));

		switch (pit_hack_mode) {
			case PIT_HACK_PROJECT_ANGEL_DEMO:
				if (counter == 0) {
					/* Project Angel PIT hack. The demo is constantly fiddling around with
					 * the counter value in ways that can sometimes cause the demo to hang,
					 * or in ways that prevent the demo from starting or can cause the demo
					 * to run at half speed.
					 *
					 * Perhaps the programmers learned the hard way that switching the counter
					 * rapidly between 18Hz and 421Hz is not a good way to run a demo.
					 *
					 * We force the counter value to one of two values to forcibly stabilize
					 * the demo's timing. Doing this also fixes the VGA tearline that is
					 * visible in the demo's Mode-X parts.
					 *
					 * I also noticed that without this hack, the music in the demo is prone
					 * to skip forward suddenly during BIOS video mode changes, which this
					 * hack also resolves.
					 *
					 * NTS: We do not modify the counter value, because that breaks the demo
					 * too when it reads back a different value than it wrote. Instead, we
					 * ignore the counter value it wrote and force a delay value. */
					/* NTS: We also force the higher rate if we detect that it wrote 18.2Hz
					 * more than once, as that is a sign it hung at startup */
					if (p->cntr > 64000 && prev_cntr > 64000)
						LOG_MSG("PIT hack for Project Angel: 18.2Hz was written twice---did the demo hang? Forcing timer to higher rate.\n");

					if (p->cntr > 64000 && prev_cntr <= 64000)
						p->delay=(1000.0f/((float)PIT_TICK_RATE/(float)65536));
					else
						p->delay=(1000.0f/((float)PIT_TICK_RATE/(float)2834));
				}
				break;
			case PIT_HACK_PC_SPEAKER_AS_TIMER:
				break;
		}

		switch (counter) {
		case 0x00:			/* Timer hooked to IRQ 0 */
			if (p->new_mode || p->mode == 0 ) {
				if(p->mode==0) PIC_RemoveEvents(PIT0_Event); // DoWhackaDo demo
				PIC_AddEvent(PIT0_Event,p->delay);
			} else LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer set without new control word");
			LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer at %.4f Hz mode %d",1000.0/p->delay,p->mode);
			break;
		case 0x02:			/* Timer hooked to PC-Speaker */
			PCSPEAKER_SetCounter(p->cntr,p->mode);
			break;
		default:
			LOG(LOG_PIT,LOG_ERROR)("PIT:Illegal timer selected for writing");
		}
		p->new_mode=false;
    }
}

static Bitu read_latch(Bitu port,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port read %X",port);
	Bit32u counter=port-0x40;
	Bit8u ret=0;
	if(GCC_UNLIKELY(pit[counter].counterstatus_set)){
		pit[counter].counterstatus_set = false;
		latched_timerstatus_locked = false;
		ret = latched_timerstatus;
	} else {
		if (pit[counter].go_read_latch == true) 
			counter_latch(counter);

		if( pit[counter].bcd == true) BIN2BCD(pit[counter].read_latch);

		switch (pit[counter].read_state) {
		case 0: /* read MSB & return to state 3 */
			ret=(pit[counter].read_latch >> 8) & 0xff;
			pit[counter].read_state = 3;
			pit[counter].go_read_latch = true;
			break;
		case 3: /* read LSB followed by MSB */
			ret = pit[counter].read_latch & 0xff;
			pit[counter].read_state = 0;
			break;
		case 1: /* read LSB */
			ret = pit[counter].read_latch & 0xff;
			pit[counter].go_read_latch = true;
			break;
		case 2: /* read MSB */
			ret = (pit[counter].read_latch >> 8) & 0xff;
			pit[counter].go_read_latch = true;
			break;
		default:
			E_Exit("Timer.cpp: error in readlatch");
			break;
		}
		if( pit[counter].bcd == true) BCD2BIN(pit[counter].read_latch);
	}
	return ret;
}

static void write_p43(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port 43 %X",val);
	Bitu latch=(val >> 6) & 0x03;
	switch (latch) {
	case 0:
	case 1:
	case 2:
		if ((val & 0x30) == 0) {
			/* Counter latch command */
			counter_latch(latch);
		} else {
			// save output status to be used with timer 0 irq
			bool old_output = counter_output(0);
			// save the current count value to be re-used in undocumented newmode
			counter_latch(latch);
			pit[latch].bcd = (val&1)>0;   
			if (val & 1) {
				if(pit[latch].cntr>=9999) pit[latch].cntr=9999;
			}

			// Timer is being reprogrammed, unlock the status
			if(pit[latch].counterstatus_set) {
				pit[latch].counterstatus_set=false;
				latched_timerstatus_locked=false;
			}
			pit[latch].start = PIC_FullIndex(); // for undocumented newmode
			pit[latch].go_read_latch = true;
			pit[latch].update_count = false;
			pit[latch].counting = false;
			pit[latch].read_state  = (val >> 4) & 0x03;
			pit[latch].write_state = (val >> 4) & 0x03;
			Bit8u mode             = (val >> 1) & 0x07;
			if (mode > 5)
				mode -= 4; //6,7 become 2 and 3

			pit[latch].mode = mode;

			/* If the line goes from low to up => generate irq. 
			 *      ( BUT needs to stay up until acknowlegded by the cpu!!! therefore: )
			 * If the line goes to low => disable irq.
			 * Mode 0 starts with a low line. (so always disable irq)
			 * Mode 2,3 start with a high line.
			 * counter_output tells if the current counter is high or low 
			 * So actually a mode 3 timer enables and disables irq al the time. (not handled) */

			if (latch == 0) {
				PIC_RemoveEvents(PIT0_Event);
				if((mode != 0)&& !old_output) {
					PIC_ActivateIRQ(0);
				} else {
					PIC_DeActivateIRQ(0);
				}
			}
			pit[latch].new_mode = true;
			if (latch == 2) {
				// notify pc speaker code that the control word was written
				PCSPEAKER_SetPITControl(mode);
			}
		}
		break;
    case 3:
		if ((val & 0x20)==0) {	/* Latch multiple pit counters */
			if (val & 0x02) counter_latch(0);
			if (val & 0x04) counter_latch(1);
			if (val & 0x08) counter_latch(2);
		}
		// status and values can be latched simultaneously
		if ((val & 0x10)==0) {	/* Latch status words */
			// but only 1 status can be latched simultaneously
			if (val & 0x02) status_latch(0);
			else if (val & 0x04) status_latch(1);
			else if (val & 0x08) status_latch(2);
		}
		break;
	}
}

void TIMER_SetGate2(bool in) {
	//No changes if gate doesn't change
	if(gate2 == in) return;
	Bit8u & mode=pit[2].mode;
	switch(mode) {
	case 0:
		if(in) pit[2].start = PIC_FullIndex();
		else {
			//Fill readlatch and store it.
			counter_latch(2);
			pit[2].cntr = pit[2].read_latch;
		}
		break;
	case 1:
		// gate 1 on: reload counter; off: nothing
		if(in) {
			pit[2].counting = true;
			pit[2].start = PIC_FullIndex();
		}
		break;
	case 2:
	case 3:
		//If gate is enabled restart counting. If disable store the current read_latch
		if(in) pit[2].start = PIC_FullIndex();
		else counter_latch(2);
		break;
	case 4:
	case 5:
		LOG(LOG_MISC,LOG_WARN)("unsupported gate 2 mode %x",mode);
		break;
	}
	gate2 = in; //Set it here so the counter_latch above works
}

bool TIMER_GetOutput2() {
	return counter_output(2);
}

#include "programs.h"

void PIT_HACK_Set_type(std::string type) {
	if (type == "project_angel_demo") {
		pit_hack_mode = PIT_HACK_PROJECT_ANGEL_DEMO;
		LOG_MSG("PIT: Hacking PIT emulation to stabilize Project Angel demo\n");
	}
	else if (type == "pc_speaker_as_timer") {
		pit_hack_mode = PIT_HACK_PC_SPEAKER_AS_TIMER;
		LOG_MSG("PIT: Hacking PIT emulation to double PIT 2 countdown value\n");
	}
	else {
		pit_hack_mode = PIT_HACK_NONE;
		LOG_MSG("PIT: Hacks disabled\n");
	}
}

class PITHACK_Program : public Program {
public:
	void Run(void) {
		if (cmd->FindString("SET",temp_line,false)) {
			PIT_HACK_Set_type(temp_line);
		}
	}
};

static void PITHACK_ProgramStart(Program * * make) {
	*make=new PITHACK_Program;
}

class TIMER:public Module_base{
private:
	IO_ReadHandleObject ReadHandler[4];
	IO_WriteHandleObject WriteHandler[4];
public:
	TIMER(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);

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
		pit[0].counterstatus_set = false;
		pit[0].update_count = false;
	
		pit[1].bcd = false;
		pit[1].write_state = 1;
		pit[1].read_state = 1;
		pit[1].go_read_latch = true;
		pit[1].cntr = 18;
		pit[1].mode = 2;
		pit[1].write_state = 3;
		pit[1].counterstatus_set = false;
	
		pit[2].read_latch=1320;	/* MadTv1 */
		pit[2].write_state = 3; /* Chuck Yeager */
		pit[2].read_state = 3;
		pit[2].mode=3;
		pit[2].bcd=false;   
		pit[2].cntr=1320;
		pit[2].go_read_latch=true;
		pit[2].counterstatus_set = false;
		pit[2].counting = false;
	
		pit[0].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[0].cntr));
		pit[1].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[1].cntr));
		pit[2].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[2].cntr));

		Prop_multival* p = section->Get_multival("pit hack");
		std::string type = p->GetSection()->Get_string("type");

		PIT_HACK_Set_type(type);
		PROGRAMS_MakeFile("PITHACK.COM",PITHACK_ProgramStart);

		latched_timerstatus_locked=false;
		gate2 = false;
		PIC_AddEvent(PIT0_Event,pit[0].delay);
	}
	~TIMER(){
		PIC_RemoveEvents(PIT0_Event);
	}
};
static TIMER* test;

void TIMER_Destroy(Section*){
	delete test;
}
void TIMER_Init(Section* sec) {
	test = new TIMER(sec);
	sec->AddDestroyFunction(&TIMER_Destroy);
}



//save state support
void *PIT0_Event_PIC_Event = (void*)PIT0_Event;

