/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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

/* $Id: serialport.h,v 1.12 2006-02-09 11:47:48 qbix79 Exp $ */

#ifndef DOSBOX_SERIALPORT_H
#define DOSBOX_SERIALPORT_H

// Uncomment this for a lot of debug messages:
// #define SERIALPORT_DEBUGMSG 

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_INOUT_H
#include "inout.h"
#endif
#ifndef DOSBOX_TIMER_H
#include "timer.h"
#endif


// Serial port interface //

class CSerial {
public:

	// Constructor takes base port (0x3f8, 0x2f8, 0x2e8, etc.), IRQ, and initial bps //
	CSerial(IO_ReadHandler* rh, IO_WriteHandler* wh,
			TIMER_TickHandler TimerHandler, 
			Bit16u initbase, Bit8u initirq, Bit32u initbps,
			Bit8u bytesize, const char* parity, Bit8u stopbits);
	
	TIMER_TickHandler TimerHnd;
	virtual ~CSerial();
	void InstallTimerHandler(TIMER_TickHandler);
	
	IO_ReadHandleObject ReadHandler[8];
	IO_WriteHandleObject WriteHandler[8];

	void Timer(void);
	virtual void Timer2(void)=0;
	
	Bitu base;
	Bitu irq;
	
	bool getDTR();
	bool getRTS();

	bool getRI();
	bool getCD();
	bool getDSR();
	bool getCTS();

	void setRI(bool value);
	void setDSR(bool value);
	void setCD(bool value);
	void setCTS(bool value);

	void Write_THR(Bit8u data);
	Bitu Read_RHR();
	Bitu Read_IER();
	void Write_IER(Bit8u data);
	Bitu Read_ISR();
	Bitu Read_LCR();
	void Write_LCR(Bit8u data);
	Bitu Read_MCR();
	void Write_MCR(Bit8u data);
	Bitu Read_LSR();
	
	// Really old hardware seems to have the delta part of this register writable
	void Write_MSR(Bit8u data);

	Bitu Read_MSR();
	Bitu Read_SPR();
	void Write_SPR(Bit8u data);
	void Write_reserved(Bit8u data, Bit8u address);
	
	// If a byte comes from wherever(loopback or real port or maybe
	// that softmodem thingy), put it in here.
	void receiveByte(Bit8u data);

	// If an error was received, put it here (in LSR register format)
	void receiveError(Bit8u errorword);

	// connected device checks, if port can receive data:
	bool CanReceiveByte();
	
	// When done sending, notify here
	void ByteTransmitted();

	// Virtual app has read the received data
	virtual void RXBufferEmpty()=0;

	// real transmit
	virtual void transmitByte(Bit8u val)=0;

	// switch break state to the passed value
	virtual void setBreak(bool value)=0;
	
	// set output lines
	virtual void updateModemControlLines(/*Bit8u mcr*/)=0;
	
	// change baudrate, number of bits, parity, word length al at once
	virtual void updatePortConfig(Bit8u dll, Bit8u dlm, Bit8u lcr)=0;
	
	// CSerial requests an update of the input lines
	virtual void updateMSR()=0;

	// after update request, or some "real" changes, 
	// modify MSR here
	void changeMSR(Bit8u data);	// make public

	void Init_Registers(Bit32u initbps,
	                             Bit8u bytesize, const char* parity, Bit8u stopbits);

private:

	// I used this spec: http://www.exar.com/products/st16c450v420.pdf

	void changeMSR_Loopback(Bit8u data);
	
	void WriteRealIER(Bit8u data);
	// reason for an interrupt has occured - functions triggers interrupt
	// if it is enabled and no higher-priority irq pending
	void rise(Bit8u priority);

	// clears the pending interrupt
	void clear(Bit8u priority);
	
	#define ERROR_PRIORITY 4	// overrun, parity error, frame error, break
	#define RX_PRIORITY 1		// a byte has been received
	#define TX_PRIORITY 2		// tx buffer has become empty
	#define MSR_PRIORITY 8		// CRS, DSR, RI, DCD change 
	#define NONE_PRIORITY 0


	Bit8u pending_interrupts;	// stores triggered interupts
	Bit8u current_priority;
	Bit8u waiting_interrupts;	// these are on, but maybe not enabled
	
	// 16C450 (no FIFO)
	//				read/write		name

	
	Bit8u DLL;	//	r				Baudrate divider low byte
	Bit8u DLM;	//	r				"" high byte

	Bit8u RHR;	//	r				Receive Holding Register, also LSB of Divisor Latch (r/w)
	#define RHR_OFFSET 0
				// Data: whole byte

