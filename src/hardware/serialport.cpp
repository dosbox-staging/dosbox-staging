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

#include <string.h>
#include <list>

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
#define LOG_UART LOG_MSG

CSerialList seriallist;

void CSerial::UpdateBaudrate(void) {
	Bitu divsize=(divisor_msb << 8) | divisor_lsb;
	if (divsize) {
		bps = SERIALBASERATE / divsize;
	}
}


void CSerial::Timer(void) {
	dotxint=true;
	checkint();
}

void CSerial::checkint(void) {
	/* Check which irq to activate */
	//TODO Check for line status changes, but we can't get errors anyway
	//Since we only check once every millisecond, we just ignore the fifosize parameter
	if ((ier & 0x1) && rqueue->inuse()) {
//		LOG_MSG("RX IRQ with %d",rqueue->inuse());
		iir=0x4;
	} else if ((ier & 0x2) && !tqueue->inuse() && dotxint) {
		iir=0x2;
	} else if ((ier & 0x8) && (mstatus & 0x0f)) {
		iir=0x0;
	} else {
		iir=0x1;
		PIC_DeActivateIRQ(irq);
			return;
		}
	if (mctrl & 0x8) PIC_ActivateIRQ(irq);
	else PIC_DeActivateIRQ(irq);
}

void CSerial::write_reg(Bitu reg, Bitu val) {
//	if (port!=9 && port!=8) LOG_MSG("Serial write %X val %x %c",port,val,val);
	switch(reg) {
	case 0x8:  // Transmit holding buffer + Divisor LSB
		if (dlab) {
			divisor_lsb = val;
			UpdateBaudrate();
			return;
		}
		if (local_loopback) rqueue->addb(val);
		else tqueue->addb(val);
		break;
	case 0x9:  // Interrupt enable register + Divisor MSB
		if (dlab) {
			divisor_msb = val;
			UpdateBaudrate();
			return;
		} else {
            ier = val;
			dotxint=true;
		}
		break;
	case 0xa:  // FIFO Control register
		FIFOenabled = (val & 0x1) > 0;
		if (val & 0x2) rqueue->clear(); //Clear receiver FIFO
		if (val & 0x4) tqueue->clear();	//Clear transmit FIFO
		if (val & 0x8) LOG(LOG_MISC,LOG_WARN)("UART:Enabled DMA mode");
		switch (val >> 6) {
		case 0:FIFOsize = 1;break;
		case 1:FIFOsize = 4;break;
		case 2:FIFOsize = 8;break;
		case 3:FIFOsize = 14;break;
		}
		break;
	case 0xb:  // Line control register
		linectrl = val;
		dlab = (val & 0x80);
		break;
	case 0xc:  // Modem control register
		mctrl=val;
		local_loopback = (val & 0x10);
		break;
	case 0xf:  // Scratch register
		scratch = val;
		break;
	default:
		LOG_UART("Modem: Write to 0x%x, with 0x%x '%c'\n", reg,val,val);
		break;
	}
}

static void WriteSerial(Bitu port,Bitu val,Bitu iolen) {
	Bitu check=port&~0xf;
	for (CSerial_it it=seriallist.begin();it!=seriallist.end();it++){
		CSerial * serial=(*it);
		if (check==serial->base) {
			serial->write_reg(port&0xf,val);
			return;
		}
	}
}

static Bitu ReadSerial(Bitu port,Bitu iolen) {
	Bitu check=port&~0xf;
	
	for (CSerial_it it=seriallist.begin();it!=seriallist.end();it++){
		CSerial * serial=(*it);
		if (check==serial->base) {

				return serial->read_reg(port&0xf);
		}
	}
	return 0;
}

void SERIAL_Update(void) {
	for (CSerial_it it=seriallist.begin();it!=seriallist.end();it++){
		CSerial * serial=(*it);
		serial->Timer();
	}
}



Bitu CSerial::read_reg(Bitu reg) {
	Bitu retval;
//	if (port!=0xd && port!=0xa) LOG_MSG("REad from port %x",reg);
	switch(reg) {
	case 0x8:  //  Receive buffer + Divisor LSB
		if (dlab) {
			return divisor_lsb ;
		} else {
			retval=rqueue->getb();
			//LOG_MSG("Received char %x %c",retval,retval);
			checkint();
			return retval;
		}
	case 0x9:  // Interrupt enable register + Divisor MSB
		if (dlab) {
			return divisor_msb ;
		} else {
			return ier;
		}
	case 0xa:  // Interrupt identification register
		retval=iir;	
		if (iir==2) {
			dotxint=false;
			iir=1;
		}
//		LOG_MSG("Read iir %d after %d",retval,iir);
		return retval | ((FIFOenabled) ? (3 << 6) : 0);
	case 0xb:  // Line control register
		return linectrl;
	case 0xC:  // Modem control register
		return mctrl;
	case 0xD:  // Line status register
		retval = 0x40;
		if (!tqueue->inuse()) retval|=0x20;
		if (rqueue->inuse()) retval|= 1;
//		LOG_MSG("Read from line status %x",retval);
		return retval;
	case 0xE:  // modem status register 
		retval=mstatus;
		mstatus&=0xf0;
		checkint();
		return retval;
	case 0xF:  // Scratch register
		return scratch;
	default:
		//LOG_DEBUG("Modem: Read from 0x%x\n", port);
		break;
	}
	return 0x00;	
}


void CSerial::SetModemStatus(Bit8u status) {
	status&=0xf;
	Bit8u oldstatus=mstatus >> 4;
	if (oldstatus ^ status ) {
		mstatus=(mstatus & 0xf) | status << 4;
		mstatus|=(oldstatus ^ status) & ((status & 0x4) | (0xf-0x4));
	}
}


CSerial::CSerial (Bit16u initbase, Bit8u initirq, Bit32u initbps) {

	Bitu i;
	Bit16u initdiv;

	base=initbase;
	irq=initirq;
	bps=initbps;

	local_loopback = 0;
	ier = 0;
	iir = 1;

	FIFOenabled = false;	
	FIFOsize = 1;
	dlab = 0;
	mstatus = 0;

	initdiv = SERIALBASERATE / bps;
	UpdateBaudrate();

	for (i=0;i<=8;i++) {
		WriteHandler[i].Install(base+i+8,WriteSerial,IO_MB);
		ReadHandler[i].Install(base+i+8,ReadSerial,IO_MB);
	}
	
	rqueue=new CFifo(QUEUE_SIZE);
	tqueue=new CFifo(QUEUE_SIZE);
};

CSerial::~CSerial(void)
{
	//Remove pointer in list if present
	for (CSerial_it it=seriallist.begin();it!=seriallist.end();)
		if(this==*it) it=seriallist.erase(it); else it++;
	
	delete rqueue;
	delete tqueue;
	
};

class SERIALPORTS:public Module_base{
public:
	SERIALPORTS(Section* configuration):Module_base(configuration){
		TIMER_AddTickHandler(&SERIAL_Update);
	}
	~SERIALPORTS(){
		TIMER_DelTickHandler(&SERIAL_Update);
	}
};

static SERIALPORTS* test;

void SERIAL_Destroy(Section* sec){
	delete test;
}

void SERIAL_Init(Section* sec) {
	test = new SERIALPORTS(sec);
	sec->AddDestroyFunction(&SERIAL_Destroy,false);	
}
