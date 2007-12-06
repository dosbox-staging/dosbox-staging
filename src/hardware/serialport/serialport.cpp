/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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

/* $Id: serialport.cpp,v 1.8 2007-12-06 17:44:19 qbix79 Exp $ */
#include <string.h>
#include <ctype.h>

#include "dosbox.h"

#include "inout.h"
#include "pic.h"
#include "setup.h"
#include "bios.h"					// SetComPorts(..)
#include "callback.h"				// CALLBACK_Idle

#include "serialport.h"
#include "directserial_win32.h"
#include "directserial_posix.h"
#include "directserial_os2.h"
#include "serialdummy.h"
#include "softmodem.h"
#include "nullmodem.h"

#include "cpu.h"


bool device_COM::Read(Bit8u * data,Bit16u * size) {
	// DTR + RTS on
	sclass->Write_MCR(0x03);
	for (Bit16u i=0; i<*size; i++)
	{
		Bit8u status;
		if(!(sclass->Getchar(&data[i],&status,true,1000))) {
			*size=i;
			return true;
		}
	}
	return true;
}


bool device_COM::Write(Bit8u * data,Bit16u * size) {
	// DTR + RTS on
	sclass->Write_MCR(0x03);
	for (Bit16u i=0; i<*size; i++)
	{
		if(!(sclass->Putchar(data[i],true,true,1000))) {
			*size=i;
			sclass->Write_MCR(0x01);
			return false;
		}
	}
	// RTS off
	sclass->Write_MCR(0x01);
	return true;
}

bool device_COM::Seek(Bit32u * pos,Bit32u type) {
	*pos = 0;
	return true;
}

bool device_COM::Close() {
	return false;
}

Bit16u device_COM::GetInformation(void) {
	return 0x80A0;
};

device_COM::device_COM(class CSerial* sc) {
	sclass = sc;
	SetName(serial_comname[sclass->idnumber]);
}

device_COM::~device_COM() {
}



// COM1 - COM4 objects
CSerial* serialports[4] ={0,0,0,0};

static Bitu SERIAL_Read (Bitu port, Bitu iolen) {
	for(Bitu i = 0; i < 4; i++) {
		if(serial_baseaddr[i]==(port&0xfff8) && (serialports[i]!=0)) {
			Bitu retval=0xff;
			switch (port & 0x7) {
				case RHR_OFFSET:
					retval = serialports[i]->Read_RHR();
					break;
				case IER_OFFSET:
					retval = serialports[i]->Read_IER();
					break;
				case ISR_OFFSET:
					retval = serialports[i]->Read_ISR();
					break;
				case LCR_OFFSET:
					retval = serialports[i]->Read_LCR();
					break;
				case MCR_OFFSET:
					retval = serialports[i]->Read_MCR();
					break;
				case LSR_OFFSET:
					retval = serialports[i]->Read_LSR();
					break;
				case MSR_OFFSET:
					retval = serialports[i]->Read_MSR();
					break;
				case SPR_OFFSET:
					retval = serialports[i]->Read_SPR();
					break;
			}

#if SERIAL_DEBUG
			const char* const dbgtext[]=
				{"RHR","IER","ISR","LCR","MCR","LSR","MSR","SPR"};
			if(serialports[i]->dbg_register)
				fprintf(serialports[i]->debugfp,"%12.3f read 0x%x from %s.\r\n",
					PIC_FullIndex(),retval,dbgtext[port&0x7]);
#endif

			return retval;	
		}
	}
	return 0xff;
}
static void SERIAL_Write (Bitu port, Bitu val, Bitu) {
	
	for(Bitu i = 0; i < 4; i++) {
		if(serial_baseaddr[i]==(port&0xfff8) && serialports[i]) {

#if SERIAL_DEBUG
		const char* const dbgtext[]={"THR","IER","FCR","LCR","MCR","!LSR","MSR","SPR"};
		if(serialports[i]->dbg_register)
			fprintf(serialports[i]->debugfp,"%12.3f write 0x%x to %s.\r\n",
				PIC_FullIndex(),val,dbgtext[port&0x7]);
#endif

			switch (port & 0x7) {
				case THR_OFFSET:
					serialports[i]->Write_THR (val);
					return;
				case IER_OFFSET:
					serialports[i]->Write_IER (val);
					return;
				case FCR_OFFSET:
					serialports[i]->Write_FCR (val);
					return;
				case LCR_OFFSET:
					serialports[i]->Write_LCR (val);
					return;
				case MCR_OFFSET:
					serialports[i]->Write_MCR (val);
					return;
				case MSR_OFFSET:
					serialports[i]->Write_MSR (val);
					return;
				case SPR_OFFSET:
					serialports[i]->Write_SPR (val);
					return;
				default:
					serialports[i]->Write_reserved (val, port & 0x7);
			}
		}
	}
}

void CSerial::changeLineProperties() {
	// update the event wait time

	float bitlen = (1000.0f/115200.0f)*(float)baud_divider;
	bytetime=bitlen*(float)(1+5+1);		// startbit + minimum length + stopbit
	bytetime+= bitlen*(float)(LCR&0x3); // databits
	if(LCR&0x4) bytetime+=bitlen;		// stopbit
	updatePortConfig (baud_divider, LCR);
}

