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

struct IRQ_Block {
	bool masked;
	bool active;
	bool inservice;
	Bit8u vector;
	char * name;
	PIC_EOIHandler * handler;
};

Bitu PIC_IRQCheck;
Bitu PIC_IRQActive;
bool PIC_IRQAgain;

static IRQ_Block irqs[16];
static Bit8u pic0_icws=0;
static Bit8u pic1_icws=0;
static Bit8u pic0_icw_state=0;
static Bit8u pic1_icw_state=0;
static bool pic0_request_iisr=0;
static bool pic1_request_iisr=0;
//TODO Special mask mode in ocw3
//TODO maybe check for illegal modes that don't work on pc too and exit the emu

//Pic 0 command port
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
			if (!irqs[8].masked) LOG_DEBUG("Someone unmasked RTC irq");
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
	Bit32u i;
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
				PIC_IRQAgain=true;
				return;
			}
		}
	}
}


void PIC_Init(void) {
	/* Setup pic0 and pic1 with initial values like DOS has normally */
	PIC_IRQCheck=0;
	PIC_IRQActive=PIC_NOIRQ;
	Bit8u i;
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
}
		

