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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: pic.cpp,v 1.25 2004-11-15 14:56:55 qbix79 Exp $ */

#include <list>

#include "dosbox.h"
#include "inout.h"
#include "cpu.h"
#include "pic.h"
#include "timer.h"

#define PIC_QUEUESIZE 128

struct IRQ_Block {
	bool masked;
	bool active;
	bool inservice;
	Bitu vector;
};

struct PIC_Controller {
	Bitu icw_words;
	Bitu icw_index;
	Bitu masked;
	Bitu active;
	Bitu inservice;

	bool special;
	bool auto_eoi;
	bool request_issr;
	Bit8u vector_base;
};

Bitu PIC_Ticks=0;
Bitu PIC_IRQCheck;
Bitu PIC_IRQActive;


static IRQ_Block irqs[16];
static PIC_Controller pics[2];

struct PICEntry {
	float index;
	Bitu value;
	PIC_EventHandler event;
	PICEntry * next;
};

static struct {
	PICEntry entries[PIC_QUEUESIZE];
	PICEntry * free_entry;
	PICEntry * next_entry;
} pic;

static void write_command(Bitu port,Bitu val,Bitu iolen) {
	PIC_Controller * pic=&pics[port==0x20 ? 0 : 1];
	Bitu irq_base=port==0x20 ? 0 : 8;
	Bitu i;
	Bit16u IRQ_priority_table[16] = 
	{ 0,1,2,9,10,11,12,13,14,15,3,4,5,6,7,8 };
	switch (val) {
	case 0x0A: /* select read interrupt request register */
		pic->request_issr=false;
		break;
	case 0x0B: /* select read interrupt in-service register */
		pic->request_issr=true;
		break;
	case 0x10:				/* ICW1 */
		pic->icw_index=1;
		pic->icw_words=2;
		break;
	case 0x11:				/* ICW1 + need for ICW4 */
		pic->icw_index=1;
		pic->icw_words=3;
		break;
	case 0x20:case 0x21:case 0x22:case 0x23:case 0x24:case 0x25:case 0x26:case 0x27:
		if (PIC_IRQActive<(irq_base+8)) {
			irqs[PIC_IRQActive].inservice=false;
			PIC_IRQActive=PIC_NOIRQ;
			for (i=0; i<=15; i++){
				if(irqs[IRQ_priority_table[i]].inservice) {
					PIC_IRQActive=IRQ_priority_table[i];
					break;
				}
			}
		} //TODO Warnings?
		break;
	case 0x4a:	/* OCW3 select read interrupt request register */
		LOG(LOG_PIC,LOG_NORMAL)("port %X : special OFF",port);
		pic->special = false;
		pic->request_issr = false;
		break;
	case 0x60:case 0x61:case 0x62:case 0x63:case 0x64:case 0x65:case 0x66:case 0x67:
		/* Spefific EOI 0-7 */
		if (PIC_IRQActive==(irq_base+val-0x60U)) {
			irqs[PIC_IRQActive].inservice=false;
			PIC_IRQActive=PIC_NOIRQ;
			for (i=0; i<=15; i++) {
				if (irqs[IRQ_priority_table[i]].inservice) {
					PIC_IRQActive=IRQ_priority_table[i];
					break;
				}
			}
		}//TODO Warnings?
		break;
	case 0x68:/* OCW3 select */
		pic->special=true;
		LOG(LOG_PIC,LOG_NORMAL)("port %X : special ON",port);
		break;
	case 0x6b:	/* OCW3 select read interrupt in-service register */
		LOG(LOG_PIC,LOG_NORMAL)("port %X : special ON",port);
		pic->special = true;
		pic->request_issr = true;
		break;
	case 0xC0:case 0xC1:case 0xC2:case 0xC3:case 0xC4:case 0xC5:case 0xC6:case 0xC7:
		/* Priority order, no need for it */
		break;
	case 0x00:case 0x80: /* Rotate stuff in eoi mode */
		/* We can live without it for now. (7 cities of gold) */
		LOG(LOG_PIC,LOG_NORMAL)("port %X : ignoring rotate stuff.",port);
		break;
	default:
		E_Exit("PIC:Unhandled command %02X",val);
	}
}