static void Serial_EventHandler(Bitu val) {
	Bitu serclassid=val&0x3;
	if(serialports[serclassid]!=0)
		serialports[serclassid]->handleEvent(val>>2);
}

void CSerial::setEvent(Bit16u type, float duration) {
    PIC_AddEvent(Serial_EventHandler,duration,(type<<2)|idnumber);
}

void CSerial::removeEvent(Bit16u type) {
    // TODO
	PIC_RemoveSpecificEvents(Serial_EventHandler,(type<<2)|idnumber);
}

void CSerial::handleEvent(Bit16u type) {
	switch(type) {
		case SERIAL_TX_LOOPBACK_EVENT: {

#if SERIAL_DEBUG
			if(dbg_serialtraffic)
				fprintf(debugfp,loopback_data<0x10? "%12.3f tx 0x%02x (%u) (loopback)\r\n":
												    "%12.3f tx 0x%02x (%c) (loopback)\r\n",
					PIC_FullIndex(),loopback_data,
					loopback_data);
#endif

			receiveByte (loopback_data);
			ByteTransmitted ();
			break;
		}
		case SERIAL_THR_LOOPBACK_EVENT: {
			ByteTransmitting();
			loopback_data=THR;
			setEvent(SERIAL_TX_LOOPBACK_EVENT,bytetime);	
			break;
		}
		case SERIAL_ERRMSG_EVENT: {
			LOG_MSG("Serial%d: Errors occured: "\
				"Framing %d, Parity %d, Overrun %d (IF0:%d), Break %d", COMNUMBER,
				framingErrors, parityErrors, overrunErrors, overrunIF0, breakErrors);
			errormsg_pending=false;
			framingErrors=0;
			parityErrors=0;
			overrunErrors=0;
			overrunIF0=0;
			breakErrors=0;
			break;					  
		}
		default: handleUpperEvent(type);
	}
}

/*****************************************************************************/
/* Interrupt control routines                                               **/
/*****************************************************************************/
void CSerial::rise (Bit8u priority) {
#if SERIAL_DEBUG
	if(dbg_interrupt)
	{
		if(priority&TX_PRIORITY && !(waiting_interrupts&TX_PRIORITY))
			fprintf(debugfp,"%12.3f tx interrupt on.\r\n",PIC_FullIndex());

		if(priority&RX_PRIORITY && !(waiting_interrupts&RX_PRIORITY))
			fprintf(debugfp,"%12.3f rx interrupt on.\r\n",PIC_FullIndex());

		if(priority&MSR_PRIORITY && !(waiting_interrupts&MSR_PRIORITY))
			fprintf(debugfp,"%12.3f msr interrupt on.\r\n",PIC_FullIndex());

		if(priority&ERROR_PRIORITY && !(waiting_interrupts&ERROR_PRIORITY))
			fprintf(debugfp,"%12.3f error interrupt on.\r\n",PIC_FullIndex());
	}
#endif
	
	waiting_interrupts |= priority;
	ComputeInterrupts();
}

// clears the pending interrupt, triggers other waiting interrupt
void CSerial::clear (Bit8u priority) {
	
#if SERIAL_DEBUG
	if(dbg_interrupt)
	{
		if(priority&TX_PRIORITY && (waiting_interrupts&TX_PRIORITY))
			fprintf(debugfp,"%12.3f tx interrupt off.\r\n",PIC_FullIndex());

		if(priority&RX_PRIORITY && (waiting_interrupts&RX_PRIORITY))
			fprintf(debugfp,"%12.3f rx interrupt off.\r\n",PIC_FullIndex());

		if(priority&MSR_PRIORITY && (waiting_interrupts&MSR_PRIORITY))
			fprintf(debugfp,"%12.3f msr interrupt off.\r\n",PIC_FullIndex());

		if(priority&ERROR_PRIORITY && (waiting_interrupts&ERROR_PRIORITY))
			fprintf(debugfp,"%12.3f error interrupt off.\r\n",PIC_FullIndex());
	}
#endif
	
	
	waiting_interrupts &= (~priority);
	ComputeInterrupts();
}

void CSerial::ComputeInterrupts () {

	Bitu val = IER & waiting_interrupts;

	if (val & ERROR_PRIORITY)	 ISR = ISR_ERROR_VAL;
	else if (val & RX_PRIORITY)	 ISR = ISR_RX_VAL;
	else if (val & TX_PRIORITY)	 ISR = ISR_TX_VAL;
	else if (val & MSR_PRIORITY) ISR = ISR_MSR_VAL;
	else ISR = ISR_CLEAR_VAL;

	if(val && !irq_active) {
		irq_active=true;
		PIC_ActivateIRQ(irq);

#if SERIAL_DEBUG
		if(dbg_interrupt)
			fprintf(debugfp,"%12.3f IRQ%d on.\r\n",PIC_FullIndex(),irq);
#endif
	}

	if(!val && irq_active) {
		irq_active=false;
		PIC_DeActivateIRQ(irq);

#if SERIAL_DEBUG
		if(dbg_interrupt)
			fprintf(debugfp,"%12.3f IRQ%d off.\r\n",PIC_FullIndex(),irq);
#endif
	}
}

