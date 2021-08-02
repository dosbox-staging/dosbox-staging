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

#include "dosbox.h"
#include "inout.h"
#include "cpu.h"
#include "callback.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"

// PIC Controllers
// ~~~~~~~~~~~~~~~
// The sources here identify the two Programmable Interrupt Controllers
// (PICs) as primary and secondary: the prior services IRQs 0 to 7
// while the latter services IRQs 8 to 15.

// In addition to describing the IRQ range for each PIC, the primary and
// secondary terminology also refers to the fact that the CPU is notified by the
// primary PIC, while the secondary PIC signals the primary via IRQ 2.

// It should be noted that some historical documents described the two PICs in a
// "master-slave" relationship, which is misleading given that fact that the
// primary has no control over the secondary.

#define PIC_QUEUESIZE 512

struct PIC_Controller {
	Bitu icw_words;
	Bitu icw_index;
	bool special;
	bool auto_eoi;
	bool rotate_on_auto_eoi;
	bool single;
	bool request_issr;
	Bit8u vector_base;

	Bit8u irr;        // request register
	Bit8u imr;        // mask register
	Bit8u imrr;       // mask register reversed (makes bit tests simpler)
	Bit8u isr;        // in service register
	Bit8u isrr;       // in service register reversed (makes bit tests simpler)
	Bit8u active_irq; //currently active irq


	void set_imr(Bit8u val);

	void check_after_EOI() {
		//Update the active_irq as an EOI is likely to change that.
		update_active_irq();
		if ((irr&imrr)&isrr) check_for_irq();
	}

	void update_active_irq() {
		if (isr == 0) {active_irq = 8; return;}
		for(Bit8u i = 0, s = 1; i < 8;i++, s<<=1){
			if( isr & s){
				active_irq = i;
				return;
			}
		}
	}

	void check_for_irq() {
		const Bit8u possible_irq = (irr&imrr)&isrr;
		if (possible_irq) {
			const Bit8u a_irq = special?8:active_irq;
			for(Bit8u i = 0, s = 1; i < a_irq;i++, s<<=1){
				if ( possible_irq & s ) {
					// There is an IRQ ready to be served,
					// so signal the primary controller
					// and/or CPU.
					activate();
					return;
				}
			}
		}
		deactivate(); // No IRQ, so remove the signal to primary
		              // controller and/or CPU.
	}

	// Signals to the primary controller and/or CPU that there is an IRQ ready.
	void activate();

	// Removes the IRQ-ready signal from the primary controller and/or CPU.
	void deactivate();

	void raise_irq(Bit8u val) {
		const auto bit = static_cast<uint8_t>(1 << val);
		if ((irr & bit) == 0) { //value changed (as it is currently not active)
			irr |= bit;
			if ((bit&imrr)&isrr) { //not masked and not in service
				if (special || val < active_irq) activate();
			}
		}
	}

	void lower_irq(Bit8u val) {
		const auto bit = static_cast<uint8_t>(1 << val);
		if (irr & bit) { //value will change (as it is currently active)
			irr &= ~bit;
			if ((bit&imrr)&isrr) { //not masked and not in service
				// This IRQ might have toggled
				// PIC_IRQCheck/caused IRQ 2 on the primary
				// controller, when it was raised. If it is
				// active, then recheck it, we can't just
				// deactivate as there might be more IRQs
				// raised.
				if (special || val < active_irq)
					check_for_irq();
			}
		}
	}

	//handles all bits and logic related to starting this IRQ, it does NOT start the interrupt on the CPU.
	void start_irq(Bit8u val);
};

static PIC_Controller pics[2];
static PIC_Controller &primary_controller = pics[0];
static PIC_Controller &secondary_controller = pics[1];
uint32_t PIC_Ticks = 0;
uint32_t PIC_IRQCheck = 0; // x86 dynamic core expects a 32 bit variable size

void PIC_Controller::set_imr(Bit8u val) {
	if (GCC_UNLIKELY(machine == MCH_PCJR)) {
		//irq 6 is a NMI on the PCJR
		if (this == &primary_controller)
			val &= ~(1 << (6));
	}
	Bit8u change = (imr) ^ (val); //Bits that have changed become 1.
	imr  =  val;
	imrr = ~val;

	//Test if changed bits are set in irr and are not being served at the moment
	//Those bits have impact on whether the cpu emulation should be paused or not.
	if ((irr & change)&isrr) check_for_irq();
}

