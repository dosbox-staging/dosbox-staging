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

#include <string.h>

#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "timer.h"
#include "math.h"
#include "regs.h"
#include "serialport.h"

#define SERIALBASERATE 115200
#define SERIALPORT_COUNT 2

#define LOG_UART LOG_MSG
CSerial *serialports[SERIALPORT_COUNT];


void CSerial::setdivisor(Bit8u dmsb, Bit8u dlsb) {
	Bitu divsize=(dmsb << 8) | dlsb;
	if (divsize!=0) {
		bps = SERIALBASERATE / divsize;
	}
}


void CSerial::checkint(void) {
	/* Find lowest priority interrupt to activate */
	Bitu i;
	for (i=0;i<INT_NONE;i++) {
		if (ints.requested & ints.enabled & (1 << i)) {
			PIC_ActivateIRQ(irq);
			ints.active=(INT_TYPES)i;
			return;
		}
	}
	/* Not a single interrupt schedulded, lower IRQ */
	PIC_DeActivateIRQ(irq);
	ints.active=INT_NONE;
}

void CSerial::raiseint(INT_TYPES type) {
//	LOG_MSG("Raising int %X rx size %d tx size %d",type,rx_fifo.used,tx_fifo.used);
	ints.requested|=1 << type;
	checkint();
}

void CSerial::lowerint(INT_TYPES type){
	ints.requested&=~(1 << type);
	checkint();
}


void CSerial::write_port(Bitu port, Bitu val) {

	port-=base;
//	LOG_UART("Serial write %X val %x %c",port,val,val);
	switch(port) {
	case 0x8:  // Transmit holding buffer + Divisor LSB
		if (dlab) {
			divisor_lsb = val;
			setdivisor(divisor_msb, divisor_lsb);
			return;
		}
		if (local_loopback) {
			rx_addb(val);
		} else tx_addb(val);
		break;
	case 0x9:  // Interrupt enable register + Divisor MSB
		if (dlab) {
			divisor_msb = val;
			setdivisor(divisor_msb, divisor_lsb);
			return;
		}
		/* Only enable the FIFO interrupt by default */
		ints.enabled=1 << INT_RX_FIFO;
		if (val & 0x1) ints.enabled|=1 << INT_RX;
		if (val & 0x2) ints.enabled|=1 << INT_TX;
		if (val & 0x4) ints.enabled|=1 << INT_LS;
		if (val & 0x8) ints.enabled|=1 << INT_MS;
		ierval = val;		
		checkint();
		break;
	case 0xa:  // FIFO Control register
		FIFOenabled = false;
		if (val & 0x1) {
//			FIFOenabled = true;
			timeout = 0;
		}
		if (val & 0x2) {		//Clear receiver FIFO
			rx_fifo.used=0;
			rx_fifo.pos=0;
		}
		if (val & 0x4) {		//Clear transmit FIFO
			tx_fifo.used=0;
			tx_fifo.pos=0;
		}
		if (val & 0x8) LOG(LOG_MISC,LOG_WARN)("UART:Enabled DMA mode");
		switch (val >> 6) {
		case 0:
			FIFOsize = 1;
			break;
		case 1:
			FIFOsize = 4;
			break;
		case 2:
			FIFOsize = 8;
			break;
		case 3:
			FIFOsize = 14;
			break;
		}
		break;
	case 0xb:  // Line control register
		linectrl = val;
		wordlen = (val & 0x3);
		dlab = (val & 0x80) > 0;
		break;
	case 0xc:  // Modem control register
		dtr  = val & 0x01;
		rts  = (val & 0x02) > 0 ;
		out1 = (val & 0x04) > 0;
		out2 = (val & 0x08) > 0;
		if (mc_handler) (*mc_handler)(val & 0xf);
		local_loopback = (val & 0x10) > 0;
		break;
	case 0xf:  // Scratch register
		scratch = val;
		break;
	default:
		LOG_UART("Modem: Write to 0x%x, with 0x%x '%c'\n", port,val,val);
		break;
	}
}

void CSerial::write_serial(Bitu port, Bitu val,Bitu iolen) {
	int i;

	for(i=0;i<SERIALPORT_COUNT;i++){
		if( (port>=serialports[i]->base+0x8) && (port<=(serialports[i]->base+0xf)) ) {
			serialports[i]->write_port(port,val);
		}
	}
}