/*****************************************************************************/
/* Can a byte be received?                                                  **/
/*****************************************************************************/
bool CSerial::CanReceiveByte() {
	return (LSR & LSR_RX_DATA_READY_MASK) == 0;
}

/*****************************************************************************/
/* A byte was received                                                      **/
/*****************************************************************************/
void CSerial::receiveByte (Bit8u data) {

#if SERIAL_DEBUG
	if(dbg_serialtraffic)
		fprintf(debugfp,loopback_data<0x10? "%12.3f rx 0x%02x (%u)\r\n":
											"%12.3f rx 0x%02x (%c)\r\n",
			PIC_FullIndex(), data, data);
#endif

	if (LSR & LSR_RX_DATA_READY_MASK) {	// Overrun error ;o
		if(!errormsg_pending) {
			errormsg_pending=true;
			setEvent(SERIAL_ERRMSG_EVENT,1000);
		}
		overrunErrors++;
		Bitu iflag= GETFLAG(IF);
		if(!iflag)overrunIF0++;

#if SERIAL_DEBUG
		if(dbg_serialtraffic)
			fprintf(debugfp, "%12.3f rx overrun (IF=%d)\r\n",
				PIC_FullIndex(), iflag);
#endif

		LSR |= LSR_OVERRUN_ERROR_MASK;
		rise (ERROR_PRIORITY);
	} else {
		RHR = data;
		LSR |= LSR_RX_DATA_READY_MASK;
		rise (RX_PRIORITY);
	}
}

/*****************************************************************************/
/* A line error was received                                                **/
/*****************************************************************************/
void CSerial::receiveError (Bit8u errorword) {
	
	if(!errormsg_pending) {
			errormsg_pending=true;
			setEvent(SERIAL_ERRMSG_EVENT,1000);
	}
	if(errorword&LSR_PARITY_ERROR_MASK) {
		parityErrors++;

#if SERIAL_DEBUG
		if(dbg_serialtraffic)
			fprintf(debugfp, "%12.3f parity error\r\n",
				PIC_FullIndex());
#endif

	}
	if(errorword&LSR_FRAMING_ERROR_MASK) {
		framingErrors++;

#if SERIAL_DEBUG
		if(dbg_serialtraffic)
			fprintf(debugfp, "%12.3f framing error\r\n",
				PIC_FullIndex());
#endif

	}
	if(errorword&LSR_RX_BREAK_MASK) {
		breakErrors++;

#if SERIAL_DEBUG
		if(dbg_serialtraffic)
			fprintf(debugfp, "%12.3f break received\r\n",
				PIC_FullIndex());
#endif

	}
	LSR |= errorword;

	rise (ERROR_PRIORITY);
}

/*****************************************************************************/
/* ByteTransmitting: Byte has made it from THR to TX.                       **/
/*****************************************************************************/
void CSerial::ByteTransmitting() {
	switch(LSR&(LSR_TX_HOLDING_EMPTY_MASK|LSR_TX_EMPTY_MASK))
	{
		case LSR_TX_HOLDING_EMPTY_MASK|LSR_TX_EMPTY_MASK:
			// bad case there must have been one
		case LSR_TX_HOLDING_EMPTY_MASK:
		case LSR_TX_EMPTY_MASK: // holding full but workreg empty impossible
			LOG_MSG("Internal error in serial port(1)(0x%x).",LSR);
			break;
		case 0: // THR is empty now.
			LSR |= LSR_TX_HOLDING_EMPTY_MASK;
			
			// trigger interrupt
			rise (TX_PRIORITY);
			break;
	}
}


/*****************************************************************************/
/* ByteTransmitted: When a byte was sent, notify here.                      **/
/*****************************************************************************/
void CSerial::ByteTransmitted () {
	switch(LSR&(LSR_TX_HOLDING_EMPTY_MASK|LSR_TX_EMPTY_MASK))
	{
		case LSR_TX_HOLDING_EMPTY_MASK|LSR_TX_EMPTY_MASK:
			// bad case there must have been one
		case LSR_TX_EMPTY_MASK: // holding full but workreg empty impossible
			LOG_MSG("Internal error in serial port(2).");
			break;
		
		case LSR_TX_HOLDING_EMPTY_MASK: // now both are empty
			LSR |= LSR_TX_EMPTY_MASK;
			break;

		case 0: // now one is empty, send the other one
			LSR |= LSR_TX_HOLDING_EMPTY_MASK;
			if (loopback) {
				loopback_data=THR;
				setEvent(SERIAL_TX_LOOPBACK_EVENT, bytetime);
			}
			else {

		#if SERIAL_DEBUG
				if(dbg_serialtraffic)
					fprintf(debugfp,THR<0x10? "%12.3f tx 0x%02x (%u) (from THR)\r\n":
														"%12.3f tx 0x%02x (%c) (from THR)\r\n",
						PIC_FullIndex(),THR,
						THR);
		#endif

				transmitByte(THR,false);
			}
			// It's ok here.
			rise (TX_PRIORITY);
			break;
	}
}