void PIC_Controller::activate() {
	// Stop the CPU if this controller is the primary
	if (this == &primary_controller) {
		PIC_IRQCheck = 1;
		//cycles 0, take care of the port IO stuff added in raise_irq base caller.
		CPU_CycleLeft += CPU_Cycles;
		CPU_Cycles = 0;
		//maybe when coming from a EOI, give a tiny delay. (for the cpu to pick it up) (see PIC_Activate_IRQ)
	}
	// Otherwise this controller is the secondary, so signal the primary
	else {
		primary_controller.raise_irq(2);
	}
}

void PIC_Controller::deactivate() {
	// Remove the IRQ check if this controller is the primary
	if (this == &primary_controller) {
		PIC_IRQCheck = 0;
	}
	// Otherwise this controller is the secondary, so signal the primary
	else {
		primary_controller.lower_irq(2);
	}
}

void PIC_Controller::start_irq(Bit8u val) {
	irr &= ~(1<<(val));
	if (!auto_eoi) {
		active_irq = val;
		isr |= 1<<(val);
		isrr = ~isr;
	} else if (GCC_UNLIKELY(rotate_on_auto_eoi)) {
		E_Exit("rotate on auto EOI not handled");
	}
}


struct PICEntry {
	double index;
	Bitu value;
	PIC_EventHandler pic_event;
	PICEntry * next;
};

static struct {
	PICEntry entries[PIC_QUEUESIZE];
	PICEntry * free_entry;
	PICEntry * next_entry;
} pic_queue;

static void write_command(io_port_t port, uint8_t val, io_width_t)
{
	PIC_Controller *pic = &pics[port == 0x20 ? 0 : 1];

	if (GCC_UNLIKELY(val&0x10)) {		// ICW1 issued
		if (val&0x04) E_Exit("PIC: 4 byte interval not handled");
		if (val&0x08) E_Exit("PIC: level triggered mode not handled");
		if (val&0xe0) E_Exit("PIC: 8080/8085 mode not handled");
		pic->set_imr(0);
		pic->single=(val&0x02)==0x02;
		pic->icw_index=1;			// next is ICW2
		pic->icw_words=2 + (val&0x01);	// =3 if ICW4 needed
	} else if (GCC_UNLIKELY(val&0x08)) {	// OCW3 issued
		if (val&0x04) E_Exit("PIC: poll command not handled");
		if (val&0x02) {		// function select
			if (val&0x01) pic->request_issr=true;	/* select read interrupt in-service register */
			else pic->request_issr=false;			/* select read interrupt request register */
		}
		if (val&0x40) {		// special mask select
			if (val&0x20) pic->special = true;
			else pic->special = false;
			//Check if there are irqs ready to run, as the priority system has possibly been changed.
			pic->check_for_irq();
			LOG(LOG_PIC, LOG_NORMAL)("port %#x : special mask %s", port,(pic->special) ? "ON" : "OFF");
		}
	} else {	// OCW2 issued
		if (val&0x20) {		// EOI commands
			if (GCC_UNLIKELY(val&0x80)) E_Exit("rotate mode not supported");
			if (val&0x40) {		// specific EOI
				pic->isr &= ~(1<< ((val-0x60)));
				pic->isrr = ~pic->isr;
				pic->check_after_EOI();
//				if (val&0x80);	// perform rotation
			} else {		// nonspecific EOI
				if (pic->active_irq != 8) { 
					//If there is no irq in service, ignore the call, some games send an eoi to both pics when a sound irq happens (regardless of the irq).
					pic->isr &= ~(1 << (pic->active_irq));
					pic->isrr = ~pic->isr;
					pic->check_after_EOI();
				}
//				if (val&0x80);	// perform rotation
			}
		} else {
			if ((val&0x40)==0) {		// rotate in auto EOI mode
				if (val&0x80) pic->rotate_on_auto_eoi=true;
				else pic->rotate_on_auto_eoi=false;
			} else if (val&0x80) {
				LOG(LOG_PIC,LOG_NORMAL)("set priority command not handled");
			}	// else NOP command
		}
	}	// end OCW2
}

