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


#include "dosbox.h"
#include "inout.h"
#include "cpu.h"
#include "pic.h"

#define PIC_QUEUESIZE 128

struct IRQ_Block {
	bool masked;
	bool active;
	bool inservice;
	Bit8u vector;
	char * name;
	PIC_EOIHandler * handler;
};

Bitu PIC_Ticks=0;

Bitu PIC_IRQCheck;
Bitu PIC_IRQActive;

static IRQ_Block irqs[16];
static Bit8u pic0_icws=0;
static Bit8u pic1_icws=0;
static Bit8u pic0_icw_state=0;
static Bit8u pic1_icw_state=0;
static bool pic0_request_iisr=0;
static bool pic1_request_iisr=0;

enum QUEUE_TYPE { 
	IRQ,EVENT
};




struct PICEntry {
	QUEUE_TYPE type;
	Bitu irq;
	Bitu index;
	PIC_EventHandler event;
	PICEntry * next;
};


static struct {
	PICEntry entries[PIC_QUEUESIZE];
	PICEntry * free_entry;
	PICEntry * next_entry;
} pic;

static void write_p20(Bit32u port,Bit8u val) {
	switch (val) {
	case 0x0A: /* select read interrupt request register */
		pic0_request_iisr=false;
		break;
	case 0x0B: /* select read interrupt in-service register */
		pic0_request_iisr=true;
		break;
	case 0x10:				/* ICW1 */
		pic0_icws=2;
		pic0_icw_state=1;
		break;
	case 0x11:				/* ICW1 + need for ICW4 */
		pic0_icws=3;
		pic0_icw_state=1;
		break;
	case 0x20: /* end of interrupt command */
	case 0x21: /* end of interrupt command */
	case 0x22: /* end of interrupt command */
	case 0x23: /* end of interrupt command */
	case 0x24: /* end of interrupt command */
	case 0x25: /* end of interrupt command */
	case 0x26: /* end of interrupt command */
	case 0x27: /* end of interrupt command */
		/* clear highest current in service bit */
		if (PIC_IRQActive<8) {
			irqs[PIC_IRQActive].inservice=false;
			if (irqs[PIC_IRQActive].handler!=0) irqs[PIC_IRQActive].handler();
			PIC_IRQActive=PIC_NOIRQ;
		}
		break;
	case 0x60: /* specific EOI 0 */
	case 0x61: /* specific EOI 1 */
	case 0x62: /* specific EOI 2 */
	case 0x63: /* specific EOI 3 */
	case 0x64: /* specific EOI 4 */
	case 0x65: /* specific EOI 5 */
	case 0x66: /* specific EOI 6 */
	case 0x67: /* specific EOI 7 */
		if (PIC_IRQActive==(val-0x60U)) {
			irqs[PIC_IRQActive].inservice=false;
			if (irqs[PIC_IRQActive].handler!=0) irqs[PIC_IRQActive].handler();
			PIC_IRQActive=PIC_NOIRQ;
		}
		break;
	// IRQ lowest priority commands
	case 0xC0: // 0 7 6 5 4 3 2 1
	case 0xC1: // 1 0 7 6 5 4 3 2
	case 0xC2: // 2 1 0 7 6 5 4 3
	case 0xC3: // 3 2 1 0 7 6 5 4
	case 0xC4: // 4 3 2 1 0 7 6 5
	case 0xC5: // 5 4 3 2 1 0 7 6
	case 0xC6: // 6 5 4 3 2 1 0 7
	case 0xC7: // 7 6 5 4 3 2 1 0
	// ignore for now TODO
	break;
	default:
		E_Exit("PIC0:Unhandled command %02X",val);
	}
}