/*****************************************************************************/
/* Transmit Holding Register, also LSB of Divisor Latch (r/w)               **/
/*****************************************************************************/
void CSerial::Write_THR (Bit8u data) {
	// 0-7 transmit data
	
	if ((LCR & LCR_DIVISOR_Enable_MASK)) {
		// write to DLL
		baud_divider&=0xFF00;
		baud_divider |= data;
		changeLineProperties();
	} else {
		// write to THR
		clear (TX_PRIORITY);

		switch(LSR&(LSR_TX_HOLDING_EMPTY_MASK|LSR_TX_EMPTY_MASK))
		{
			case 0: // both full - overflow
#if SERIAL_DEBUG
				if(dbg_serialtraffic) fprintf(debugfp, "%12.3f tx overflow\r\n",
					PIC_FullIndex());
#endif
				// overwrite THR
				THR = data;
				break;
		
			case LSR_TX_EMPTY_MASK: // holding full but workreg empty impossible
				LOG_MSG("Internal error in serial port(3).");
				break;
		
			case LSR_TX_HOLDING_EMPTY_MASK: // now both are full
				LSR &= (~LSR_TX_HOLDING_EMPTY_MASK);
				THR = data;
				break;

			case LSR_TX_HOLDING_EMPTY_MASK|LSR_TX_EMPTY_MASK:
				// both are full until the first delay has passed
				THR=data;
				LSR &= (~LSR_TX_EMPTY_MASK);
				LSR &= (~LSR_TX_HOLDING_EMPTY_MASK);
				if(loopback) setEvent(SERIAL_THR_LOOPBACK_EVENT, bytetime/10);
				else {
			
#if SERIAL_DEBUG
					if(dbg_serialtraffic)
						fprintf(debugfp,data<0x10? "%12.3f tx 0x%02x (%u)\r\n":
												   "%12.3f tx 0x%02x (%c)\r\n",
							PIC_FullIndex(),data,
							data);
#endif	
							
					transmitByte (data,true);
				}
				
				// triggered
				// when holding gets empty
				// rise (TX_PRIORITY);
				break;
		}
	}
}


/*****************************************************************************/
/* Receive Holding Register, also LSB of Divisor Latch (r/w)                **/
/*****************************************************************************/
Bitu CSerial::Read_RHR () {
	// 0-7 received data
	if ((LCR & LCR_DIVISOR_Enable_MASK)) return baud_divider&0xff;
	else {
		clear (RX_PRIORITY);
		LSR &= (~LSR_RX_DATA_READY_MASK);
		return RHR;
	}
}

/*****************************************************************************/
/* Interrupt Enable Register, also MSB of Divisor Latch (r/w)               **/
/*****************************************************************************/
// Modified by:
// - writing to it.
Bitu CSerial::Read_IER () {
	// 0	receive holding register (byte received)
	// 1	transmit holding register (byte sent)
	// 2	receive line status (overrun, parity error, frame error, break)
	// 3	modem status 
	// 4-7	0

	if (LCR & LCR_DIVISOR_Enable_MASK) return baud_divider>>8;
	else return IER;
}

void CSerial::Write_IER (Bit8u data) {
	if ((LCR & LCR_DIVISOR_Enable_MASK)) {	// write to DLM
		baud_divider&=0xff;
		baud_divider |= ((Bit16u)data)<<8;
		changeLineProperties();
	} else {

		IER = data&0xF;
		if ((LSR & LSR_TX_HOLDING_EMPTY_MASK) && (IER&TX_PRIORITY))
			waiting_interrupts |= TX_PRIORITY;
		
		ComputeInterrupts();
	}
}

/*****************************************************************************/
/* Interrupt Status Register (r)                                            **/
/*****************************************************************************/
// modified by:
// - incoming interrupts
// - loopback mode
Bitu CSerial::Read_ISR () {
	// 0	0:interrupt pending 1: no interrupt
	// 1-3	identification
	//      011 LSR
	//		010 RXRDY
	//		001 TXRDY
	//		000 MSR
	// 4-7	0

	if(IER&Modem_Status_INT_Enable_MASK) updateMSR();
	Bit8u retval = ISR;

	// clear changes ISR!! mean..
	if(ISR==ISR_TX_VAL) clear (TX_PRIORITY);
	return retval;
}

void CSerial::Write_FCR (Bit8u data) {
	if((!fifo_warn) && (data&0x1)) {
		fifo_warn=true;
		LOG_MSG("Serial%d: Warning: Tried to activate FIFO.",COMNUMBER);
	}
}

