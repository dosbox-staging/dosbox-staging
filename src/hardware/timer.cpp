/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "support.h"

const std::chrono::steady_clock::time_point system_start_time = std::chrono::steady_clock::now();

static inline void BIN2BCD(uint16_t& val) {
	const auto b = ((val / 10) % 10) << 4;
	const auto c = ((val / 100) % 10) << 8;
	const auto d = ((val / 1000) % 10) << 12;
	assert(b + c + d <= UINT16_MAX);

	const uint16_t temp = (val % 10) + static_cast<uint16_t>(b + c + d);
	val = temp;
}

static inline void BCD2BIN(uint16_t& val) {
	uint16_t temp= (val&0x0f) +((val>>4)&0x0f) *10 +((val>>8)&0x0f) *100 +((val>>12)&0x0f) *1000;
	val=temp;
}

struct PIT_Block {
	uint32_t cntr;
	double delay;
	double start;

	uint16_t read_latch;
	uint16_t write_latch;

	uint8_t mode;
	uint8_t latch_mode;
	uint8_t read_state;
	uint8_t write_state;

	bool bcd;
	bool go_read_latch;
	bool new_mode;
	bool counterstatus_set;
	bool counting;
	bool update_count;
};

static PIT_Block pit[3];
static bool gate2;

static uint8_t latched_timerstatus;
// the timer status can not be overwritten until it is read or the timer was 
// reprogrammed.
static bool latched_timerstatus_locked;

static void PIT0_Event(uint32_t /*val*/)
{
	PIC_ActivateIRQ(0);
	if (pit[0].mode != 0) {
		pit[0].start += pit[0].delay;

		if (GCC_UNLIKELY(pit[0].update_count)) {
			pit[0].delay = (1000.0 / (static_cast<double>(PIT_TICK_RATE) /
			                          (double)pit[0].cntr));
			pit[0].update_count = false;
		}
		PIC_AddEvent(PIT0_Event,pit[0].delay);
	}
}