static void write_data(io_port_t port, uint8_t val, io_width_t)
{
	PIC_Controller *pic = &pics[port == 0x21 ? 0 : 1];
	switch (pic->icw_index) {
	case 0: /* mask register */ pic->set_imr(val); break;
	case 1: /* icw2          */
		LOG(LOG_PIC, LOG_NORMAL)("%d:Base vector %u", port == 0x21 ? 0 : 1, val);
		pic->vector_base = val & 0xf8;
		if (pic->icw_index++ >= pic->icw_words)
			pic->icw_index = 0;
		else if (pic->single)
			pic->icw_index = 3; /* skip ICW3 in single mode */
		break;
	case 2:							/* icw 3 */
		LOG(LOG_PIC, LOG_NORMAL)("%d:ICW 3 %u", port == 0x21 ? 0 : 1, val);
		if (pic->icw_index++ >= pic->icw_words)
			pic->icw_index = 0;
		break;
	case 3:							/* icw 4 */
		/*
		        0	    1 8086/8080  0 mcs-8085 mode
		        1	    1 Auto EOI   0 Normal EOI
		        2-3	   0x Non buffer Mode
		                   10 Buffer Mode Secondary controller
		                   11 Buffer mode Primary controller
		        4	      Special/Not Special nested mode
		*/
		pic->auto_eoi=(val & 0x2)>0;

		LOG(LOG_PIC, LOG_NORMAL)("%d:ICW 4 %u", port == 0x21 ? 0 : 1, val);

		if ((val & 0x01) == 0)
			E_Exit("PIC:ICW4: %x, 8085 mode not handled", val);
		if ((val & 0x10) != 0)
			LOG_MSG("PIC:ICW4: %x, special fully-nested mode not handled", val);

		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		break;
	default: LOG(LOG_PIC, LOG_NORMAL)("ICW HUH? %x", val); break;
	}
}

static uint8_t read_command(io_port_t port, io_width_t)
{
	PIC_Controller *pic = &pics[port == 0x20 ? 0 : 1];
	if (pic->request_issr) {
		return pic->isr;
	} else {
		return pic->irr;
	}
}

static uint8_t read_data(io_port_t port, io_width_t)
{
	PIC_Controller *pic = &pics[port == 0x21 ? 0 : 1];
	return pic->imr;
}

// DOS managed up to 15 IRQs
void PIC_ActivateIRQ(const uint8_t irq)
{
	const uint8_t t = irq > 7 ? (irq - 8) : irq;
	PIC_Controller *pic = &pics[irq > 7 ? 1 : 0];

	Bit32s OldCycles = CPU_Cycles;
	pic->raise_irq(t); //Will set the CPU_Cycles to zero if this IRQ will be handled directly

	if (GCC_UNLIKELY(OldCycles != CPU_Cycles)) {
		// if CPU_Cycles have changed, this means that the interrupt was triggered by an I/O
		// register write rather than an event.
		// Real hardware executes 0 to ~13 NOPs or comparable instructions
		// before the processor picks up the interrupt. Let's try with 2
		// cycles here.
		// Required by Panic demo (irq0), It came from the desert (MPU401)
		// Does it matter if CPU_CycleLeft becomes negative?

		// It might be an idea to do this always in order to simulate this
		// So on write mask and EOI as well. (so inside the activate function)
//		CPU_CycleLeft += (CPU_Cycles-2);
		CPU_CycleLeft -= 2;
		CPU_Cycles = 2;
	}
}

// DOS managed up to 15 IRQs
void PIC_DeActivateIRQ(const uint8_t irq)
{
	const uint8_t t = irq > 7 ? (irq - 8) : irq;
	PIC_Controller *pic = &pics[irq > 7 ? 1 : 0];
	pic->lower_irq(t);
}

static void secondary_startIRQ()
{
	uint8_t pic1_irq = 8;
	const uint8_t p = (secondary_controller.irr & secondary_controller.imrr) &
	                  secondary_controller.isrr;
	const uint8_t max = secondary_controller.special
	                            ? 8
	                            : secondary_controller.active_irq;
	for (uint8_t i = 0, s = 1; i < max; i++, s <<= 1) {
		if (p & s) {
			pic1_irq = i;
			break;
		}
	}
	// Maybe change the E_Exit to a return
	if (GCC_UNLIKELY(pic1_irq == 8))
		E_Exit("PIC: IRQ 2 is active, but IRQ is not active on the secondary controller.");

	secondary_controller.start_irq(pic1_irq);
	primary_controller.start_irq(2);
	CPU_HW_Interrupt(secondary_controller.vector_base + pic1_irq);
}

static void inline primary_startIRQ(uint8_t i)
{
	primary_controller.start_irq(i);
	CPU_HW_Interrupt(primary_controller.vector_base + i);
}

void PIC_runIRQs(void) {
	if (!GETFLAG(IF)) return;
	if (GCC_UNLIKELY(!PIC_IRQCheck)) return;
	if (GCC_UNLIKELY(cpudecoder==CPU_Core_Normal_Trap_Run)) return;

	const uint8_t p = (primary_controller.irr & primary_controller.imrr) &
	                  primary_controller.isrr;
	const uint8_t max = primary_controller.special
	                            ? 8
	                            : primary_controller.active_irq;
	for (uint8_t i = 0, s = 1; i < max; i++, s <<= 1) {
		if (p & s) {
			if (i == 2) { // second pic
				secondary_startIRQ();
			} else {
				primary_startIRQ(i);
			}
			break;
		}
	}
	//Disable check variable.
	PIC_IRQCheck = 0;
}