Bitu CSerial::read_port(Bitu port) {
	Bit8u outval = 0;

	port-=base;
//	LOG_MSG("Serial read form %X",port);
	switch(port) {
	case 0x8:  //  Receive buffer + Divisor LSB
		if (dlab) {
			return divisor_lsb ;
		} else {
			outval = rx_readb();
//			LOG_UART("Read from %X %X %c remain %d",port,outval,outval,rx_fifo.used);		
			return outval;
		}
	case 0x9:  // Interrupt enable register + Divisor MSB
		if (dlab) {
			return divisor_msb ;
		} else {
//			LOG_UART("Read from %X %X",port,ierval);
			return ierval;
		}
	case 0xa:  // Interrupt identification register
		switch (ints.active) {
		case INT_MS:
			outval = 0x0;
			break;
		case INT_TX:
			outval = 0x2;
			lowerint(INT_TX);
			goto skipreset;
		case INT_RX:
			outval = 0x4;
			break;
		case INT_RX_FIFO:
			lowerint(INT_RX_FIFO);
			outval = 0xc;
			goto skipreset;
		case INT_LS:
			outval = 0x6;
			break;
		case INT_NONE:
			outval = 0x1;
			break;
		}
		ints.active=INT_NONE;
skipreset:	
		if (FIFOenabled) outval |= 3 << 6;
//		LOG_UART("Read from %X %X",port,outval);		
		return outval;
	case 0xb:  // Line control register
		LOG_UART("Read from %X %X",port,outval);
		return linectrl;
	case 0xC:  // Modem control register
		outval = dtr | (rts << 1)  | (out1 << 2) | (out2 << 3) | (local_loopback << 4);
//		LOG_UART("Read from %X %X",port,outval);
		return outval;
	case 0xD:  // Line status register
		lowerint(INT_LS);
		outval = 0x40;
		if (FIFOenabled) {
			if (!tx_fifo.used) outval|=0x20;
		} else if (tx_fifo.used<FIFO_SIZE) outval|=0x20;

		if (rx_fifo.used) outval|= 1;
//		LOG_UART("Read from %X %X",port,outval);
		return outval;
		break;
	case 0xE:  // modem status register 
		lowerint(INT_MS);
//		LOG_UART("Read from %X %X",port,outval);		
		outval=mstatus;
		mstatus&=0xf0;
		return outval;
	case 0xF:  // Scratch register
		return scratch;
	default:
		//LOG_DEBUG("Modem: Read from 0x%x\n", port);
		break;
	}

	return 0x00;

	

}

Bitu CSerial::read_serial(Bitu port,Bitu iolen)
{
	int i;
	for(i=0;i<SERIALPORT_COUNT;i++){
		if( (port>=serialports[i]->base+0x8) && (port<=(serialports[i]->base+0xf)) ) {
			return serialports[i]->read_port(port);
		}
	}
	return 0x00;	
}

Bitu CSerial::rx_free() {
	return FIFO_SIZE-rx_fifo.used;
}

Bitu CSerial::tx_free() {
	return FIFO_SIZE-tx_fifo.used;
}

Bitu CSerial::tx_size() {
	if (FIFOenabled && rx_fifo.used && (rx_lastread < (PIC_Ticks-2))) {
		raiseint(INT_RX_FIFO);
	}
	return tx_fifo.used;
}

Bitu CSerial::rx_size() {
	return rx_fifo.used;
}

void CSerial::rx_addb(Bit8u data) {
//	LOG_UART("RX add %c",data);
	if (rx_fifo.used<FIFO_SIZE) {
		Bitu where=rx_fifo.pos+rx_fifo.used;
		if (where>=FIFO_SIZE) where-=FIFO_SIZE;
		rx_fifo.data[where]=data;
		rx_fifo.used++;
		if (FIFOenabled && (rx_fifo.used < FIFOsize)) return;
		/* Raise rx irq if possible */
		if (ints.active != INT_RX) raiseint(INT_RX);
	} 
}

void CSerial::rx_adds(Bit8u * data,Bitu size) {
	if ((rx_fifo.used+size)<=FIFO_SIZE) {
		Bitu where=rx_fifo.pos+rx_fifo.used;
		rx_fifo.used+=size;
		while (size--) {
			if (where>=FIFO_SIZE) where-=FIFO_SIZE;
			rx_fifo.data[where++]=*data++;
		}
		if (FIFOenabled && (rx_fifo.used < FIFOsize)) return;
		if (ints.active != INT_RX) raiseint(INT_RX);
	}
//	else LOG_MSG("WTF");
}