static bool counter_output(const uint32_t counter)
{
	PIT_Block *p = &pit[counter];
	auto index = PIC_FullIndex() - p->start;
	switch (p->mode) {
	case 0:
		if (p->new_mode) return false;
		if (index>p->delay) return true;
		else return false;
		break;
	case 2:
		if (p->new_mode) return true;
		index = fmod(index, p->delay);
		return index>0;
	case 3:
		if (p->new_mode) return true;
		index = fmod(index, p->delay);
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
static void status_latch(const uint32_t counter)
{
	// the timer status can not be overwritten until it is read or the timer
	// was reprogrammed.
	if (!latched_timerstatus_locked) {
		PIT_Block *p = &pit[counter];
		latched_timerstatus = 0;
		// Timer Status Word
		// 0: BCD
		// 1-3: Timer mode
		// 4-5: read/load mode
		// 6: "NULL" - this is 0 if "the counter value is in the
		// counter" ;) should rarely be 1 (i.e. on exotic modes) 7: OUT
		// - the logic level on the Timer output pin
		if (p->bcd)
			latched_timerstatus |= 0x1;
		latched_timerstatus |= ((p->mode & 7) << 1);
		if ((p->read_state == 0) || (p->read_state == 3))
			latched_timerstatus |= 0x30;
		else if (p->read_state == 1)
			latched_timerstatus |= 0x10;
		else if (p->read_state == 2)
			latched_timerstatus |= 0x20;
		if (counter_output(counter))
			latched_timerstatus |= 0x80;
		if (p->new_mode)
			latched_timerstatus |= 0x40;
		// The first thing that is being read from this counter now is
		// the counter status.
		p->counterstatus_set = true;
		latched_timerstatus_locked = true;
	}
}
static void counter_latch(uint32_t counter)
{
	/* Fill the read_latch of the selected counter with current count */
	PIT_Block * p=&pit[counter];
	p->go_read_latch=false;

	//If gate2 is disabled don't update the read_latch
	if (counter == 2 && !gate2 && p->mode !=1) return;

	auto elapsed_ms = PIC_FullIndex() - p->start;
	auto save_read_latch = [p](double latch_time) {
		// Latch is a 16-bit counter, so ensure it doesn't overflow
		const auto bound_latch = clamp(static_cast<int>(latch_time), 0,
		                               static_cast<int>(UINT16_MAX));
		p->read_latch = static_cast<uint16_t>(bound_latch);
	};

	if (GCC_UNLIKELY(p->new_mode)) {
		const auto total_ticks = static_cast<uint32_t>(elapsed_ms /
		                                               PERIOD_OF_1K_PIT_TICKS);
		// if (p->mode==3) ticks_since_then /= 2; // TODO figure this
		// out on real hardware
		save_read_latch(p->read_latch - total_ticks);
		return;
	}
	const auto cntr = static_cast<double>(p->cntr);
	switch (p->mode) {
	case 4:         /* Software Triggered Strobe */
	case 0:		/* Interrupt on Terminal Count */
		/* Counter keeps on counting after passing terminal count */
		if (elapsed_ms > p->delay) {
			elapsed_ms -= p->delay;
			if (p->bcd) {
				elapsed_ms = fmod(elapsed_ms, PERIOD_OF_1K_PIT_TICKS * 10000.0);
				save_read_latch(9999 - elapsed_ms * PIT_TICK_RATE_KHZ);
			} else {
				elapsed_ms = fmod(elapsed_ms, PERIOD_OF_1K_PIT_TICKS * 0x10000);
				save_read_latch(0xffff - elapsed_ms * PIT_TICK_RATE_KHZ);
			}
		} else {
			save_read_latch(cntr - elapsed_ms * PIT_TICK_RATE_KHZ);
		}
		break;
	case 1: // countdown
		if(p->counting) {
			if (elapsed_ms > p->delay) {     // has timed out
				save_read_latch(0xffff); // unconfirmed
			} else {
				save_read_latch(cntr - elapsed_ms * PIT_TICK_RATE_KHZ);
			}
		}
		break;
	case 2:		/* Rate Generator */
		elapsed_ms = fmod(elapsed_ms, p->delay);
		save_read_latch(cntr - (elapsed_ms / p->delay) * cntr);
		break;
	case 3:		/* Square Wave Rate Generator */
		elapsed_ms = fmod(elapsed_ms, p->delay);
		elapsed_ms *= 2;
		if (elapsed_ms > p->delay)
			elapsed_ms -= p->delay;
		save_read_latch(cntr - (elapsed_ms / p->delay) * cntr);
		// In mode 3 it never returns odd numbers LSB (if odd number is
		// written 1 will be subtracted on first clock and then always
		// 2) fixes "Corncob 3D"
		save_read_latch(p->read_latch & 0xfffe);
		break;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading counter %d",p->mode,counter);
		save_read_latch(0xffff);
		break;
	}
}

static void write_latch(io_port_t port, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	// LOG(LOG_PIT,LOG_ERROR)("port %X write:%X
	// state:%X",port,val,pit[port-0x40].write_state);
	const uint16_t counter = port - 0x40;
	PIT_Block *p = &pit[counter];
	if (p->bcd == true)
		BIN2BCD(p->write_latch);

	switch (p->write_state) {
	case 0:
		// write_latch is 16-bits
		p->write_latch = static_cast<uint16_t>(p->write_latch |
		                                       ((val & 0xff) << 8));
		p->write_state = 3;
		break;
	case 3:
		p->write_latch = val & 0xff;
		p->write_state = 0;
		break;
	case 1: p->write_latch = val & 0xff; break;
	case 2:
		p->write_latch = static_cast<uint16_t>((val & 0xff) << 8);
		break;
	}

	if (p->bcd == true)
		BCD2BIN(p->write_latch);

	if (p->write_state != 0) {
		if (p->write_latch == 0) {
			if (p->bcd == false)
				p->cntr = 0x10000;
			else
				p->cntr = 9999;
		}
		// square wave, count by 2
		else if (p->write_latch == 1 && p->mode == 3)
			// counter==1 and mode==3 makes a low frequency
			// buzz (Paratrooper)
			p->cntr = p->bcd ? 10000 : 0x10001;
		else
			p->cntr = p->write_latch;

		if ((!p->new_mode) && (p->mode == 2) && (counter == 0)) {
			// In mode 2 writing another value has no direct
			// effect on the count until the old one has run
			// out. This might apply to other modes too.
			// This is not fixed for PIT2 yet!!
			p->update_count = true;
			return;
		}
		p->start = PIC_FullIndex();
		p->delay = (1000.0 / ((double)PIT_TICK_RATE / (double)p->cntr));

		switch (counter) {
		case 0x00: /* Timer hooked to IRQ 0 */
			if (p->new_mode || p->mode == 0) {
				if (p->mode == 0) { // DoWhackaDo demo
					PIC_RemoveEvents(PIT0_Event);
				}
				PIC_AddEvent(PIT0_Event, p->delay);
			} else
				LOG(LOG_PIT, LOG_NORMAL)
			("PIT 0 Timer set without new control word");
			LOG(LOG_PIT, LOG_NORMAL)
			("PIT 0 Timer at %.4f Hz mode %d", 1000.0 / p->delay,
			 p->mode);
			break;
		case 0x02: // Timer hooked to PC-Speaker
			// LOG(LOG_PIT,"PIT 2 Timer at %.3g Hz mode %d",
			//     PIT_TICK_RATE/(double)p->cntr,p->mode);
			PCSPEAKER_SetCounter(p->cntr, p->mode);
			break;
		default:
			LOG(LOG_PIT, LOG_ERROR)
			("PIT:Illegal timer selected for writing");
		}
		p->new_mode = false;
	}
}

static uint8_t read_latch(io_port_t port, io_width_t)
{
	// LOG(LOG_PIT,LOG_ERROR)("port read %X",port);
	const uint16_t counter = port - 0x40;
	uint8_t ret = 0;
	if (GCC_UNLIKELY(pit[counter].counterstatus_set)) {
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

static void write_p43(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	// LOG(LOG_PIT,LOG_ERROR)("port 43 %X",val);
	const uint8_t latch = (val >> 6) & 0x03;
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
			// save the current count value to be re-used in
			// undocumented newmode
			counter_latch(latch);
			pit[latch].bcd = (val & 1) > 0;
			if (val & 1) {
				if (pit[latch].cntr >= 9999)
					pit[latch].cntr = 9999;
			}

			// Timer is being reprogrammed, unlock the status
			if (pit[latch].counterstatus_set) {
				pit[latch].counterstatus_set = false;
				latched_timerstatus_locked = false;
			}
			pit[latch].start = PIC_FullIndex(); // for undocumented
			                                    // newmode
			pit[latch].go_read_latch = true;
			pit[latch].update_count = false;
			pit[latch].counting = false;
			pit[latch].read_state = (val >> 4) & 0x03;
			pit[latch].write_state = (val >> 4) & 0x03;
			uint8_t mode = (val >> 1) & 0x07;
			if (mode > 5)
				mode -= 4; // 6,7 become 2 and 3

			pit[latch].mode = mode;

			/* If the line goes from low to up => generate irq.
			 * ( BUT needs to stay up until acknowlegded by the
			 * cpu!!! therefore: ) If the line goes to low =>
			 * disable irq. Mode 0 starts with a low line. (so
			 * always disable irq) Mode 2,3 start with a high line.
			 * counter_output tells if the current counter is high
			 * or low So actually a mode 3 timer enables and
			 * disables irq al the time. (not handled) */

			if (latch == 0) {
				PIC_RemoveEvents(PIT0_Event);
				if ((mode != 0) && !old_output) {
					PIC_ActivateIRQ(0);
				} else {
					PIC_DeActivateIRQ(0);
				}
			} else if (latch == 2) {
				PCSPEAKER_SetCounter(0, 3);
			}
			pit[latch].new_mode = true;
			if (latch == 2) {
				// notify pc speaker code that the control word was written
				PCSPEAKER_SetPITControl(mode);
			}
		}
		break;
	case 3:
		if ((val & 0x20) == 0) { /* Latch multiple pit counters */
			if (val & 0x02)
				counter_latch(0);
			if (val & 0x04)
				counter_latch(1);
			if (val & 0x08)
				counter_latch(2);
		}
		// status and values can be latched simultaneously
		if ((val & 0x10) == 0) { /* Latch status words */
			// but only 1 status can be latched simultaneously
			if (val & 0x02)
				status_latch(0);
			else if (val & 0x04)
				status_latch(1);
			else if (val & 0x08)
				status_latch(2);
		}
		break;
	}
}

void TIMER_SetGate2(bool in) {
	//No changes if gate doesn't change
	if(gate2 == in) return;
	uint8_t & mode=pit[2].mode;
	switch (mode) {
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

bool TIMER_GetOutput2(void) {
	return counter_output(2);
}

class TIMER final : public Module_base{
private:
	IO_ReadHandleObject ReadHandler[4];
	IO_WriteHandleObject WriteHandler[4];
public:
	TIMER(Section* configuration):Module_base(configuration){
		WriteHandler[0].Install(0x40, write_latch, io_width_t::byte);
		//	WriteHandler[1].Install(0x41,write_latch,io_width_t::byte);
		WriteHandler[2].Install(0x42, write_latch, io_width_t::byte);
		WriteHandler[3].Install(0x43, write_p43, io_width_t::byte);
		ReadHandler[0].Install(0x40, read_latch, io_width_t::byte);
		ReadHandler[1].Install(0x41, read_latch, io_width_t::byte);
		ReadHandler[2].Install(0x42, read_latch, io_width_t::byte);
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

		pit[0].delay = (1000.0 / ((double)PIT_TICK_RATE / (double)pit[0].cntr));
		pit[1].delay = (1000.0 / ((double)PIT_TICK_RATE / (double)pit[1].cntr));
		pit[2].delay = (1000.0 / ((double)PIT_TICK_RATE / (double)pit[2].cntr));

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