//Pic 0 Interrupt mask
static void write_p21(Bit32u port,Bit8u val) {
	Bit8u i;
	switch(pic0_icw_state) {
	case 0:                        /* mask register */
		for (i=0;i<=7;i++) {
			irqs[i].masked=(val&(1<<i))>0;
			if (irqs[i].active && !irqs[i].masked) PIC_IRQCheck|=(1 << 1);
			else PIC_IRQCheck&=~(1 << i);
		};
		break;
	case 1:                        /* icw2          */
		LOG_DEBUG("PIC0:Base vector %X",val);
		for (i=0;i<=7;i++) {
			irqs[i].vector=(val&0xf8)+i;
		};
	default:                       /* icw2, 3, and 4*/
		if(pic0_icw_state++ >= pic0_icws) pic0_icw_state=0; 	
	}
}

static Bit8u read_p20(Bit32u port) {
	Bit8u ret=0;
	Bit32u i;
	Bit8u b=1;
	if (pic0_request_iisr) {
		for (i=0;i<=7;i++) {
			if (irqs[i].inservice) ret|=b;
			b <<= 1;
		}
	} else {
		for (i=0;i<=7;i++) {
			if (irqs[i].active)	ret|=b;
			b <<= 1;
		}
	};
	return ret;
}

static Bit8u read_p21(Bit32u port) {
	Bit8u ret=0;
	Bit32u i;
	Bit8u b=1;
	for (i=0;i<=7;i++) {
		if (irqs[i].masked)	ret|=b;
		b <<= 1;
	}
	return ret;
}

static void write_pa0(Bit32u port,Bit8u val) {
	Bit32u i;
	switch (val) {
	case 0x0A: /* select read interrupt request register */
		pic1_request_iisr=false;
		break;
	case 0x0B: /* select read interrupt in-service register */
		pic1_request_iisr=true;
		break;
	case 0x10:				/* ICW1 */
		/* Clear everything set full mask and clear all inservice */
		for (i=0;i<=7;i++) {
			irqs[i].masked=true;
			irqs[i].active=false;
			irqs[i].inservice=false;
		}
		pic1_icws=2;
		pic1_icw_state=1;
		break;
	case 0x11:				/* ICW1 + need for ICW4 */
		pic1_icws=3;
		pic1_icw_state=1;
		break;
	case 0x20: /* end of interrupt command */
	case 0x21: /* end of interrupt command */
	case 0x22: /* end of interrupt command */
	case 0x23: /* end of interrupt command */
	case 0x24: /* end of interrupt command */
	case 0x25: /* end of interrupt command */
	case 0x26: /* end of interrupt command */
	case 0x27: /* end of interrupt command */
          /* clear highest current in service bit */
		if (PIC_IRQActive>7 && PIC_IRQActive <16) {
			irqs[PIC_IRQActive].inservice=false;
			if (irqs[PIC_IRQActive].handler!=0) irqs[PIC_IRQActive].handler();
			PIC_IRQActive=PIC_NOIRQ;
		}
		break;
	case 0x60: /* specific EOI 0 */
	case 0x61: /* specific EOI 1 */
	case 0x62: /* specific EOI 2 */
	case 0x63: /* specific EOI 3 */
	case 0x64: /* specific EOI 4 */
	case 0x65: /* specific EOI 5 */
	case 0x66: /* specific EOI 6 */
	case 0x67: /* specific EOI 7 */
		if (PIC_IRQActive==(8+val-0x60U)) {
			irqs[PIC_IRQActive].inservice=false;
			if (irqs[PIC_IRQActive].handler!=0) irqs[PIC_IRQActive].handler();
			PIC_IRQActive=PIC_NOIRQ;
		};
		break;
	// IRQ lowest priority commands
	case 0xC0: // 0 7 6 5 4 3 2 1
	case 0xC1: // 1 0 7 6 5 4 3 2
	case 0xC2: // 2 1 0 7 6 5 4 3
	case 0xC3: // 3 2 1 0 7 6 5 4
	case 0xC4: // 4 3 2 1 0 7 6 5
	case 0xC5: // 5 4 3 2 1 0 7 6
	case 0xC6: // 6 5 4 3 2 1 0 7
	case 0xC7: // 7 6 5 4 3 2 1 0
	//TODO Maybe does it even matter?
	break;
	default:
		E_Exit("Unhandled command %04X sent to port A0",val);
	}
}