/*****************************************************************************/
/* Line Control Register (r/w)                                              **/
/*****************************************************************************/
// signal decoder configuration:
// - parity, stopbits, word length
// - send break
// - switch between RHR/THR and baud rate registers
// Modified by:
// - writing to it.
Bitu CSerial::Read_LCR () {
	// 0-1	word length
	// 2	stop bits
	// 3	parity enable
	// 4-5	parity type
	// 6	set break
	// 7	divisor latch enable
	return LCR;
}

void CSerial::Write_LCR (Bit8u data) {
	Bit8u lcr_old = LCR;
	LCR = data;
	if (((data ^ lcr_old) & LCR_PORTCONFIG_MASK) != 0) {
		changeLineProperties();
	}
	if (((data ^ lcr_old) & LCR_BREAK_MASK) != 0) {
		if(!loopback) setBreak ((LCR & LCR_BREAK_MASK)!=0);
		else {
			// TODO: set loopback break event to reveiveError after
		}
#if SERIAL_DEBUG
		if(dbg_serialtraffic)
			fprintf(debugfp,((LCR & LCR_BREAK_MASK)!=0)?
						"%12.3f break on.\r\n":
						"%12.3f break off.\r\n", PIC_FullIndex());
#endif
	}
}

/*****************************************************************************/
/* Modem Control Register (r/w)                                             **/
/*****************************************************************************/
// Set levels of RTS and DTR, as well as loopback-mode.
// Modified by: 
// - writing to it.
Bitu CSerial::Read_MCR () {
	// 0	-DTR
	// 1	-RTS
	// 2	-OP1
	// 3	-OP2
	// 4	loopback enable
	// 5-7	0
	Bit8u retval=0;
	if(dtr) retval|=MCR_DTR_MASK;
	if(rts) retval|=MCR_RTS_MASK;
	if(op1) retval|=MCR_OP1_MASK;
	if(op2) retval|=MCR_OP2_MASK;
	if(loopback) retval|=MCR_LOOPBACK_Enable_MASK;
	return retval;
}

void CSerial::Write_MCR (Bit8u data) {
	// WARNING: At the time setRTSDTR is called rts and dsr members are still wrong.

	bool temp_dtr = data & MCR_DTR_MASK? true:false;
	bool temp_rts = data & MCR_RTS_MASK? true:false;
	bool temp_op1 = data & MCR_OP1_MASK? true:false;
	bool temp_op2 = data & MCR_OP2_MASK? true:false;
	bool temp_loopback = data & MCR_LOOPBACK_Enable_MASK? true:false;
	if(loopback!=temp_loopback) {
		if(temp_loopback) setRTSDTR(false,false);
		else setRTSDTR(temp_rts,temp_dtr);
	}

	if (temp_loopback) {	// is on:
		// DTR->DSR
		// RTS->CTS
		// OP1->RI
		// OP2->CD
		if(temp_dtr!=dtr && !d_dsr) {
			d_dsr=true;
			rise (MSR_PRIORITY);
		}
		if(temp_rts!=rts && !d_cts) {
			d_cts=true;
			rise (MSR_PRIORITY);
		}
		if(temp_op1!=op1 && !d_ri) {
			// interrupt only at trailing edge
			if(!temp_op1) {
				d_ri=true;
				rise (MSR_PRIORITY);
			}
		}
		if(temp_op2!=op2 && !d_cd) {
			d_cd=true;
			rise (MSR_PRIORITY);
		}
	} else {
		// loopback is off
		if(temp_rts!=rts) {
			// RTS difference
			if(temp_dtr!=dtr) {
				// both difference

#if SERIAL_DEBUG
			if(dbg_modemcontrol)
			{
				fprintf(debugfp,temp_rts?"%12.3f RTS on.\r\n":
									"%12.3f RTS off.\r\n", PIC_FullIndex());
				fprintf(debugfp,temp_dtr?"%12.3f DTR on.\r\n":
									"%12.3f DTR off.\r\n", PIC_FullIndex());
			}
#endif
				setRTSDTR(temp_rts, temp_dtr);
			} else {
				// only RTS

#if SERIAL_DEBUG
			if(dbg_modemcontrol)
				fprintf(debugfp,temp_rts?"%12.3f RTS on.\r\n":"%12.3f RTS off.\r\n", PIC_FullIndex());
#endif

				setRTS(temp_rts);
			}
		} else if(temp_dtr!=dtr) {
			// only DTR
#if SERIAL_DEBUG
			if(dbg_modemcontrol)
				fprintf(debugfp,temp_dtr?"%12.3f DTR on.\r\n":"%12.3f DTR off.\r\n", PIC_FullIndex());
#endif
			setDTR(temp_dtr);
		}
	}
	dtr=temp_dtr;
	rts=temp_rts;
	op1=temp_op1;
	op2=temp_op2;
	loopback=temp_loopback;
}

/*****************************************************************************/
/* Line Status Register (r)                                                 **/
/*****************************************************************************/
// errors, tx registers status, rx register status
// modified by:
// - event from real serial port
// - loopback
Bitu CSerial::Read_LSR () {
	Bitu retval = LSR;
	LSR &= (~LSR_ERROR_MASK);			// clear error bits on read
	clear (ERROR_PRIORITY);
	return retval;
}