void PIC_SetIRQMask(uint32_t irq, bool masked)
{
	uint32_t t = irq > 7 ? (irq - 8) : irq;
	PIC_Controller *pic = &pics[irq > 7 ? 1 : 0];
	// clear bit
	const auto bit = static_cast<uint8_t>(1 << t);
	Bit8u newmask = pic->imr;
	newmask &= ~bit;
	if (masked) newmask |= bit;
	pic->set_imr(newmask);
}

static void AddEntry(PICEntry * entry) {
	PICEntry * find_entry=pic_queue.next_entry;
	if (GCC_UNLIKELY(find_entry ==0)) {
		entry->next=0;
		pic_queue.next_entry=entry;
	} else if (find_entry->index>entry->index) {
		pic_queue.next_entry=entry;
		entry->next=find_entry;
	} else while (find_entry) {
		if (find_entry->next) {
			/* See if the next index comes later than this one */
			if (find_entry->next->index > entry->index) {
				entry->next=find_entry->next;
				find_entry->next=entry;
				break;
			} else {
				find_entry=find_entry->next;
			}
		} else {
			entry->next=find_entry->next;
			find_entry->next=entry;
			break;
		}
	}
	Bits cycles=PIC_MakeCycles(pic_queue.next_entry->index-PIC_TickIndex());
	if (cycles<CPU_Cycles) {
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=0;
	}
}
static bool InEventService = false;
static double srv_lag = 0.0;

void PIC_AddEvent(PIC_EventHandler handler, double delay, uint32_t val)
{
	if (GCC_UNLIKELY(!pic_queue.free_entry)) {
		LOG(LOG_PIC,LOG_ERROR)("Event queue full");
		return;
	}
	PICEntry * entry=pic_queue.free_entry;
	if(InEventService) entry->index = delay + srv_lag;
	else entry->index = delay + PIC_TickIndex();

	entry->pic_event=handler;
	entry->value=val;
	pic_queue.free_entry=pic_queue.free_entry->next;
	AddEntry(entry);
}

void PIC_RemoveSpecificEvents(PIC_EventHandler handler, uint32_t val)
{
	PICEntry *entry = pic_queue.next_entry;
	PICEntry *prev_entry;
	prev_entry = 0;
	while (entry) {
		if (GCC_UNLIKELY((entry->pic_event == handler)) && (entry->value == val)) {
			if (prev_entry) {
				prev_entry->next=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=prev_entry->next;
				continue;
			} else {
				pic_queue.next_entry=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=pic_queue.next_entry;
				continue;
			}
		}
		prev_entry=entry;
		entry=entry->next;
	}
}

void PIC_RemoveEvents(PIC_EventHandler handler) {
	PICEntry * entry=pic_queue.next_entry;
	PICEntry * prev_entry;
	prev_entry=0;
	while (entry) {
		if (GCC_UNLIKELY(entry->pic_event==handler)) {
			if (prev_entry) {
				prev_entry->next=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=prev_entry->next;
				continue;
			} else {
				pic_queue.next_entry=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=pic_queue.next_entry;
				continue;
			}
		}
		prev_entry=entry;
		entry=entry->next;
	}
}


bool PIC_RunQueue(void) {
	/* Check to see if a new millisecond needs to be started */
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=0;
	if (CPU_CycleLeft<=0) {
		return false;
	}

	const auto index_nd_f = static_cast<double>(PIC_TickIndexND());

	/* Check the queue for an entry */
	InEventService = true;
	while (pic_queue.next_entry &&
	       (pic_queue.next_entry->index * static_cast<double>(CPU_CycleMax) <= index_nd_f)) {
		PICEntry *entry = pic_queue.next_entry;
		pic_queue.next_entry = entry->next;

		srv_lag = entry->index;
		(entry->pic_event)(entry->value); // call the event handler

		/* Put the entry in the free list */
		entry->next=pic_queue.free_entry;
		pic_queue.free_entry=entry;
	}
	InEventService = false;

	/* Check when to set the new cycle end */
	if (pic_queue.next_entry) {
		auto cycles = static_cast<int32_t>(
		        pic_queue.next_entry->index * static_cast<double>(CPU_CycleMax) -
		        index_nd_f);
		if (GCC_UNLIKELY(!cycles))
			cycles = 1;
		if (cycles < CPU_CycleLeft) {
			CPU_Cycles = cycles;
		} else {
			CPU_Cycles=CPU_CycleLeft;
		}
	} else CPU_Cycles=CPU_CycleLeft;
	CPU_CycleLeft-=CPU_Cycles;
	if (PIC_IRQCheck) PIC_runIRQs();
	return true;
}

