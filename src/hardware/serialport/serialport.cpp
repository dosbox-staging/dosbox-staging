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

/* $Id: serialport.cpp,v 1.5 2007-01-08 19:45:41 qbix79 Exp $ */

#include <string.h>
#include <ctype.h>

#include "dosbox.h"

#include "support.h"
#include "inout.h"
#include "pic.h"
#include "setup.h"
#include "timer.h"

#include "serialport.h"
#include "directserial_win32.h"
#include "directserial_os2.h"
#include "serialdummy.h"
#include "softmodem.h"

#define LOG_UART LOG_MSG


// COM1 - COM4 objects
static CSerial *serial1 = 0;
static CSerial *serial2 = 0;
static CSerial *serial3 = 0;
static CSerial *serial4 = 0;
//static CSerial** serialPortObjects[] = {NULL, &serial1,&serial2,&serial3,&serial4};

Bit8u serialGetComnumberByBaseAddress (Bit16u base) {
	if (base == COM1_BASE)
		return 1;
	else if (base == COM2_BASE)
		return 2;
	else if (base == COM3_BASE)
		return 3;
	else if (base == COM4_BASE)
		return 4;
	else
		return 255;	// send thispointer to nirwana ;)
}

// Usage of FastDelegates (http://www.codeproject.com/cpp/FastDelegate.asp)
// as I/O-Array would make many of these unneccessary. (member function pointer)

//Some defines for repeated functions

#define SERIAL_UPDATE(number) \
void SERIAL##number##_Update(void) {	\
	serial##number->Timer ();	\
}

#define SERIAL_WRITE_TREE(number) \
static void SERIAL##number##_Write (Bitu port, Bitu val, Bitu iolen) {	\
	switch (port & 0x7) {	\
	case THR_OFFSET:	\
		serial##number ->Write_THR (val);	\
		return;	\
	case IER_OFFSET:	\
		serial##number ->Write_IER (val);	\
		return;	\
	case LCR_OFFSET:	\
		serial##number ->Write_LCR (val);	\
		return;	\
	case MCR_OFFSET:	\
		serial##number ->Write_MCR (val);	\
		return;	\
	case MSR_OFFSET:	\
		serial##number ->Write_MSR (val);	\
		return;	\
	case SPR_OFFSET:	\
		serial##number ->Write_SPR (val);	\
		return;	\
	default:	\
		serial##number ->Write_reserved (val, port & 0x7);	\
	}	\
}

#define SERIAL_READ_TREE(number) \
static Bitu SERIAL##number##_Read (Bitu port, Bitu iolen) {	\
	switch (port & 0x7) {	\
	case RHR_OFFSET:	\
		return serial##number ->Read_RHR ();	\
	case IER_OFFSET:	\
		return serial##number ->Read_IER ();	\
	case ISR_OFFSET:	\
		return serial##number ->Read_ISR ();	\
	case LCR_OFFSET:	\
		return serial##number ->Read_LCR ();	\
	case MCR_OFFSET:	\
		return serial##number ->Read_MCR ();	\
	case LSR_OFFSET:	\
		return serial##number ->Read_LSR ();	\
	case MSR_OFFSET:	\
		return serial##number ->Read_MSR ();	\
	case SPR_OFFSET:	\
		return serial##number ->Read_SPR ();	\
	}	\
	return 0;	\
}

//The Functions

SERIAL_UPDATE(1);
SERIAL_UPDATE(2);
SERIAL_UPDATE(3);
SERIAL_UPDATE(4);

SERIAL_WRITE_TREE(1);
SERIAL_WRITE_TREE(2);
SERIAL_WRITE_TREE(3);
SERIAL_WRITE_TREE(4);

SERIAL_READ_TREE(1);
SERIAL_READ_TREE(2);
SERIAL_READ_TREE(3);
SERIAL_READ_TREE(4);

#undef SERIAL_UPDATE
#undef SERIAL_WRITE_TREE
#undef SERIAL_READ_TREE

void CSerial::Timer (void) {
	//LOG_UART("Serial port at %x: Timer", base);
	if (loopback_pending) {
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Loopback sent back", base);
#endif
		loopback_pending = false;
		receiveByte (loopback_data);
		ByteTransmitted ();
	}
	Timer2 ();
}