void CSerial::Write_MSR (Bit8u val) {
	d_cts = (val&MSR_dCTS_MASK)?true:false;
	d_dsr = (val&MSR_dDSR_MASK)?true:false;
	d_cd = (val&MSR_dCD_MASK)?true:false;
	d_ri = (val&MSR_dRI_MASK)?true:false;
}

/*****************************************************************************/
/* Modem Status Register (r)                                                **/
/*****************************************************************************/
// Contains status of the control input lines (CD, RI, DSR, CTS) and
// their "deltas": if level changed since last read delta = 1.
// modified by:
// - real values
// - write operation to MCR in loopback mode
Bitu CSerial::Read_MSR () {
	Bit8u retval=0;
	
	if (loopback) {
		
		if (rts) retval |= MSR_CTS_MASK;
		if (dtr) retval |= MSR_DSR_MASK;
		if (op1) retval |= MSR_RI_MASK;
		if (op2) retval |= MSR_CD_MASK;
	
	} else {

		updateMSR();
		if (cd) retval |= MSR_CD_MASK;
		if (ri) retval |= MSR_RI_MASK;
		if (dsr) retval |= MSR_DSR_MASK;
		if (cts) retval |= MSR_CTS_MASK;
	
	}
	// new delta flags
	if(d_cd) retval|=MSR_dCD_MASK;
	if(d_ri) retval|=MSR_dRI_MASK;
	if(d_cts) retval|=MSR_dCTS_MASK;
	if(d_dsr) retval|=MSR_dDSR_MASK;
	
	d_cd = false;
	d_ri = false;
	d_cts = false;
	d_dsr = false;
	
	clear (MSR_PRIORITY);
	return retval;
}

/*****************************************************************************/
/* Scratchpad Register (r/w)                                                **/
/*****************************************************************************/
// Just a memory register. Not much to do here.
Bitu CSerial::Read_SPR () {
	return SPR;
}

void CSerial::Write_SPR (Bit8u data) {
	SPR = data;
}

/*****************************************************************************/
/* Write_reserved                                                           **/
/*****************************************************************************/
void CSerial::Write_reserved (Bit8u data, Bit8u address) {
	/*LOG_UART("Serial%d: Write to reserved register, value 0x%x, register %x",
		COMNUMBER, data, address);*/
}

/*****************************************************************************/
/* MCR Access: returns cirquit state as boolean.                            **/
/*****************************************************************************/
bool CSerial::getDTR () {
	if(loopback) return false;
	else return dtr;
}

bool CSerial::getRTS () {
	if(loopback) return false;
	else return rts;
}

/*****************************************************************************/
/* MSR Access                                                               **/
/*****************************************************************************/
bool CSerial::getRI () {
	return ri;
}

bool CSerial::getCD () {
	return cd;
}

bool CSerial::getDSR () {
	return dsr;
}

bool CSerial::getCTS () {
	return cts;
}

void CSerial::setRI (bool value) {
	if (value != ri) {

#if SERIAL_DEBUG
		if(dbg_modemcontrol)
			fprintf(debugfp,value?"%12.3f RI  on.\r\n":"%12.3f RI  off.\r\n", PIC_FullIndex());
#endif
		// don't change delta when in loopback mode
		ri=value;
		if(!loopback) {
            if(value==false) d_ri=true;
			rise (MSR_PRIORITY);
		}
	}
	//else no change
}
void CSerial::setDSR (bool value) {
	if (value != dsr) {
#if SERIAL_DEBUG
		if(dbg_modemcontrol)
			fprintf(debugfp,value?"%12.3f DSR on.\r\n":"%12.3f DSR off.\r\n", PIC_FullIndex());
#endif
		// don't change delta when in loopback mode
		dsr=value;
		if(!loopback) {
            d_dsr=true;
			rise (MSR_PRIORITY);
		}
	}
	//else no change
}
void CSerial::setCD (bool value) {
	if (value != cd) {
#if SERIAL_DEBUG
		if(dbg_modemcontrol)
			fprintf(debugfp,value?"%12.3f CD  on.\r\n":"%12.3f CD  off.\r\n", PIC_FullIndex());
#endif
		// don't change delta when in loopback mode
		cd=value;
		if(!loopback) {
            d_cd=true;
			rise (MSR_PRIORITY);
		}
	}
	//else no change
}
void CSerial::setCTS (bool value) {
	if (value != cts) {
#if SERIAL_DEBUG
		if(dbg_modemcontrol)
			fprintf(debugfp,value?"%12.3f CTS on.\r\n":"%12.3f CTS off.\r\n", PIC_FullIndex());
#endif
		// don't change delta when in loopback mode
		cts=value;
		if(!loopback) {
            d_cts=true;
			rise (MSR_PRIORITY);
		}
	}
	//else no change
}