static void write_data(Bitu port,Bitu val,Bitu iolen) {
	PIC_Controller * pic=&pics[port==0x21 ? 0 : 1];
	Bitu irq_base=(port==0x21) ? 0 : 8;
	Bitu i;
	switch(pic->icw_index) {
	case 0:                        /* mask register */
		LOG(LOG_PIC,LOG_NORMAL)("%d mask %X",port==0x21 ? 0 : 1,val);
		for (i=0;i<=7;i++) {
			irqs[i+irq_base].masked=(val&(1<<i))>0;
			if (irqs[i+irq_base].active && !irqs[i+irq_base].masked) PIC_IRQCheck|=(1 << (i+irq_base));
			else PIC_IRQCheck&=~(1 << (i+irq_base));
		};
#if 0
		if (PIC_IRQCheck) {
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=0;
		}
#endif
		break;
	case 1:                        /* icw2          */
		LOG(LOG_PIC,LOG_NORMAL)("%d:Base vector %X",port==0x21 ? 0 : 1,val);
		for (i=0;i<=7;i++) {
			irqs[i+irq_base].vector=(val&0xf8)+i;
		};
		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		break;
	case 2:							/* icw 3 */
		LOG(LOG_PIC,LOG_NORMAL)("%d:ICW 3 %X",port==0x21 ? 0 : 1,val);
		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		break;
	case 3:							/* icw 4 */
		/*
			0	    1 8086/8080  0 mcs-8085 mode
			1	    1 Auto EOI   1 Normal EOI
			2-3	   0x Non buffer Mode 
				   10 Buffer Mode Slave 
				   11 Buffer mode Master	
			4		Special/Not Special nested mode 
		*/
		pic->auto_eoi=(val & 0x2)>0;
		
		LOG(LOG_PIC,LOG_NORMAL)("%d:ICW 4 %X",port==0x21 ? 0 : 1,val);
		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		break;
	default:                       /* icw 3, and 4*/
		LOG(LOG_PIC,LOG_NORMAL)("ICW HUH? %X",val);
	}
}


static Bitu read_command(Bitu port,Bitu iolen) {
	PIC_Controller * pic=&pics[port==0x20 ? 0 : 1];
	Bitu irq_base=(port==0x20) ? 0 : 8;
	Bitu i;Bit8u ret=0;Bit8u b=1;
	if (pic->request_issr) {
		for (i=irq_base;i<irq_base+8;i++) {
			if (irqs[i].inservice) ret|=b;
			b <<= 1;
		}
	} else {
		for (i=irq_base;i<irq_base+8;i++) {
			if (irqs[i].active)	ret|=b;
			b <<= 1;
		}
	}
	return ret;
}

static Bitu read_data(Bitu port,Bitu iolen) {
	PIC_Controller * pic=&pics[port==0x21 ? 0 : 1];
	Bitu irq_base=(port==0x21) ? 0 : 8;
	Bitu i;Bit8u ret=0;Bit8u b=1;
	for (i=irq_base;i<=irq_base+7;i++) {
		if (irqs[i].masked)	ret|=b;
		b <<= 1;
	}
	return ret;
}


void PIC_ActivateIRQ(Bitu irq) {
	if (irq<16) {
		irqs[irq].active=true;
		if (!irqs[irq].masked) {
			PIC_IRQCheck|=(1 << irq);
		}
	}
}

void PIC_DeActivateIRQ(Bitu irq) {
	if (irq<16) {
		irqs[irq].active=false;
		PIC_IRQCheck&=~(1 << irq);
	}
}

void PIC_runIRQs(void) {
	Bitu i;
	if (!GETFLAG(IF)) return;
	if (!PIC_IRQCheck) return;
	Bit16u IRQ_priority_lookup[17] = 
		{ 0,1,2,11,12,13,14,15,3,4,5,6,7,8,9,10,16 };
	Bit16u activeIRQ = PIC_IRQActive;
	if (activeIRQ==PIC_NOIRQ) activeIRQ = 16;
	for (i=0;i<=15;i++) {
	   if ((IRQ_priority_lookup[i]<IRQ_priority_lookup[activeIRQ]) || (pics[ (i<8)?0:1].special) ){
		  	if (!irqs[i].masked && irqs[i].active) {
				irqs[i].active=false;
				PIC_IRQCheck&=~(1 << i);
				if (i==2) i=9;
				CPU_HW_Interrupt(irqs[i].vector);
				if (!pics[0].auto_eoi) {
					PIC_IRQActive=i;
					irqs[i].inservice=true;
				}
				return;
			}
		}
	}
}

void PIC_SetIRQMask(Bitu irq, bool masked) {
	irqs[irq].masked=masked;
	if (irqs[irq].active && !irqs[irq].masked) { 
		PIC_IRQCheck|=(1 << (irq));
	} else { 
		PIC_IRQCheck&=~(1 << (irq));
	};
	if (PIC_IRQCheck) {
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=0;
	}
}