static void write_pa1(Bit32u port,Bit8u val) {
	Bit8u i;
	switch(pic1_icw_state) {
	case 0:                        /* mask register */
		for (i=0;i<=7;i++) {
			irqs[i+8].masked=(val&1 <<i)>0;
		};
		break;
     case 1:                        /* icw2          */
		for (i=0;i<=7;i++) {
			irqs[i+8].vector=(val&0xf8)+i;
		};
     default:                       /* icw2, 3, and 4*/
       if(pic1_icw_state++ >= pic1_icws) pic1_icw_state=0; 
	}
}

static Bit8u read_pa0(Bit32u port) {
	Bit8u ret=0;
	Bit32u i;
	Bit8u b=1;
	if (pic1_request_iisr) {
		for (i=0;i<=7;i++) {
			if (irqs[i+8].inservice) ret|=b;
			b <<= 1;
		}
	} else {
		for (i=0;i<=7;i++) {
			if (irqs[i+8].active) ret|=b;
			b <<= 1;
		}
	}
	return ret;
}


static Bit8u read_pa1(Bit32u port) {
	Bit8u ret=0;
	Bit32u i;
	Bit8u b=1;
	for (i=0;i<=7;i++) {
		if (irqs[i+8].masked) ret|=b;
		b <<= 1;
	}
	return ret;
}

void PIC_RegisterIRQ(Bit32u irq,PIC_EOIHandler handler,char * name) {
	if (irq>15) E_Exit("PIC:Illegal IRQ");
	irqs[irq].name=name;
	irqs[irq].handler=handler;
}

void PIC_FreeIRQ(Bit32u irq) {
	if (irq>15) E_Exit("PIC:Illegal IRQ");
	irqs[irq].name=0;
	irqs[irq].handler=0;
	irqs[irq].active=0;
	irqs[irq].inservice=0;
	PIC_IRQCheck&=~(1 << irq);
}

void PIC_ActivateIRQ(Bit32u irq) {
	if (irq<16) {
		irqs[irq].active=true;
		if (!irqs[irq].masked) {
			PIC_IRQCheck|=(1 << irq);
		}
	}
}

void PIC_DeActivateIRQ(Bit32u irq) {
	if (irq<16) {
		irqs[irq].active=false;
		PIC_IRQCheck&=~(1 << irq);
	}
}

void PIC_runIRQs(void) {
	Bitu i;
	if (!flags.intf) return;
	if (PIC_IRQActive!=PIC_NOIRQ) return;
	if (!PIC_IRQCheck) return;
	for (i=0;i<=15;i++) {
		if (i!=2) {
			if (!irqs[i].masked && irqs[i].active) {
				irqs[i].inservice=true;
				irqs[i].active=false;
				PIC_IRQCheck&=~(1 << i);
				Interrupt(irqs[i].vector);
				PIC_IRQActive=i;
				return;
			}
		}
	}
}

static void AddEntry(PICEntry * entry) {
	PICEntry * find_entry=pic.next_entry;
	if (!find_entry) {
		entry->next=0;
		pic.next_entry=entry;
		return;
	}
	if (find_entry->index>entry->index) {
		pic.next_entry=entry;
		entry->next=find_entry;
		return;
	}
	while (find_entry) {
		if (find_entry->next) {
			/* See if the next index comes later than this one */
			if (find_entry->next->index>entry->index) {
				entry->next=find_entry->next;
				find_entry->next=entry;
				return;
			} else {
				find_entry=find_entry->next;
			}
		} else {
			entry->next=find_entry->next;
			find_entry->next=entry;
			return;
		}
	}
}