/*****************************************************************************/
/* Initialisation                                                           **/
/*****************************************************************************/
void CSerial::Init_Registers () {
	// The "power on" settings
	irq_active=false;
	waiting_interrupts = 0x0;

	Bit32u initbps = 9600;
	Bit8u bytesize = 8;
    char parity = 'N';
	Bit8u stopbits = 1;
							  
	Bit8u lcrresult = 0;
	Bit16u baudresult = 0;

	RHR = 0;
	THR = 0;
	IER = 0;
	ISR = 0x1;
	LCR = 0;
	//MCR = 0xff;
	loopback = true;
	dtr=true;
	rts=true;
	op1=true;
	op2=true;


	LSR = 0x60;
	d_cts = true;	
	d_dsr = true;	
	d_ri = true;
	d_cd = true;	
	cts = true;	
	dsr = true;	
	ri = true;	
	cd = true;	

	SPR = 0xFF;

	baud_divider=0x0;

	// make lcr: byte size, parity, stopbits, baudrate

	if (bytesize == 5)
		lcrresult |= LCR_DATABITS_5;
	else if (bytesize == 6)
		lcrresult |= LCR_DATABITS_6;
	else if (bytesize == 7)
		lcrresult |= LCR_DATABITS_7;
	else
		lcrresult |= LCR_DATABITS_8;

	switch(parity)
	{
	case 'N':
	case 'n':
		lcrresult |= LCR_PARITY_NONE;
		break;
	case 'O':
	case 'o':
		lcrresult |= LCR_PARITY_ODD;
		break;
	case 'E':
	case 'e':
		lcrresult |= LCR_PARITY_EVEN;
		break;
	case 'M':
	case 'm':
		lcrresult |= LCR_PARITY_MARK;
		break;
	case 'S':
	case 's':
		lcrresult |= LCR_PARITY_SPACE;
		break;
	}

	// baudrate
	if (initbps > 0)
		baudresult = (Bit16u) (115200 / initbps);
	else
		baudresult = 12;			// = 9600 baud

	Write_MCR (0);
	Write_LCR (LCR_DIVISOR_Enable_MASK);
	Write_THR ((Bit8u) baudresult & 0xff);
	Write_IER ((Bit8u) (baudresult >> 8));
	Write_LCR (lcrresult);
	updateMSR();
	Read_MSR();
}

CSerial::CSerial(Bitu id, CommandLine* cmd) {

#if SERIAL_DEBUG
	dbg_serialtraffic = cmd->FindExist("dbgtr", false);
	dbg_modemcontrol  = cmd->FindExist("dbgmd", false);
	dbg_register      = cmd->FindExist("dbgreg", false);
	dbg_interrupt     = cmd->FindExist("dbgirq", false);
	dbg_aux			  = cmd->FindExist("dbgaux", false);


	if(dbg_serialtraffic|dbg_modemcontrol|dbg_register|dbg_interrupt|dbg_aux)
		debugfp=OpenCaptureFile("serlog",".serlog.txt");
	else debugfp=0;

#endif



	idnumber=id;
	mydosdevice=new device_COM(this);
	DOS_AddDevice(mydosdevice);
	Bit16u base = serial_baseaddr[id];
	fifo_warn=false;

	errormsg_pending=false;
	framingErrors=0;
	parityErrors=0;
	overrunErrors=0;
	overrunIF0=0;
	breakErrors=0;
	
	// find the IRQ
	irq = serial_defaultirq[id];
	getBituSubstring("irq:",&irq, cmd);
	if (irq < 2 || irq > 15) irq = serial_defaultirq[id];


	for (Bitu i = 0; i <= 7; i++) {
		WriteHandler[i].Install (i + base, SERIAL_Write, IO_MB);
		ReadHandler[i].Install (i + base, SERIAL_Read, IO_MB);
	}

#if SERIAL_DEBUG
	if(debugfp) fprintf(debugfp,"COM%d: BASE %3x, IRQ %d\r\n\r\n",
		COMNUMBER,base,irq);
#endif
};

bool CSerial::getBituSubstring(const char* name,Bitu* data, CommandLine* cmd) {
	std::string tmpstring;
	if(!(cmd->FindStringBegin(name,tmpstring,false))) return false;
	const char* tmpchar=tmpstring.c_str();
	if(sscanf(tmpchar,"%u",data)!=1) return false;
	return true;
}

CSerial::~CSerial(void) {
	DOS_DelDevice(mydosdevice);
	for(Bitu i = 0; i <= SERIAL_BASE_EVENT_COUNT; i++)
		removeEvent(i);
};
bool CSerial::Getchar(Bit8u* data, Bit8u* lsr, bool wait_dsr, Bitu timeout) {
	
	double starttime=PIC_FullIndex();
	// wait for it to become empty
	// wait for DSR on
	if(wait_dsr) {
		while((!(Read_MSR()&0x20))&&(starttime>PIC_FullIndex()-timeout))
			CALLBACK_Idle();
		if(!(starttime>PIC_FullIndex()-timeout)) {
			#if SERIAL_DEBUG
if(dbg_aux)
		fprintf(debugfp,"%12.3f Getchar status timeout: MSR 0x%x\r\n", PIC_FullIndex(),Read_MSR());
#endif
			return false;
		}
	}
	// wait for a byte to arrive
	while((!((*lsr=Read_LSR())&0x1))&&(starttime>PIC_FullIndex()-timeout))
		CALLBACK_Idle();
	
	if(!(starttime>PIC_FullIndex()-timeout)) {
		#if SERIAL_DEBUG
if(dbg_aux)
		fprintf(debugfp,"%12.3f Getchar data timeout: MSR 0x%x\r\n", PIC_FullIndex(),Read_MSR());
#endif
		return false;
	}


	*data=Read_RHR();

#if SERIAL_DEBUG
	if(dbg_aux)
		fprintf(debugfp,"%12.3f API read success: 0x%x\r\n", PIC_FullIndex(),*data);
#endif

	return true;
}