static void AddEntry(PICEntry * entry) {
	PICEntry * find_entry=pic.next_entry;
	if (!find_entry) {
		entry->next=0;
		pic.next_entry=entry;
	} else if (find_entry->index>entry->index) {
		pic.next_entry=entry;
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
	Bits cycles=PIC_MakeCycles(pic.next_entry->index-PIC_TickIndex());
	if (cycles<CPU_Cycles) {
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=0;
	}
}

void PIC_AddEvent(PIC_EventHandler handler,float delay,Bitu val) {
	if (!pic.free_entry) {
		LOG(LOG_PIC,LOG_ERROR)("Event queue full");
		return;
	}
	PICEntry * entry=pic.free_entry;
	entry->index=delay+PIC_TickIndex();;
	entry->event=handler;
	entry->value=val;
	pic.free_entry=pic.free_entry->next;
	AddEntry(entry);
}


void PIC_RemoveEvents(PIC_EventHandler handler) {
	PICEntry * entry=pic.next_entry;
	PICEntry * prev_entry;
	prev_entry=0;
	while (entry) {
		if (entry->event==handler) {
			if (prev_entry) {
				prev_entry->next=entry->next;
				entry->next=pic.free_entry;
				pic.free_entry=entry;
				entry=prev_entry->next;
				continue;
			} else {
				pic.next_entry=entry->next;
				entry->next=pic.free_entry;
				pic.free_entry=entry;
				entry=pic.next_entry;
				continue;
			}
		}
		prev_entry=entry;
		entry=entry->next;
	}	
}


bool PIC_RunQueue(void) {
	/* Check to see if a new milisecond needs to be started */
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=0;
	if (CPU_CycleLeft<=0) {
		return false;
	}
	/* Check the queue for an entry */
	double index=PIC_TickIndex();
	while (pic.next_entry && pic.next_entry->index<=index) {
		PICEntry * entry=pic.next_entry;
		pic.next_entry=entry->next;
		(entry->event)(entry->value);
		/* Put the entry in the free list */
		entry->next=pic.free_entry;
		pic.free_entry=entry;
	}
	/* Check when to set the new cycle end */
	if (pic.next_entry) {
		Bits cycles=PIC_MakeCycles(pic.next_entry->index-index);
		if (!cycles) cycles=1;
		if (cycles<CPU_CycleLeft) {
			CPU_Cycles=cycles;
		} else {
			CPU_Cycles=CPU_CycleLeft;
		}
	} else CPU_Cycles=CPU_CycleLeft;
	CPU_CycleLeft-=CPU_Cycles;
	if 	(PIC_IRQCheck)	PIC_runIRQs();
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
	TickerBlock * * where=&firstticker;
	while (ticker) {
		if (ticker->handler==handler) {
			*where=ticker->next;
			delete ticker;
			return;
		}
		where=&ticker->next;
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
	PICEntry * entry=pic.next_entry;
	while (entry) {
		if (entry->index>=1) entry->index-=1;
		else entry->index=0;
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



void PIC_Init(Section* sec) {
	/* Setup pic0 and pic1 with initial values like DOS has normally */
	PIC_IRQCheck=0;
	PIC_IRQActive=PIC_NOIRQ;
	PIC_Ticks=0;
	Bitu i;
	for (i=0;i<2;i++) {
		pics[i].masked=0xff;
		pics[i].active=0;
		pics[i].inservice=0;
		pics[i].auto_eoi=false;
		pics[i].auto_eoi=false;
		pics[i].request_issr=false;
		pics[i].special=false;
		pics[i].icw_index=0;
		pics[i].icw_words=0;
	}
	for (i=0;i<=7;i++) {
		irqs[i].active=false;
		irqs[i].masked=true;
		irqs[i].inservice=false;
		irqs[i+8].active=false;
		irqs[i+8].masked=true;
		irqs[i+8].inservice=false;
		irqs[i].vector=0x8+i;
		irqs[i+8].vector=0x70+i;	
	}
	irqs[0].masked=false;					/* Enable system timer */
	irqs[1].masked=false;					/* Enable Keyboard IRQ */
	irqs[8].masked=false;					/* Enable RTC IRQ */
/*	irqs[12].masked=false;	moved to mouse.cpp */				/* Enable Mouse IRQ */
	IO_RegisterReadHandler(0x20,read_command,IO_MB);
	IO_RegisterReadHandler(0x21,read_data,IO_MB);
	IO_RegisterWriteHandler(0x20,write_command,IO_MB);
	IO_RegisterWriteHandler(0x21,write_data,IO_MB);
	IO_RegisterReadHandler(0xa0,read_command,IO_MB);
	IO_RegisterReadHandler(0xa1,read_data,IO_MB);
	IO_RegisterWriteHandler(0xa0,write_command,IO_MB);
	IO_RegisterWriteHandler(0xa1,write_data,IO_MB);
	/* Initialize the pic queue */
	for (i=0;i<PIC_QUEUESIZE-1;i++) {
		pic.entries[i].next=&pic.entries[i+1];
	}
	pic.entries[PIC_QUEUESIZE-1].next=0;
	pic.free_entry=&pic.entries[0];
	pic.next_entry=0;

}
		