/*****************************************************************************/
/* Interrupt control routines                                               **/
/*****************************************************************************/
void CSerial::rise (Bit8u priority) {
	//LOG_UART("Serial port at %x: Rise priority 0x%x", base, priority);
	waiting_interrupts |= priority;
	WriteRealIER (IER);
}

// clears the pending interrupt, triggers other waiting interrupt
void CSerial::clear (Bit8u priority) {
	//LOG_UART("Serial port at %x: cleared priority 0x%x", base, priority);
	waiting_interrupts &= (~priority);
	WriteRealIER (IER);
}

void CSerial::WriteRealIER (Bit8u data) {
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: write IER, value 0x%x", base, data);
#endif
	data = data & 0xF;						// THE UPPER ONES ALWAYS READ 0! NOTHIN' ELSE!
	Bit8u old_pending_interrupts = pending_interrupts;
	Bit8u difference_pending_interrupts;

	// rise TX AGAIN when present and is being enabled
	if ((data & TX_PRIORITY) && (!(IER & TX_PRIORITY)))
		if (LSR & LSR_TX_HOLDING_EMPTY_MASK)
			waiting_interrupts |= TX_PRIORITY;

	pending_interrupts = waiting_interrupts & data;
	if ((difference_pending_interrupts = (pending_interrupts ^ old_pending_interrupts))) {	// something in pending interrupts has changed
		if (difference_pending_interrupts & pending_interrupts) {	// some new bits were set
			if (!current_priority) {	// activate interrupt..
				if (pending_interrupts & ERROR_PRIORITY) {
					current_priority = ERROR_PRIORITY;
					ISR = ISR_ERROR_VAL;
				} else if (pending_interrupts & RX_PRIORITY) {
					current_priority = RX_PRIORITY;
					ISR = ISR_RX_VAL;
				} else if (pending_interrupts & TX_PRIORITY) {
					current_priority = TX_PRIORITY;
					ISR = ISR_TX_VAL;
				} else if (pending_interrupts & MSR_PRIORITY) {
					current_priority = MSR_PRIORITY;
					ISR = ISR_MSR_VAL;
				}
#ifdef SERIALPORT_DEBUGMSG
				LOG_UART ("Serial port at %x: Interrupt activated: priority %d", base, current_priority);
#endif
				PIC_ActivateIRQ (irq);
			}// else the new interrupts were already
			// written into pending_interrupts
		}// else no new bits were set

		if (difference_pending_interrupts & (~pending_interrupts)) {	// some bits were reset
			if (pending_interrupts) {	// some more are waiting
				if (!(current_priority & pending_interrupts)) {	// the current interrupt has been cleared
					// choose the next one
					if (pending_interrupts & ERROR_PRIORITY) {
						current_priority = ERROR_PRIORITY;
						ISR = ISR_ERROR_VAL;
					} else if (pending_interrupts & RX_PRIORITY) {
						current_priority = RX_PRIORITY;
						ISR = ISR_RX_VAL;
					} else if (pending_interrupts & TX_PRIORITY) {
						current_priority = TX_PRIORITY;
						ISR = ISR_TX_VAL;
					} else if (pending_interrupts & MSR_PRIORITY) {
						current_priority = MSR_PRIORITY;
						ISR = ISR_MSR_VAL;
					}
				}
			} else {
				current_priority = NONE_PRIORITY;
				ISR = ISR_CLEAR_VAL;
				PIC_DeActivateIRQ (irq);
#ifdef SERIALPORT_DEBUGMSG
				LOG_UART ("Serial port at %x: Interrupt deactivated.", base);
#endif
			}
		}
	}
	IER = data;
}



/*****************************************************************************/
/* Internal register modification                                           **/
/*****************************************************************************/
void CSerial::changeMSR (Bit8u data) {
	if (!(MCR & MCR_LOOPBACK_Enable_MASK)) {
		// see if something changed
		if ((MSR & MSR_LINE_MASK) != data) {
			Bit8u change = (MSR & MSR_LINE_MASK) ^ data;
			// set new deltas
			MSR |= (change >> 4);
			// set new line states
			MSR &= MSR_delta_MASK;
			MSR |= data;
			rise (MSR_PRIORITY);
		}
	}
}