/* The TIMER Part */
struct TickerBlock {
	TIMER_TickHandler handler;
	TickerBlock * next;
};

static TickerBlock * firstticker=0;


void TIMER_DelTickHandler(TIMER_TickHandler handler) {
	TickerBlock * ticker=firstticker;
	TickerBlock * * tick_where=&firstticker;
	while (ticker) {
		if (ticker->handler==handler) {
			*tick_where=ticker->next;
			delete ticker;
			return;
		}
		tick_where=&ticker->next;
		ticker=ticker->next;
	}
}

void TIMER_AddTickHandler(TIMER_TickHandler handler) {
	TickerBlock * newticker=new TickerBlock;
	newticker->next=firstticker;
	newticker->handler=handler;
	firstticker=newticker;
}

void TIMER_AddTick(void) {
	/* Setup new amount of cycles for PIC */
	CPU_CycleLeft=CPU_CycleMax;
	CPU_Cycles=0;
	PIC_Ticks++;
	/* Go through the list of scheduled events and lower their index with 1000 */
	PICEntry * entry=pic_queue.next_entry;
	while (entry) {
		entry->index -= 1.0f;
		entry=entry->next;
	}
	/* Call our list of ticker handlers */
	TickerBlock * ticker=firstticker;
	while (ticker) {
		TickerBlock * nextticker=ticker->next;
		ticker->handler();
		ticker=nextticker;
	}
}

/* Use full name to avoid name clash with compile option for position-independent code */
class PIC_8259A final : public Module_base {
private:
	IO_ReadHandleObject ReadHandler[4];
	IO_WriteHandleObject WriteHandler[4];
public:
	PIC_8259A(Section* configuration):Module_base(configuration){
		/* Setup pic0 and pic1 with initial values like DOS has normally */
		PIC_IRQCheck = 0;
		PIC_Ticks = 0;
		Bitu i;
		for (i=0;i<2;i++) {
			pics[i].auto_eoi=false;
			pics[i].rotate_on_auto_eoi=false;
			pics[i].request_issr=false;
			pics[i].special=false;
			pics[i].single=false;
			pics[i].icw_index=0;
			pics[i].icw_words=0;
			pics[i].irr = pics[i].isr = pics[i].imrr = 0;
			pics[i].isrr = pics[i].imr = 0xff;
			pics[i].active_irq = 8;
		}
		primary_controller.vector_base = 0x08;
		secondary_controller.vector_base = 0x70;

		PIC_SetIRQMask(0,false);					/* Enable system timer */
		PIC_SetIRQMask(1,false);					/* Enable system timer */
		PIC_SetIRQMask(2,false);					/* Enable second pic */
		PIC_SetIRQMask(8,false);					/* Enable RTC IRQ */

		if (machine==MCH_PCJR) {
			/* Enable IRQ6 (replacement for the NMI for PCJr) */
			PIC_SetIRQMask(6,false);
		}
		ReadHandler[0].Install(0x20, read_command, io_width_t::byte);
		ReadHandler[1].Install(0x21, read_data, io_width_t::byte);
		WriteHandler[0].Install(0x20, write_command, io_width_t::byte);
		WriteHandler[1].Install(0x21, write_data, io_width_t::byte);
		ReadHandler[2].Install(0xa0, read_command, io_width_t::byte);
		ReadHandler[3].Install(0xa1, read_data, io_width_t::byte);
		WriteHandler[2].Install(0xa0, write_command, io_width_t::byte);
		WriteHandler[3].Install(0xa1, write_data, io_width_t::byte);
		/* Initialize the pic queue */
		for (i=0;i<PIC_QUEUESIZE-1;i++) {
			pic_queue.entries[i].next=&pic_queue.entries[i+1];
		}
		pic_queue.entries[PIC_QUEUESIZE-1].next=0;
		pic_queue.free_entry=&pic_queue.entries[0];
		pic_queue.next_entry=0;
	}

	~PIC_8259A(){
	}
};

static PIC_8259A* test;

void PIC_Destroy(Section* /*sec*/){
	delete test;
}

void PIC_Init(Section* sec) {
	test = new PIC_8259A(sec);
	sec->AddDestroyFunction(&PIC_Destroy);
}