	Bit8u THR;	//	w				Transmit Holding Register
	#define THR_OFFSET 0
				// Data: whole byte

	Bit8u IER;	//	r/w				Interrupt Enable Register, also MSB of Divisor Latch (r/w)
	#define IER_OFFSET 1
				// Data:
				// bit0				receive holding register
				// bit1				transmit holding register
				// bit2				receive line status interrupt
				// bit3				modem status interrupt
				
	#define RHR_INT_Enable_MASK				0x1
	#define THR_INT_Enable_MASK				0x2
	#define Receive_Line_INT_Enable_MASK	0x4
	#define Modem_Status_INT_Enable_MASK	0x8

	Bit8u ISR;	//	r				Interrupt Status Register
	#define ISR_OFFSET 2

	#define ISR_CLEAR_VAL 0x1
	#define ISR_ERROR_VAL 0x6
	#define ISR_RX_VAL 0x4
	#define ISR_TX_VAL 0x2
	#define ISR_MSR_VAL 0x0
public:	
	Bit8u LCR;	//	r/w				Line Control Register
private:
	#define LCR_OFFSET 3
						// bit0: word length bit0
						// bit1: word length bit1
						// bit2: stop bits
						// bit3: parity enable
						// bit4: even parity
						// bit5: set parity
						// bit6: set break
						// bit7: divisor latch enable

	
	#define	LCR_BREAK_MASK 0x40
	#define LCR_DIVISOR_Enable_MASK 0x80
	#define LCR_PORTCONFIG_MASK 0x3F
	
	#define LCR_PARITY_NONE		0x0
	#define LCR_PARITY_ODD		0x8
	#define LCR_PARITY_EVEN		0x18
	#define LCR_PARITY_MARK		0x28
	#define LCR_PARITY_SPACE	0x38

	#define LCR_DATABITS_5		0x0
	#define LCR_DATABITS_6		0x1
	#define LCR_DATABITS_7		0x2
	#define LCR_DATABITS_8		0x3

	#define LCR_STOPBITS_1		0x0
	#define LCR_STOPBITS_MORE_THAN_1 0x4

	Bit8u MCR;	//	r/w				Modem Control Register
	#define MCR_OFFSET 4
						// bit0: DTR
						// bit1: RTS
						// bit2: OP1
						// bit3: OP2
						// bit4: loop back enable

	#define MCR_LOOPBACK_Enable_MASK 0x10					
	#define MCR_LEVELS_MASK 0xf	
	
	#define MCR_DTR_MASK 0x1
	#define MCR_RTS_MASK 0x2	
	#define MCR_OP1_MASK 0x4	
	#define MCR_OP2_MASK 0x8	

	Bit8u LSR;	//	r				Line Status Register
	#define LSR_OFFSET 5

	#define LSR_RX_DATA_READY_MASK 0x1
	#define LSR_OVERRUN_ERROR_MASK 0x2
	#define LSR_PARITY_ERROR_MASK 0x4
	#define LSR_FRAMING_ERROR_MASK 0x8
	#define LSR_RX_BREAK_MASK 0x10
	#define LSR_TX_HOLDING_EMPTY_MASK 0x20
	#define LSR_TX_EMPTY_MASK 0x40

	#define LSR_ERROR_MASK 0x1e


	Bit8u MSR;	//	r				Modem Status Register
	#define MSR_OFFSET 6
						// bit0: deltaCTS
						// bit1: deltaDSR
						// bit2: deltaRI
						// bit3: deltaCD
						// bit4: CTS
						// bit5: DSR
						// bit6: RI
						// bit7: CD
	
	#define MSR_delta_MASK 0xf
	#define MSR_LINE_MASK 0xf0

	#define MSR_dCTS_MASK 0x1
	#define MSR_dDSR_MASK 0x2
	#define MSR_dRI_MASK 0x4
	#define MSR_dCD_MASK 0x8
	#define MSR_CTS_MASK 0x10
	#define MSR_DSR_MASK 0x20
	#define MSR_RI_MASK 0x40
	#define MSR_CD_MASK 0x80

	Bit8u SPR;	//	r/w				Scratchpad Register
	#define SPR_OFFSET 7


	// For loopback purposes...
	bool loopback_pending;
	Bit8u loopback_data;
	void transmitLoopbackByte(Bit8u val);

	// 16C550 (FIFO)
	// TODO
	//Bit8u FCR;	// FIFO Control Register
	
};

#define COM1_BASE 0x3f8
#define COM2_BASE 0x2f8
#define COM3_BASE 0x3e8
#define COM4_BASE 0x2e8

#endif