void PIC_AddEvent(PIC_EventHandler handler,Bitu delay) {
	if (!pic.free_entry) {
		LOG_WARN("PIC:No free queue entries");
		return;
	}
	PICEntry * entry=pic.free_entry;
	Bitu index=delay+PIC_Index();
	entry->index=index;
	entry->event=handler;
	entry->type=EVENT;
	pic.free_entry=pic.free_entry->next;
	AddEntry(entry);
}

void PIC_AddIRQ(Bitu irq,Bitu delay) {
	if (irq>15) E_Exit("PIC:Illegal IRQ");
	if (!pic.free_entry) {
		LOG_WARN("PIC:No free queue entries");
		return;
	}
	PICEntry * entry=pic.free_entry;
	Bitu index=delay+PIC_Index();
	entry->index=index;
	entry->irq=irq;
	entry->type=IRQ;
	pic.free_entry=pic.free_entry->next;
	AddEntry(entry);
}


void PIC_RemoveEvents(PIC_EventHandler handler) {
	PICEntry * entry=pic.next_entry;
	PICEntry * prev_entry;
	prev_entry=0;
	while (entry) {
		switch (entry->type) {
		case EVENT:
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
			break;
		}
		prev_entry=entry;
		entry=entry->next;
	}
}

Bitu PIC_RunQueue(void) {
	Bitu ret;
	/* Check to see if a new milisecond needs to be started */
	if (CPU_Cycles>0) {
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=0;
	}
	while (CPU_CycleLeft>0) {
		/* Check the queue for an entry */
		Bitu index=PIC_Index();
		while (pic.next_entry && pic.next_entry->index<=index) {
			PICEntry * entry=pic.next_entry;
			pic.next_entry=entry->next;
			switch (entry->type) {
			case EVENT:
				(entry->event)();
				break;
			case IRQ:
				PIC_ActivateIRQ(entry->irq);
				break;
			}
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
		if 	(PIC_IRQCheck)	PIC_runIRQs();
		/* Run the actual cpu core */
		CPU_CycleLeft-=CPU_Cycles;
		ret=(*cpudecoder)();
		if (CPU_Cycles>0) {
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=0;
		}
		if (ret) return ret;	
	}
	/* Prepare everything for next round */
	CPU_CycleLeft=CPU_CycleMax;
	PIC_Ticks++;
	/* Go through the list of scheduled irq's and lower their index with 1000 */
	PICEntry * entry=pic.next_entry;
	while (entry) {
		if (entry->index>1000) entry->index-=1000;
		else entry->index=0;
		entry=entry->next;
	}
	return 0;
}

void PIC_Init(Section* sec) {
	/* Setup pic0 and pic1 with initial values like DOS has normally */
	PIC_IRQCheck=0;
	PIC_IRQActive=PIC_NOIRQ;
	PIC_Ticks=0;
	Bitu i;
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
	irqs[12].masked=false;					/* Enable Mouse IRQ */
	IO_RegisterReadHandler(0x20,read_p20,"Master PIC Command");
	IO_RegisterReadHandler(0x21,read_p21,"Master PIC Data");
	IO_RegisterWriteHandler(0x20,write_p20,"Master PIC Command");
	IO_RegisterWriteHandler(0x21,write_p21,"Master PIC Data");
	IO_RegisterReadHandler(0xa0,read_pa0,"Slave PIC Command");
	IO_RegisterReadHandler(0xa1,read_pa1,"Slave PIC Data");
	IO_RegisterWriteHandler(0xa0,write_pa0,"Slave PIC Command");
	IO_RegisterWriteHandler(0xa1,write_pa1,"Slave PIC Data");
	/* Initialize the pic queue */
	for (i=0;i<PIC_QUEUESIZE-1;i++) {
		pic.entries[i].next=&pic.entries[i+1];
	}
	pic.entries[PIC_QUEUESIZE-1].next=0;
	pic.free_entry=&pic.entries[0];
	pic.next_entry=0;

}
		