void CSerial::changeMSR_Loopback (Bit8u data) {
	// see if something changed
	if ((MSR & MSR_LINE_MASK) != data) {
		Bit8u change = (MSR & MSR_LINE_MASK) ^ data;
		// set new deltas
		MSR |= (change >> 4);
		// set new line states
		MSR &= MSR_delta_MASK;
		MSR |= data;
		rise (MSR_PRIORITY);
	}

/*
	Bit8u temp=MSR;
	// look for signal changes
	if(temp|=((data&MSR_LINE_MASK)^(MSR&MSR_LINE_MASK))>>4)
	{	// some signals changed
		temp&=MSR_delta_MASK;// clear line states
		temp|=(data&MSR_LINE_MASK);	// set new line states
		
		MSR=temp;
		rise(MSR_PRIORITY);
	}
	// else nothing happened*/
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
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: byte received: %d", base, data);
#endif

	if (LSR & LSR_RX_DATA_READY_MASK) {	// Overrun error ;o
		LOG_UART ("Serial port at %x: RX Overrun!", base, data);
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
	LSR |= errorword;

	rise (ERROR_PRIORITY);
}

/*****************************************************************************/
/* ByteTransmitted: When a byte was sent, notify here.                      **/
/*****************************************************************************/
void CSerial::ByteTransmitted () {
	if (LSR & LSR_TX_HOLDING_EMPTY_MASK) {  // one space was empty
		// now both are
		LSR |= LSR_TX_EMPTY_MASK;
	} else {  // both were full, now 1 is empty
		if (MCR & MCR_LOOPBACK_Enable_MASK) {  // loopback mode
			transmitLoopbackByte (THR);
		} else {  // direct "real" mode
			transmitByte (THR);
		}
		LSR |= LSR_TX_HOLDING_EMPTY_MASK;
	}
	rise (TX_PRIORITY);
}

/*****************************************************************************/
/* Transmit Holding Register, also LSB of Divisor Latch (r/w)               **/
/*****************************************************************************/
void CSerial::Write_THR (Bit8u data) {
	if ((LCR & LCR_DIVISOR_Enable_MASK)) {
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Write to DLL value 0x%x", base, data);
#endif
		// write to DLL
		DLL = data;
		updatePortConfig (DLL, DLM, LCR);
	} else {
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Write to THR value 0x%x", base, data);
#endif

		// write to THR
		clear (TX_PRIORITY);

		if (LSR & LSR_TX_HOLDING_EMPTY_MASK) {	// one is empty 
			if (LSR & LSR_TX_EMPTY_MASK) {	// both are empty
				LSR &= (~LSR_TX_EMPTY_MASK);

				if (MCR & MCR_LOOPBACK_Enable_MASK) {	// loopback mode
					transmitLoopbackByte (data);
				} else {  // direct "real" mode
					transmitByte (data);
				}
				rise (TX_PRIORITY);
			} else {  // only THR is empty; add byte and clear holding bit
				LSR &= (~LSR_TX_HOLDING_EMPTY_MASK);
				THR = data;
			}
		} else {
#ifdef SERIALPORT_DEBUGMSG
			LOG_UART ("Serial port at %x: TX Overflow!", base);
#endif
			// TX overflow; how does real hardware respond to that... I do nothing
		}
	}
}


/*****************************************************************************/
/* Receive Holding Register, also LSB of Divisor Latch (r/w)                **/
/*****************************************************************************/
Bitu CSerial::Read_RHR () {
	Bit8u retval;
	if ((LCR & LCR_DIVISOR_Enable_MASK)) {
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Read from DLL value 0x%x", base, DLL);
#endif
		return DLL;
	} else {
		clear (RX_PRIORITY);
		LSR &= (~LSR_RX_DATA_READY_MASK);
		retval = RHR;
		RXBufferEmpty ();						// <--- this one changes RHR

#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Read from RHR value 0x%x", base, retval);
#endif

		return retval;
	}
}

/*****************************************************************************/
/* Interrupt Enable Register, also MSB of Divisor Latch (r/w)               **/
/*****************************************************************************/
// Modified by:
// - writing to it.
Bitu CSerial::Read_IER () {
	if ((LCR & LCR_DIVISOR_Enable_MASK)) {	// IER or MSB?
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Read from DLM value 0x%x", base, DLM);
#endif
		return DLM;
	} else {
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Read from IER value 0x%x", base, IER);
#endif
		return IER;
	}
}

void CSerial::Write_IER (Bit8u data) {
	if ((LCR & LCR_DIVISOR_Enable_MASK)) {	// write to DLM
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Write to DLM value 0x%x", base, data);
#endif

		DLM = data;
		updatePortConfig (DLL, DLM, LCR);
	} else {
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: Write to IER value 0x%x", base, data);
#endif
		WriteRealIER (data);
	}
}

/*****************************************************************************/
/* Interrupt Status Register (r)                                            **/
/*****************************************************************************/
// modified by:
// - incoming interrupts
// - loopback mode
Bitu CSerial::Read_ISR () {
	Bit8u retval = ISR;

	// clear changes ISR!! mean..
	clear (TX_PRIORITY);
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Read from ISR value 0x%x", base, retval);
#endif
	return retval;
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
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Read from LCR value 0x%x", base, LCR);
#endif
	return LCR;
}

void CSerial::Write_LCR (Bit8u data) {
	Bit8u lcr_old = LCR;
	LCR = data;
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Write to LCR value 0x%x", base, data);

	// for debug output
	if (((data ^ lcr_old) & LCR_DIVISOR_Enable_MASK) != 0) {
		if ((data & LCR_DIVISOR_Enable_MASK) != 0)
			LOG_UART ("Serial port at %x: Divisor-mode entered", base);

		else
			LOG_UART ("Serial port at %x: Divisor-mode exited", base);
	}
#endif
	if (((data ^ lcr_old) & LCR_PORTCONFIG_MASK) != 0) {
#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: comm parameters changed", base);
#endif
		updatePortConfig (DLL, DLM, LCR);
	}
	if (((data ^ lcr_old) & LCR_BREAK_MASK) != 0) {
		setBreak ((LCR & LCR_BREAK_MASK) != 0);

#ifdef SERIALPORT_DEBUGMSG
		LOG_UART ("Serial port at %x: break toggled: %d", base,
							(LCR & LCR_BREAK_MASK) != 0);
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
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Read from MCR value 0x%x", base, MCR);
#endif
	return MCR;
}

void CSerial::Write_MCR (Bit8u data) {
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Write to MCR value 0x%x", base, data);
#endif
	data &= 0x1F;					// UPPER BITS ALWAYS 0!!!

	if (MCR & MCR_LOOPBACK_Enable_MASK)
		if (data & MCR_LOOPBACK_Enable_MASK) {	// was on, is now on
			Bit8u param = 0;

			// RTS->CD
			// DTR->RI
			// OP1->DSR
			// OP2->CTS

			if (data & MCR_RTS_MASK)
				param |= MSR_CD_MASK;
			if (data & MCR_DTR_MASK)
				param |= MSR_RI_MASK;
			if (data & MCR_OP1_MASK)
				param |= MSR_DSR_MASK;
			if (data & MCR_OP2_MASK)
				param |= MSR_CTS_MASK;

			changeMSR_Loopback (param);

		} else {
			MCR = data;
			updateModemControlLines ();
			// is switched off now
	} else {
		if (data & MCR_LOOPBACK_Enable_MASK) {	// is switched on:
			Bit8u par = 0;
			if (data & MCR_RTS_MASK)
				par |= MSR_CD_MASK;
			if (data & MCR_DTR_MASK)
				par |= MSR_RI_MASK;
			if (data & MCR_OP1_MASK)
				par |= MSR_DSR_MASK;
			if (data & MCR_OP2_MASK)
				par |= MSR_CTS_MASK;

			changeMSR_Loopback (par);

			// go back to empty state
			LSR &= (LSR_TX_EMPTY_MASK | LSR_TX_HOLDING_EMPTY_MASK);

		} else {
			MCR = data;
			updateModemControlLines ();
			// loopback is off
		}
	}
	MCR = data;
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

#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Read from LSR value 0x%x", base, retval);
#endif
	return retval;
}

void CSerial::Write_MSR (Bit8u val) {
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Write to MSR, value 0x%x", base, val);
#endif
	MSR &= MSR_LINE_MASK;
	MSR |= val & MSR_delta_MASK;
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
	Bit8u retval;
	if (!(MCR & MCR_LOOPBACK_Enable_MASK)) {
		updateMSR ();
	}
	retval = MSR;
	clear (MSR_PRIORITY);
	// clear deltas
	MSR &= MSR_LINE_MASK;
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Read from MSR value 0x%x", base, retval);
#endif
	return retval;
}

/*****************************************************************************/
/* Scratchpad Register (r/w)                                                **/
/*****************************************************************************/
// Just a memory register. Not much to do here.
Bitu CSerial::Read_SPR () {
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Read from SPR value 0x%x", base, SPR);
#endif
	return SPR;
}

void CSerial::Write_SPR (Bit8u data) {
#ifdef SERIALPORT_DEBUGMSG
	LOG_UART ("Serial port at %x: Write to SPR value 0x%x", base, data);
#endif
	SPR = data;
}

/*****************************************************************************/
/* Write_reserved                                                           **/
/*****************************************************************************/
void CSerial::Write_reserved (Bit8u data, Bit8u address) {
	LOG_UART("Serial port at %x: Write to reserved register, value 0x%x, register %x", base, data, address);
}

/*****************************************************************************/
/* Loopback: add byte to loopback buffer; it is received next time Timer    **/
/* function is invoked; (time needed is not emulated correctly, but I don't **/
/* think this is sooooo important....)                                      **/
/*****************************************************************************/
void CSerial::transmitLoopbackByte (Bit8u val) {
	//LOG_MSG("Serial port at %x: Loopback requested", base);
	loopback_pending = true;
	loopback_data = val;
}

/*****************************************************************************/
/* MCR Access: returns cirquit state as boolean.                            **/
/*****************************************************************************/
bool CSerial::getDTR () {
	return (MCR & MCR_DTR_MASK) != 0;
}

bool CSerial::getRTS () {
	return (MCR & MCR_RTS_MASK) != 0;
}

/*****************************************************************************/
/* MSR Access                                                               **/
/*****************************************************************************/
bool CSerial::getRI () {
	return (MSR & MSR_RI_MASK) != 0;
}

bool CSerial::getCD () {
	return (MSR & MSR_CD_MASK) != 0;
}

bool CSerial::getDSR () {
	return (MSR & MSR_DSR_MASK) != 0;
}

bool CSerial::getCTS () {
	return (MSR & MSR_CTS_MASK) != 0;
}

// these give errors if invoked while loopback mode... but who cares.
void CSerial::setRI (bool value) {
	bool compare = ((MSR & MSR_RI_MASK) != 0);
	if (value != compare) {
		if (value)
			MSR |= MSR_RI_MASK;
		else
			MSR &= (~MSR_RI_MASK);
		MSR |= MSR_dRI_MASK;
		rise (MSR_PRIORITY);
	}
	//else no change
}
void CSerial::setDSR (bool value) {
	bool compare = ((MSR & MSR_DSR_MASK) != 0);
	if (value != compare) {
		if (value)
			MSR |= MSR_DSR_MASK;
		else
			MSR &= (~MSR_DSR_MASK);
		MSR |= MSR_dDSR_MASK;
		rise (MSR_PRIORITY);
	}
	//else no change
}
void CSerial::setCD (bool value) {
	bool compare = ((MSR & MSR_CD_MASK) != 0);
	if (value != compare) {
		if (value)
			MSR |= MSR_CD_MASK;
		else
			MSR &= (~MSR_CD_MASK);
		MSR |= MSR_dCD_MASK;
		rise (MSR_PRIORITY);
	}
	//else no change
}
void CSerial::setCTS (bool value) {
	bool compare = ((MSR & MSR_CTS_MASK) != 0);
	if (value != compare) {
		if (value)
			MSR |= MSR_CTS_MASK;
		else
			MSR &= (~MSR_CTS_MASK);
		MSR |= MSR_dCTS_MASK;
		rise (MSR_PRIORITY);
	}
	//else no change
}

/*****************************************************************************/
/* Initialisation                                                           **/
/*****************************************************************************/
void CSerial::Init_Registers (Bit32u initbps, Bit8u bytesize, 
                              const char *parity, Bit8u stopbits) {
	Bit8u lcrresult = 0;
	Bit16u baudresult = 0;

	RHR = 0;
	THR = 0;
	IER = 0;
	ISR = 0x1;
	LCR = 0;
	MCR = 0;
	LSR = 0x60;
	MSR = 0;

	SPR = 0xFF;

	DLL = 0x0;
	DLM = 0x0;

	pending_interrupts = 0x0;
	current_priority = 0x0;
	waiting_interrupts = 0x0;
	loopback_pending = false;


	// make lcr: byte size, parity, stopbits, baudrate

	if (bytesize == 5)
		lcrresult |= LCR_DATABITS_5;
	else if (bytesize == 6)
		lcrresult |= LCR_DATABITS_6;
	else if (bytesize == 7)
		lcrresult |= LCR_DATABITS_7;
	else
		lcrresult |= LCR_DATABITS_8;

	if (parity[0] == 'O' || parity[0] == 'o')
		lcrresult |= LCR_PARITY_ODD;
	else if (parity[0] == 'E' || parity[0] == 'e')
		lcrresult |= LCR_PARITY_EVEN;
	else if (parity[0] == 'M' || parity[0] == 'm')
		lcrresult |= LCR_PARITY_MARK;
	else if (parity[0] == 'S' || parity[0] == 's')
		lcrresult |= LCR_PARITY_SPACE;
	else
		lcrresult |= LCR_PARITY_NONE;

	if (stopbits == 2)
		lcrresult |= LCR_STOPBITS_MORE_THAN_1;
	else
		lcrresult |= LCR_STOPBITS_1;

	// baudrate

	if (initbps > 0)
		baudresult = (Bit16u) (115200 / initbps);
	else
		baudresult = 12;			// = 9600 baud

	Write_LCR (LCR_DIVISOR_Enable_MASK);
	Write_THR ((Bit8u) baudresult & 0xff);
	Write_IER ((Bit8u) (baudresult >> 8));
	Write_LCR (lcrresult);
}

CSerial::CSerial(IO_ReadHandler * rh, IO_WriteHandler * wh, TIMER_TickHandler,
                 Bit16u initbase, Bit8u initirq, Bit32u initbps, Bit8u bytesize, 
                 const char *parity, Bit8u stopbits) {
	base = initbase;
	irq = initirq;
	TimerHnd = NULL;
	//TimerHnd = th;				// for destructor
	//TIMER_AddTickHandler(TimerHnd);

	for (Bitu i = 0; i <= 7; i++) {
		WriteHandler[i].Install (i + base, wh, IO_MB);
		ReadHandler[i].Install (i + base, rh, IO_MB);
	}
};

CSerial::~CSerial(void) {
	
	if(TimerHnd) TIMER_DelTickHandler(TimerHnd);
};

void CSerial::InstallTimerHandler(TIMER_TickHandler th)
{
	if(TimerHnd==NULL) {
		TimerHnd=th;	
		TIMER_AddTickHandler(th);
	}
}

bool getParameter(char *input, char *buffer, const char *parametername,
                  Bitu buffersize) {
	Bitu outputPos = 0;
	Bitu currentState = 0;				// 0 = before '=' 1 = after '=' 2 = in word
	Bitu startInputPos;
	Bitu inputPos;
	char *res1 = strstr(input, parametername);
	if (res1 == 0)
		return false;
	inputPos = res1 - input;
	inputPos += strlen (parametername);
	startInputPos = inputPos;
	while (input[inputPos] != 0 && outputPos + 2 < buffersize) {
		if (currentState == 0) {
			if (input[inputPos] == ' ')
				inputPos++;
			else if (input[inputPos] == ':') {
				currentState = 1;
				inputPos++;
			} else
				return false;
		} else if (currentState == 1) {
			if (input[inputPos] == ' ')
				inputPos++;
			else {
				currentState = 2;
				buffer[outputPos] = input[inputPos];
				outputPos++;
				inputPos++;
			}

		} else {
			if (input[inputPos] == ' ') {
				buffer[outputPos] = 0;
				return true;
			} else {
				buffer[outputPos] = input[inputPos];
				outputPos++;
				inputPos++;
			}
		}
	}
	buffer[outputPos] = 0;
	if (inputPos == startInputPos)
		return false;
	return true;
}

// functions for parsing the config line
bool scanNumber (char *scan, Bitu * retval) {
	*retval = 0;

	while (char c = *scan) {
		if (c >= '0' && c <= '9') {
			*retval *= 10;
			*retval += c - '0';
			scan++;
		} else
			return false;
	}
	return true;
}

bool getFirstWord (char *input, char *buffer, Bitu buffersize) {
	Bitu outputPointer = 0;
	Bitu currentState = 0;				// 0 = scanning spaces 1 = in word
	Bitu currentPos = 0;
	while (input[currentPos] != 0 && outputPointer + 2 < buffersize) {
		if (currentState == 0) {
			if (input[currentPos] != ' ') {
				currentState = 1;
				buffer[outputPointer] = input[currentPos];
				outputPointer++;
			}
		} else {
			if (input[currentPos] == ' ') {
				buffer[outputPointer] = 0;
				return true;
			} else {
				buffer[outputPointer] = input[currentPos];
				outputPointer++;
			}
		}
		currentPos++;
	}
	buffer[outputPointer] = 0;		// end of string
	if (currentState == 0)
		return false;
	else
		return true;
}

void BIOS_SetComPorts (Bit16u baseaddr[]);

class SERIALPORTS:public Module_base {
public:
	CSerial ** serialPortObjects[4];
	SERIALPORTS (Section * configuration):Module_base (configuration) {
		// put handlers into arrays
		IO_ReadHandler *serialReadHandlers[] = {
			&SERIAL1_Read, &SERIAL2_Read, &SERIAL3_Read, &SERIAL4_Read
		};
		IO_WriteHandler *serialWriteHandlers[] = {
			&SERIAL1_Write, &SERIAL2_Write, &SERIAL3_Write, &SERIAL4_Write
		};
		TIMER_TickHandler serialTimerHandlers[] = {
			&SERIAL1_Update, &SERIAL2_Update, &SERIAL3_Update, &SERIAL4_Update
		};

		// default ports & interrupts
		Bit16u addresses[] = { COM1_BASE, COM2_BASE, COM3_BASE, COM4_BASE };
		Bit8u defaultirq[] = { 4, 3, 4, 3 };
		Bit16u biosParameter[4] = { 0, 0, 0, 0 };
		Section_prop *section = static_cast <Section_prop*>(configuration);
		char tmpbuffer[15];

		const char *configstringsconst[4] = {
			section->Get_string ("serial1"),
			section->Get_string ("serial2"),
			section->Get_string ("serial3"),
			section->Get_string ("serial4")
		};
		/* Create copies so they can be modified */
		char *configstrings[4] = { 0 };
		for(Bitu i = 0;i < 4;i++) {
			size_t len = strlen(configstringsconst[i]);
			configstrings[i] = new char[len+1];
			strcpy(configstrings[i],configstringsconst[i]);
			configstrings[i] = upcase(configstrings[i]);
		}
	
		serialPortObjects[0] = &serial1;
		serialPortObjects[1] = &serial2;
		serialPortObjects[2] = &serial3;
		serialPortObjects[3] = &serial4;

		// iterate through all 4 com ports
		for (Bitu i = 0; i < 4; i++) {
			Bit16u baseAddress = addresses[i];
			Bitu irq = defaultirq[i];
			Bitu bps = 9600;
			Bitu bytesize = 8;
			Bitu stopbits = 1;
			char parity = 'N';
			biosParameter[i] = baseAddress;

			// parameter: irq
			if (getParameter(configstrings[i], tmpbuffer, "IRQ", sizeof (tmpbuffer))) {
				if (scanNumber (tmpbuffer, &irq)) {
					if (irq < 0 || irq == 2 || irq > 15)
						irq = defaultirq[i];
				} else
					irq = defaultirq[i];
			}
			// parameter: bps
			if (getParameter(configstrings[i], tmpbuffer, "STARTBPS", sizeof (tmpbuffer))) {
				if (scanNumber (tmpbuffer, &bps)) {
					if (bps <= 0)
						bps = 9600;
				} else
					bps = 9600;
			}
			// parameter: bytesize
			if (getParameter(configstrings[i], tmpbuffer, "BYTESIZE", sizeof (tmpbuffer))) {
				if (scanNumber (tmpbuffer, &bytesize)) {
					if (bytesize < 5 || bytesize > 8)
						bytesize = 8;
				} else
					bytesize = 8;
			}
			// parameter: stopbits
			if (getParameter(configstrings[i], tmpbuffer, "STOPBITS", sizeof (tmpbuffer))) {
				if (scanNumber (tmpbuffer, &stopbits)) {
					if (stopbits < 1 || stopbits > 2)
						stopbits = 1;
				} else
					stopbits = 1;
			}
			// parameter: parity
			if (getParameter(configstrings[i], tmpbuffer, "PARITY", sizeof (tmpbuffer))) {
				if (!(tmpbuffer[0] == 'N' || tmpbuffer[0] == 'O' || tmpbuffer[0] == 'E'
				    || tmpbuffer[0] == 'M' || tmpbuffer[0] == 'S'))
					tmpbuffer[0] = 'N';
				parity = tmpbuffer[0];
			}
			//LOG_MSG("COM%d: %s",i+1,configstrings[i]);
			if (getFirstWord (configstrings[i], tmpbuffer, sizeof (tmpbuffer))) {
				//LOG_MSG("COM%d: %s",i+1,tmpbuffer);
				if (!strcmp (tmpbuffer, "DUMMY")) {
					*serialPortObjects[i] = new CSerialDummy (serialReadHandlers[i], serialWriteHandlers[i],serialTimerHandlers[i], baseAddress, irq, bps,bytesize, &parity, stopbits);
				}
#ifdef DIRECTSERIAL_AVAILIBLE
				else if (!strcmp (tmpbuffer, "DIRECTSERIAL")) {
					// parameter: realport
					if (getParameter (configstrings[i], tmpbuffer, "REALPORT", sizeof (tmpbuffer))) {	// realport is required
						CDirectSerial *cdstemp = new CDirectSerial (serialReadHandlers[i], serialWriteHandlers[i], serialTimerHandlers[i], baseAddress, irq, bps,bytesize, &parity,stopbits,tmpbuffer);

						if (cdstemp->InstallationSuccessful) {
							*serialPortObjects[i] = cdstemp;
						} else {	// serial port name was wrong or already in use
							delete cdstemp;
							*serialPortObjects[i] = NULL;
							biosParameter[i] = 0;
						}

					} else {
						*serialPortObjects[i] = NULL;
						biosParameter[i] = 0;
					}
				}
#endif
#if C_MODEM
				else if (!strcmp (tmpbuffer, "MODEM")) {
					Bitu listenport = 23;
					// parameter: listenport
					if (getParameter(configstrings[i], tmpbuffer, "LISTENPORT", sizeof (tmpbuffer))) {
						if (scanNumber (tmpbuffer, &listenport)) {
							if (listenport <= 0 || listenport > 65535)
								listenport = 23;
						} else
							listenport = 23;
					}

					*serialPortObjects[i] = new CSerialModem (serialReadHandlers[i], serialWriteHandlers[i], serialTimerHandlers[i], baseAddress, irq, bps,bytesize, &parity, stopbits, 0, listenport);
				}
#endif
				else if (!strcmp (tmpbuffer, "DISABLED")) {
					*serialPortObjects[i] = NULL;
					biosParameter[i] = 0;
				} else {
					LOG_MSG ("Invalid type for COM%d.", i + 1);
					*serialPortObjects[i] = NULL;
					biosParameter[i] = 0;
				}
			} else {
				*serialPortObjects[i] = NULL;
				biosParameter[i] = 0;
			}
		}

		delete [] configstrings[0];delete [] configstrings[1];
		delete [] configstrings[2];delete [] configstrings[3];
		BIOS_SetComPorts (biosParameter);
	}

	~SERIALPORTS () {
		for (Bitu i = 0; i < 4; i++)
			if (*serialPortObjects[i]) {
				delete *(serialPortObjects[i]);
				*(serialPortObjects[i]) = 0;
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