bool CSerial::Putchar(Bit8u data, bool wait_dsr, bool wait_cts, Bitu timeout) {
	
	double starttime=PIC_FullIndex();
	//Bit16u starttime=
	// wait for it to become empty
	while(!(LSR&0x20)) {
		CALLBACK_Idle();
	}
	// wait for DSR+CTS on
	if(wait_dsr||wait_cts) {
		if(wait_dsr||wait_cts) {
			while(((Read_MSR()&0x30)!=0x30)&&(starttime>PIC_FullIndex()-timeout))
				CALLBACK_Idle();
		} else if(wait_dsr) {
			while(!(Read_MSR()&0x20)&&(starttime>PIC_FullIndex()-timeout))
				CALLBACK_Idle();
		} else if(wait_cts) {
			while(!(Read_MSR()&0x10)&&(starttime>PIC_FullIndex()-timeout))
				CALLBACK_Idle();

		} 
		if(!(starttime>PIC_FullIndex()-timeout)) {
#if SERIAL_DEBUG
			if(dbg_aux)
				fprintf(debugfp,"%12.3f Putchar timeout: MSR 0x%x\r\n",
					PIC_FullIndex(),Read_MSR());
#endif
			return false;
		}
	}
	Write_THR(data);

#if SERIAL_DEBUG
	if(dbg_aux)
		fprintf(debugfp,"%12.3f API write success: 0x%x\r\n", PIC_FullIndex(),data);
#endif

	return true;
}

class SERIALPORTS:public Module_base {
public:
	SERIALPORTS (Section * configuration):Module_base (configuration) {
		
		Bit16u biosParameter[4] = { 0, 0, 0, 0 };
		Section_prop *section = static_cast <Section_prop*>(configuration);

		const char *configstrings[4] = {
			section->Get_string ("serial1"),
			section->Get_string ("serial2"),
			section->Get_string ("serial3"),
			section->Get_string ("serial4")
		};
		// iterate through all 4 com ports
		for (Bitu i = 0; i < 4; i++) {
			biosParameter[i] = serial_baseaddr[i];

			CommandLine* cmd;
			std::string str;
			cmd=new CommandLine(0,configstrings[i]);
			cmd->FindCommand(1,str);
			
			if(!str.compare("dummy")) {
				serialports[i] = new CSerialDummy (i, cmd);
			}
#ifdef DIRECTSERIAL_AVAILIBLE
			else if(!str.compare("directserial")) {
				serialports[i] = new CDirectSerial (i, cmd);
				if (!serialports[i]->InstallationSuccessful)  {
					// serial port name was wrong or already in use
					delete serialports[i];
					serialports[i] = NULL;
					biosParameter[i] = 0;
				}
			}
#endif

#if C_MODEM
			else if(!str.compare("modem")) {
				serialports[i] = new CSerialModem (i, cmd);
				if (!serialports[i]->InstallationSuccessful)  {
					delete serialports[i];
					serialports[i] = NULL;
					biosParameter[i] = 0;
				}
			}
			else if(!str.compare("nullmodem")) {
				serialports[i] = new CNullModem (i, cmd);
				if (!serialports[i]->InstallationSuccessful)  {
					delete serialports[i];
					serialports[i] = NULL;
					biosParameter[i] = 0;
				}
			}
#endif
			else if(!str.compare("disabled")) {
				serialports[i] = NULL;
				biosParameter[i] = 0;
			} else {
				LOG_MSG ("Invalid type for COM%d.", i + 1);
				serialports[i] = NULL;
				biosParameter[i] = 0;
			}
			delete cmd;
		} // for
		BIOS_SetComPorts (biosParameter);
	}

	~SERIALPORTS () {
		for (Bitu i = 0; i < 4; i++)
			if (serialports[i]) {
				delete serialports[i];
				serialports[i] = 0;
			}
	}
};

static SERIALPORTS *testSerialPortsBaseclass;

void SERIAL_Destroy (Section * sec) {
	delete testSerialPortsBaseclass;
	testSerialPortsBaseclass = NULL;
}

void SERIAL_Init (Section * sec) {
	// should never happen
	if (testSerialPortsBaseclass) delete testSerialPortsBaseclass;
	testSerialPortsBaseclass = new SERIALPORTS (sec);
	sec->AddDestroyFunction (&SERIAL_Destroy, true);
}