void CSerial::tx_addb(Bit8u data) {
//	LOG_UART("TX add %c",data);
	if (tx_fifo.used<FIFO_SIZE) {
		Bitu where=tx_fifo.pos+tx_fifo.used;
		if (where>=FIFO_SIZE) where-=FIFO_SIZE;
		tx_fifo.data[where]=data;
		tx_fifo.used++;
		if (tx_fifo.used<(FIFO_SIZE-16)) {
			/* Only generate FIFO irq's every 16 bytes */
			if (FIFOenabled && (tx_fifo.used & 0xf)) return;
			raiseint(INT_TX);
		}
	} else {
//		LOG_MSG("tx addb");
	}
}

Bit8u CSerial::rx_readb() {
	if (rx_fifo.used) {
		rx_lastread=PIC_Ticks;
		Bit8u val=rx_fifo.data[rx_fifo.pos];
		rx_fifo.pos++;
		if (rx_fifo.pos>=FIFO_SIZE) rx_fifo.pos-=FIFO_SIZE;
		rx_fifo.used--;
		//Don't care for FIFO Size
		if (FIFOenabled || !rx_fifo.used) lowerint(INT_RX);
		else raiseint(INT_RX);
		return val;
	} else {
//		LOG_MSG("WTF rx readb");
		return 0;
	}
}

Bit8u CSerial::tx_readb() {
	if (tx_fifo.used) {
		Bit8u val=tx_fifo.data[tx_fifo.pos];
		tx_fifo.pos++;
		if (tx_fifo.pos>=FIFO_SIZE) tx_fifo.pos-=FIFO_SIZE;
		tx_fifo.used--;
		if (FIFOenabled && !tx_fifo.used) raiseint(INT_TX);
		return val;
	} else  {
//		LOG_MSG("WTF tx readb");
		return 0;
	}
}


void CSerial::setmodemstatus(Bit8u status) {
	Bitu oldstatus=mstatus >> 4;
	if(oldstatus ^ status ) {
		mstatus=status << 4;
		mstatus|=(oldstatus ^ status);
		raiseint(INT_MS);
	}
}

Bit8u CSerial::getmodemstatus() {
	return (mstatus >> 4);
}

Bit8u CSerial::getlinestatus() {
	return read_port(0xd);
}


void CSerial::SetMCHandler(MControl_Handler * mcontrol) {
	mc_handler=mcontrol;
}

CSerial::CSerial (Bit16u initbase, Bit8u initirq, Bit32u initbps) {

	int i;
	Bit16u initdiv;

	base=initbase;
	irq=initirq;
	bps=initbps;

	mc_handler = 0;
	tx_fifo.used = tx_fifo.pos = 0;
	rx_fifo.used = rx_fifo.pos = 0;

	rx_lastread = PIC_Ticks;
	linectrl = dtr = rts = out1 = out2 = 0;
	local_loopback = 0;
	ierval = 0;
	ints.enabled=1 << INT_RX_FIFO;
	ints.active=INT_NONE;
	ints.requested=0;

	FIFOenabled = false;	
	FIFOsize = 1;
	timeout = 0;
	dlab = 0;
	ierval = 0;

	initdiv = SERIALBASERATE / bps;
	setdivisor(initdiv >> 8, initdiv & 0x0f);

	for (i=8;i<=0xf;i++) {
		IO_RegisterWriteHandler(initbase+i,write_serial,IO_MB);
		IO_RegisterReadHandler(initbase+i,read_serial,IO_MB);
	}
	
	PIC_RegisterIRQ(irq,0,"SERIAL");


};

CSerial::~CSerial(void)
{

};



CSerial *getComport(Bitu portnum)
{
	return serialports[portnum-1];
}

void SERIAL_Init(Section* sec) {

	unsigned long args = 1;
	Section_prop * section=static_cast<Section_prop *>(sec);

//	if(!section->Get_bool("enabled")) return;

	serialports[0] = new CSerial(0x3f0,4,SERIALBASERATE);
	serialports[1] = new CSerial(0x2f0,3,SERIALBASERATE);
}

