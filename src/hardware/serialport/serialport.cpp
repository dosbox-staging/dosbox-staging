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

#include <algorithm>
#include <cassert>
#include <ctype.h>
#include <string.h>
#include <tuple>

#include "inout.h"
#include "pic.h"
#include "setup.h"
#include "bios.h"					// SetComPorts(..)
#include "callback.h"				// CALLBACK_Idle

#include "serialport.h"
#include "directserial.h"
#include "serialdummy.h"
#include "softmodem.h"
#include "nullmodem.h"

#include "cpu.h"

#define LOG_SER(x) log_ser

bool device_COM::Read(uint8_t *data, uint16_t *size)
{
	// DTR + RTS on
	sclass->Write_MCR(0x03);
	for (uint16_t i = 0; i < *size; i++) {
		uint8_t status;
		if (!(sclass->Getchar(&data[i], &status, true, 1000))) {
			*size=i;
			return true;
		}
	}
	return true;
}

bool device_COM::Write(uint8_t *data, uint16_t *size)
{
	// DTR + RTS on
	sclass->Write_MCR(0x03);
	for (uint16_t i = 0; i < *size; i++) {
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

bool device_COM::Seek(uint32_t *pos, uint32_t type)
{
	(void)type; // unused, but required for API compliance
	*pos = 0;
	return true;
}

bool device_COM::Close() {
	return false;
}

uint16_t device_COM::GetInformation()
{
	return 0x80A0;
}

device_COM::device_COM(class CSerial* sc) {
	sclass = sc;
	SetName(serial_comname[sclass->port_index]);
}

device_COM::~device_COM() {
}

// COM1 - COM4 objects
CSerial *serialports[SERIAL_MAX_PORTS] = {nullptr};

static Bitu SERIAL_Read (Bitu port, Bitu iolen) {
	(void)iolen; // unused, but required for API compliance
	uint32_t i = 0;
	uint32_t retval = 0;
	uint8_t offset_type = static_cast<uint8_t>(port) & 0x7;
	switch(port&0xff8) {
		case 0x3f8: i=0; break;
		case 0x2f8: i=1; break;
		case 0x3e8: i=2; break;
		case 0x2e8: i=3; break;
		default: return 0xff;
	}
	if (!serialports[i])
		return 0xff;

	switch (offset_type) {
	case RHR_OFFSET: retval = serialports[i]->Read_RHR(); break;
	case IER_OFFSET: retval = serialports[i]->Read_IER(); break;
	case ISR_OFFSET: retval = serialports[i]->Read_ISR(); break;
	case LCR_OFFSET: retval = serialports[i]->Read_LCR(); break;
	case MCR_OFFSET: retval = serialports[i]->Read_MCR(); break;
	case LSR_OFFSET: retval = serialports[i]->Read_LSR(); break;
	case MSR_OFFSET: retval = serialports[i]->Read_MSR(); break;
	case SPR_OFFSET: retval = serialports[i]->Read_SPR(); break;
	}

#if SERIAL_DEBUG
	const char* const dbgtext[]=
		{"RHR","IER","ISR","LCR","MCR","LSR","MSR","SPR","DLL","DLM"};
	if(serialports[i]->dbg_register) {
		if ((offset_type < 2) &&
		    ((serialports[i]->LCR) & LCR_DIVISOR_Enable_MASK))
			offset_type += 8;
		serialports[i]->log_ser(serialports[i]->dbg_register,
		                        "read  0x%2x from %s.", retval,
		                        dbgtext[offset_type]);
	}
#endif
	return static_cast<Bitu>(retval);
}
static void SERIAL_Write(Bitu port, uint8_t val, Bitu)
{
	uint32_t i;
	const uint8_t offset_type = static_cast<uint8_t>(port) & 0x7;
	switch(port&0xff8) {
		case 0x3f8: i=0; break;
		case 0x2f8: i=1; break;
		case 0x3e8: i=2; break;
		case 0x2e8: i=3; break;
		default: return;
	}
	if(serialports[i]==0) return;
	
#if SERIAL_DEBUG
		const char* const dbgtext[]={"THR","IER","FCR",
			"LCR","MCR","!LSR","MSR","SPR","DLL","DLM"};
		if(serialports[i]->dbg_register) {
			uint32_t debugindex = offset_type;
			if ((offset_type < 2) &&
			    ((serialports[i]->LCR) & LCR_DIVISOR_Enable_MASK)) {
				debugindex += 8;
			}
			serialports[i]->log_ser(serialports[i]->dbg_register,
			                        "write 0x%2x to %s.", val,
			                        dbgtext[debugindex]);
	        }
#endif
	        switch (offset_type) {
	        case THR_OFFSET: serialports[i]->Write_THR(val); return;
	        case IER_OFFSET: serialports[i]->Write_IER(val); return;
	        case FCR_OFFSET: serialports[i]->Write_FCR(val); return;
	        case LCR_OFFSET: serialports[i]->Write_LCR(val); return;
	        case MCR_OFFSET: serialports[i]->Write_MCR(val); return;
	        case MSR_OFFSET: serialports[i]->Write_MSR(val); return;
	        case SPR_OFFSET:
			serialports[i]->Write_SPR (val);
			return;
		default:
			serialports[i]->Write_reserved (val, port & 0x7);
	        }
}
#if SERIAL_DEBUG
void CSerial::log_ser(bool active, char const* format,...) {
	if(active) {
		// copied from DEBUG_SHOWMSG
		char buf[512];
		buf[0]=0;
		sprintf(buf,"%12.3f [% 7u] ",PIC_FullIndex(), SDL_GetTicks());
		va_list msg;
		va_start(msg,format);
		vsprintf(buf+strlen(buf),format,msg);
		va_end(msg);
		// Add newline if not present
		const uint32_t len = strlen(buf);
		if(buf[len-1]!='\n') strcat(buf,"\r\n");
		fputs(buf,debugfp);
	}
}
#endif

void CSerial::changeLineProperties() {
	// update the event wait time
	float bitlen;

	if(baud_divider==0) bitlen=(1000.0f/115200.0f);
	else bitlen = (1000.0f/115200.0f)*(float)baud_divider;
	bytetime=bitlen*(float)(1+5+1);		// startbit + minimum length + stopbit
	bytetime+= bitlen*(float)(LCR&0x3); // databits
	if(LCR&0x4) bytetime+=bitlen;		// 2nd stopbit
	if(LCR&0x8) bytetime+=bitlen;		// parity

#if SERIAL_DEBUG
	const char* const dbgtext[]={"none","odd","none","even","none","mark","none","space"};
	log_ser(dbg_serialtraffic,"New COM parameters: baudrate %5.0f, parity %s, wordlen %d, stopbits %d",
		1.0/bitlen*1000.0f,dbgtext[(LCR&0x38)>>3],(LCR&0x3)+5,((LCR&0x4)>>2)+1);
#endif	
	updatePortConfig (baud_divider, LCR);
}

static void Serial_EventHandler(Bitu val) {
	const uint32_t serclassid = val & 0x3;
	if (serialports[serclassid] != 0) {
		const auto event_type = static_cast<uint16_t>(val >> 2);
		serialports[serclassid]->handleEvent(event_type);
	}
}

void CSerial::setEvent(uint16_t type, float duration)
{
	PIC_AddEvent(Serial_EventHandler, duration,
	             static_cast<Bitu>((type << 2) | port_index));
}

void CSerial::removeEvent(uint16_t type)
{
	// TODO
	PIC_RemoveSpecificEvents(Serial_EventHandler,
	                         static_cast<Bitu>((type << 2) | port_index));
}

void CSerial::handleEvent(uint16_t type)
{
	switch (type) { 
	case SERIAL_TX_LOOPBACK_EVENT: 
#if SERIAL_DEBUG
		log_ser(dbg_serialtraffic,
		        loopback_data < 0x10 ? "tx 0x%02x (%" PRIu8 ") (loopback)"
		                             : "tx 0x%02x (%c) (loopback)",
		        loopback_data, loopback_data);
#endif
		receiveByte(loopback_data);
		ByteTransmitted();
		break;

	case SERIAL_THR_LOOPBACK_EVENT:
		loopback_data = txfifo->front();
		ByteTransmitting();
		setEvent(SERIAL_TX_LOOPBACK_EVENT, bytetime);
		break;

	case SERIAL_ERRMSG_EVENT:
		LOG_MSG("SERIAL: Port %" PRIu8 " errors:\n"
		        "  - framing %" PRIu32 "\n"
		        "  - parity %" PRIu32 "\n"
		        "  - RX overruns %" PRIu32 "\n"
		        "  - IF0 overruns: %" PRIu32 "\n"
		        "  - TX overruns: %" PRIu32 "\n"
		        "  - break %" PRIu32,
		        GetPortNumber(), framingErrors, parityErrors,
		        overrunErrors, overrunIF0, txOverrunErrors, breakErrors);
		errormsg_pending = false;
		framingErrors = 0;
		parityErrors = 0;
		overrunErrors = 0;
		txOverrunErrors = 0;
		overrunIF0 = 0;
		breakErrors = 0;
		break;

	case SERIAL_RX_TIMEOUT_EVENT:
		rise(TIMEOUT_PRIORITY);
		break;

	default:
		handleUpperEvent(type);
	}
}

/*****************************************************************************/
/* Interrupt control routines                                               **/
/*****************************************************************************/
void CSerial::rise(uint8_t priority)
{
#if SERIAL_DEBUG
	if(priority&TX_PRIORITY && !(waiting_interrupts&TX_PRIORITY))
		log_ser(dbg_interrupt,"tx interrupt on.");
	if(priority&RX_PRIORITY && !(waiting_interrupts&RX_PRIORITY))
		log_ser(dbg_interrupt,"rx interrupt on.");
	if(priority&MSR_PRIORITY && !(waiting_interrupts&MSR_PRIORITY))
		log_ser(dbg_interrupt,"msr interrupt on.");
	if(priority&TIMEOUT_PRIORITY && !(waiting_interrupts&TIMEOUT_PRIORITY))
		log_ser(dbg_interrupt,"fifo rx timeout interrupt on.");
#endif
	
	waiting_interrupts |= priority;
	ComputeInterrupts();
}

// clears the pending interrupt, triggers other waiting interrupt
void CSerial::clear(uint8_t priority)
{
#if SERIAL_DEBUG
	if(priority&TX_PRIORITY && (waiting_interrupts&TX_PRIORITY))
		log_ser(dbg_interrupt,"tx interrupt off.");
	if(priority&RX_PRIORITY && (waiting_interrupts&RX_PRIORITY))
		log_ser(dbg_interrupt,"rx interrupt off.");
	if(priority&MSR_PRIORITY && (waiting_interrupts&MSR_PRIORITY))
		log_ser(dbg_interrupt,"msr interrupt off.");
	if(priority&ERROR_PRIORITY && (waiting_interrupts&ERROR_PRIORITY))
		log_ser(dbg_interrupt,"error interrupt off.");
#endif
	waiting_interrupts &= (~priority);
	ComputeInterrupts();
}

void CSerial::ComputeInterrupts () {
	const uint32_t val = IER & waiting_interrupts;

	if (val & ERROR_PRIORITY)			ISR = ISR_ERROR_VAL;
	else if (val & TIMEOUT_PRIORITY)	ISR = ISR_FIFOTIMEOUT_VAL;
	else if (val & RX_PRIORITY)			ISR = ISR_RX_VAL;
	else if (val & TX_PRIORITY)			ISR = ISR_TX_VAL;
	else if (val & MSR_PRIORITY)		ISR = ISR_MSR_VAL;
	else ISR = ISR_CLEAR_VAL;

	if(val && !irq_active) 
	{
		irq_active=true;
		if(op2) {
			PIC_ActivateIRQ(irq);
#if SERIAL_DEBUG
			log_ser(dbg_interrupt,"IRQ%d on.",irq);
#endif
		}
	} else if((!val) && irq_active) {
		irq_active=false;
		if(op2) { 
			PIC_DeActivateIRQ(irq);
#if SERIAL_DEBUG
			log_ser(dbg_interrupt,"IRQ%d off.",irq);
#endif
		}
	}
}

/*****************************************************************************/
/* Can a byte be received?                                                  **/
/*****************************************************************************/
bool CSerial::CanReceiveByte() {
	return !rxfifo->isFull();
}

/*****************************************************************************/
/* A byte was received                                                      **/
/*****************************************************************************/
void CSerial::receiveByteEx(uint8_t data, uint8_t error)
{
#if SERIAL_DEBUG
	log_ser(dbg_serialtraffic,
	        data < 0x10 ? "\t\t\t\trx 0x%02x (%" PRIu8 ")" : "\t\t\t\trx 0x%02x (%c)",
	        data, data);
#endif
	if (!(rxfifo->push(data))) {
		// Overrun error ;o
		error |= LSR_OVERRUN_ERROR_MASK;
	}
	removeEvent(SERIAL_RX_TIMEOUT_EVENT);
	if (rxfifo->numQueued() == rx_interrupt_threshold)
		rise(RX_PRIORITY);
	else
		setEvent(SERIAL_RX_TIMEOUT_EVENT, bytetime * 4.0f);

	if(error) {
		// A lot of UART chips generate a framing error too when receiving break
		if(error&LSR_RX_BREAK_MASK) error |= LSR_FRAMING_ERROR_MASK;
#if SERIAL_DEBUG
		log_ser(dbg_serialtraffic,"with error: framing=%d,overrun=%d,break=%d,parity=%d",
			(error&LSR_FRAMING_ERROR_MASK)>0,(error&LSR_OVERRUN_ERROR_MASK)>0,
			(error&LSR_RX_BREAK_MASK)>0,(error&LSR_PARITY_ERROR_MASK)>0);
#endif
		if(FCR&FCR_ACTIVATE) {
			// error and FIFO active
			if(!errorfifo->isFull()) {
				errors_in_fifo++;
				errorfifo->push(error);
			}
			else {
				uint8_t toperror = errorfifo->back();
				if(!toperror) errors_in_fifo++;
				errorfifo->push(error | toperror);
			}
			if (errorfifo->front()) {
				// the next byte in the error fifo has an error
				rise (ERROR_PRIORITY);
				LSR |= error;
			}
		} else {
			// error and FIFO inactive
			rise (ERROR_PRIORITY);
			LSR |= error;
		};
        if(error&LSR_PARITY_ERROR_MASK) {
			parityErrors++;
		};
		if(error&LSR_OVERRUN_ERROR_MASK) {
			overrunErrors++;
			if(!GETFLAG(IF)) overrunIF0++;
#if SERIAL_DEBUG
			log_ser(dbg_serialtraffic,"rx overrun (IF=%d)", GETFLAG(IF)>0);
#endif
		};
		if(error&LSR_FRAMING_ERROR_MASK) {
			framingErrors++;
		}
		if(error&LSR_RX_BREAK_MASK) {
			breakErrors++;
		}
		// trigger status window error notification
		if(!errormsg_pending) {
			errormsg_pending=true;
			setEvent(SERIAL_ERRMSG_EVENT,1000);
		}
	} else {
		// no error
		if(FCR&FCR_ACTIVATE) {
			errorfifo->push(error);
		}
	}
}

void CSerial::receiveByte(uint8_t data)
{
	receiveByteEx(data, 0);
}

/*****************************************************************************/
/* ByteTransmitting: Byte has made it from THR to TX.                       **/
/*****************************************************************************/
void CSerial::ByteTransmitting() {
	if(sync_guardtime) {
		// LOG_MSG("SERIAL: Port %" PRIu8 " byte transmitting after guard.",
		//         GetPortNumber());
		// if(txfifo->isEmpty())
		//	LOG_MSG("SERIAL: Port %" PRIu8 " FIFO empty when it should not",
		//	        GetPortNumber());
		sync_guardtime=false;
		txfifo->pop();
	}
	// else
	// 	LOG_MSG("SERIAL: Port %" PRIu8 " byte transmitting.",
	// 	        GetPortNumber());
	if(txfifo->isEmpty())rise (TX_PRIORITY);
}

/*****************************************************************************/
/* ByteTransmitted: When a byte was sent, notify here.                      **/
/*****************************************************************************/
void CSerial::ByteTransmitted () {
	if(!txfifo->isEmpty()) {
		// there is more data
		uint8_t data = txfifo->pop();
#if SERIAL_DEBUG
		log_ser(dbg_serialtraffic,data<0x10?
			"\t\t\t\t\ttx 0x%02x (%" PRIu8 ") (from buffer)":
			"\t\t\t\t\ttx 0x%02x (%c) (from buffer)", data, data);
#endif
		if (loopback) setEvent(SERIAL_TX_LOOPBACK_EVENT, bytetime);
		else transmitByte(data,false);
		if(txfifo->isEmpty())rise (TX_PRIORITY);

	} else {
#if SERIAL_DEBUG
		log_ser(dbg_serialtraffic,"tx buffer empty.");
#endif
		LSR |= LSR_TX_EMPTY_MASK;
	}
}

/*****************************************************************************/
/* Transmit Holding Register, also LSB of Divisor Latch (r/w)               **/
/*****************************************************************************/
void CSerial::Write_THR(uint8_t data)
{
	// 0-7 transmit data

	if ((LCR & LCR_DIVISOR_Enable_MASK)) {
		// write to DLL
		baud_divider&=0xFF00;
		baud_divider |= data;
		changeLineProperties();
	} else {
		// write to THR
        clear (TX_PRIORITY);

		if((LSR & LSR_TX_EMPTY_MASK))
		{	/* we were idle before
			LOG_MSG("SERIAL: Port %" PRIu8 " starting new transmit cycle",
			        GetPortNumber());
			if (sync_guardtime)
				LOG_MSG("SERIAL: Port %" PRIu8 " internal error 1",
				        GetPortNumber());
			if (!(LSR & LSR_TX_EMPTY_MASK))
				LOG_MSG("SERIAL: Port %" PRIu8 " internal error 2",
				        GetPortNumber());
			if (txfifo->isUsed())
				LOG_MSG("SERIAL: Port %" PRIu8 " internal error 3",
				        GetPortNumber());
			*/

			// need "warming up" time
			sync_guardtime=true;
			// block the fifo so it returns THR full (or not in case of FIFO on)
			txfifo->push(data);
			// transmit shift register is busy
			LSR &= (~LSR_TX_EMPTY_MASK);
			if(loopback) setEvent(SERIAL_THR_LOOPBACK_EVENT, bytetime/10);
			else {
#if SERIAL_DEBUG
				log_ser(dbg_serialtraffic,
				        data < 0x10 ? "\t\t\t\t\ttx 0x%02x (%" PRIu8
				                      ") [FIFO=%2zu]"
				                    : "\t\t\t\t\ttx 0x%02x (%c) [FIFO=%2zu]",
				        data, data, txfifo->numQueued());
#endif
				transmitByte (data,true);
			}
		} else {
			//  shift register is transmitting
			if (!txfifo->push(data)) {
				// TX overflow
#if SERIAL_DEBUG
				log_ser(dbg_serialtraffic,"tx overflow");
#endif
				txOverrunErrors++;
				if(!errormsg_pending) {
					errormsg_pending=true;
					setEvent(SERIAL_ERRMSG_EVENT,1000);
				}
			}
		}
	}
}

/*****************************************************************************/
/* Receive Holding Register, also LSB of Divisor Latch (r/w)                **/
/*****************************************************************************/
uint32_t CSerial::Read_RHR()
{
	// 0-7 received data
	if ((LCR & LCR_DIVISOR_Enable_MASK)) return baud_divider&0xff;
	else {
		uint8_t data = rxfifo->pop();
		if(FCR&FCR_ACTIVATE) {
			uint8_t error = errorfifo->pop();
			if(error) errors_in_fifo--;
			// new error
			if(!rxfifo->isEmpty()) {
				error = errorfifo->front();
				if(error) {
					LSR |= error;
					rise(ERROR_PRIORITY);
				}
			}
		}
		// Reading RHR resets the FIFO timeout
		clear (TIMEOUT_PRIORITY);
		// RX int. is cleared if the buffer holds less data than the threshold
		if (rxfifo->numQueued() < rx_interrupt_threshold)
			clear(RX_PRIORITY);
		removeEvent(SERIAL_RX_TIMEOUT_EVENT);
		if(!rxfifo->isEmpty()) setEvent(SERIAL_RX_TIMEOUT_EVENT,bytetime*4.0f);
		return data;
	}
}

/*****************************************************************************/
/* Interrupt Enable Register, also MSB of Divisor Latch (r/w)               **/
/*****************************************************************************/
// Modified by:
// - writing to it.
uint32_t CSerial::Read_IER()
{
	// 0	receive holding register (byte received)
	// 1	transmit holding register (byte sent)
	// 2	receive line status (overrun, parity error, frame error, break)
	// 3	modem status
	// 4-7	0

	if (LCR & LCR_DIVISOR_Enable_MASK) return baud_divider>>8;
	else return IER&0x0f;
}

void CSerial::Write_IER(uint8_t data)
{
	if ((LCR & LCR_DIVISOR_Enable_MASK)) { // write to DLM
		baud_divider&=0xff;
		baud_divider |= ((uint16_t)data) << 8;
		changeLineProperties();
	} else {
		// Retrigger TX interrupt
		if (txfifo->isEmpty()&& (data&TX_PRIORITY))
			waiting_interrupts |= TX_PRIORITY;
		
		IER = data&0xF;
		if((FCR&FCR_ACTIVATE)&&data&RX_PRIORITY) IER |= TIMEOUT_PRIORITY; 
		ComputeInterrupts();
	}
}

/*****************************************************************************/
/* Interrupt Status Register (r)                                            **/
/*****************************************************************************/
// modified by:
// - incoming interrupts
// - loopback mode
uint32_t CSerial::Read_ISR()
{
	// 0	0:interrupt pending 1: no interrupt
	// 1-3	identification
	//      011 LSR
	//		010 RXRDY
	//		110 RX_TIMEOUT
	//		001 TXRDY
	//		000 MSR
	// 4-7	0

	if(IER&Modem_Status_INT_Enable_MASK) updateMSR();
	uint8_t retval = ISR;

	// clear changes ISR!! mean..
	if(ISR==ISR_TX_VAL) clear(TX_PRIORITY);
	if(FCR&FCR_ACTIVATE) retval |= FIFO_STATUS_ACTIVE;

	return retval;
}

#define BIT_CHANGE_H(oldv,newv,bitmask) (!(oldv&bitmask) && (newv&bitmask))
#define BIT_CHANGE_L(oldv,newv,bitmask) ((oldv&bitmask) && !(newv&bitmask))

void CSerial::Write_FCR(uint8_t data)
{
	if (BIT_CHANGE_H(FCR, data, FCR_ACTIVATE)) {
		// FIFO was switched on
		errors_in_fifo=0; // should already be 0
		errorfifo->setSize(fifo_size);
		rxfifo->setSize(fifo_size);
		txfifo->setSize(fifo_size);
		DEBUG_LOG_MSG("SERIAL: Port %" PRIu8 " %u-byte FIFO enabled",
		              GetPortNumber(), fifo_size);
	} else if (BIT_CHANGE_L(FCR, data, FCR_ACTIVATE)) {
		// FIFO was switched off
		errors_in_fifo=0;
		errorfifo->setSize(1);
		rxfifo->setSize(1);
		txfifo->setSize(1);
		rx_interrupt_threshold=1;
		DEBUG_LOG_MSG("SERIAL: Port %" PRIu8 " FIFO disabled",
		              GetPortNumber());
	}
	FCR=data&0xCF;
	if(FCR&FCR_CLEAR_RX) {
		errors_in_fifo=0;
		errorfifo->clear();
		rxfifo->clear();
	}
	if(FCR&FCR_CLEAR_TX) txfifo->clear();
	if(FCR&FCR_ACTIVATE) {
		switch(FCR>>6) {
			case 0: rx_interrupt_threshold=1; break;
			case 1: rx_interrupt_threshold=4; break;
			case 2: rx_interrupt_threshold=8; break;
			case 3: rx_interrupt_threshold=14; break;
		}
		DEBUG_LOG_MSG("SERIAL: Port %" PRIu8
		              " FIFO interrupting every %u bytes",
		              GetPortNumber(), rx_interrupt_threshold);
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
uint32_t CSerial::Read_LCR()
{
	// 0-1	word length
	// 2	stop bits
	// 3	parity enable
	// 4-5	parity type
	// 6	set break
	// 7	divisor latch enable
	return LCR;
}

void CSerial::Write_LCR(uint8_t data)
{
	uint8_t lcr_old = LCR;
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
		log_ser(dbg_serialtraffic,((LCR & LCR_BREAK_MASK)!=0) ?
			"break on.":"break off.");
#endif
	}
}

/*****************************************************************************/
/* Modem Control Register (r/w)                                             **/
/*****************************************************************************/
// Set levels of RTS and DTR, as well as loopback-mode.
// Modified by: 
// - writing to it.
uint32_t CSerial::Read_MCR()
{
	// 0	-DTR
	// 1	-RTS
	// 2	-OP1
	// 3	-OP2
	// 4	loopback enable
	// 5-7	0
	uint8_t retval = 0;
	if(dtr) retval|=MCR_DTR_MASK;
	if(rts) retval|=MCR_RTS_MASK;
	if(op1) retval|=MCR_OP1_MASK;
	if(op2) retval|=MCR_OP2_MASK;
	if(loopback) retval|=MCR_LOOPBACK_Enable_MASK;
	return retval;
}

void CSerial::Write_MCR(uint8_t data)
{
	// WARNING: At the time setRTSDTR is called rts and dsr members are
	// still wrong.
	if (data & FIFO_FLOWCONTROL)
		LOG_MSG("SERIAL: Port %" PRIu8 " warning, tried to activate hardware "
		        "handshake.",
		        GetPortNumber());
	bool new_dtr = data & MCR_DTR_MASK? true:false;
	bool new_rts = data & MCR_RTS_MASK? true:false;
	bool new_op1 = data & MCR_OP1_MASK? true:false;
	bool new_op2 = data & MCR_OP2_MASK? true:false;
	bool new_loopback = data & MCR_LOOPBACK_Enable_MASK? true:false;
	if (loopback != new_loopback) {
		if (new_loopback) setRTSDTR(false,false);
		else setRTSDTR(new_rts,new_dtr);
	}

	if (new_loopback) {	// is on:
		// DTR->DSR
		// RTS->CTS
		// OP1->RI
		// OP2->CD
		if (new_dtr != dtr && !d_dsr) {
			d_dsr = true;
			rise (MSR_PRIORITY);
		}
		if (new_rts != rts && !d_cts) {
			d_cts = true;
			rise (MSR_PRIORITY);
		}
		if (new_op1 != op1 && !d_ri) {
			// interrupt only at trailing edge
			if (!new_op1) {
				d_ri = true;
				rise (MSR_PRIORITY);
			}
		}
		if (new_op2 != op2 && !d_cd) {
			d_cd = true;
			rise (MSR_PRIORITY);
		}
	} else {
		// loopback is off
		if (new_rts != rts) {
			// RTS difference
			if (new_dtr != dtr) {
				// both difference

#if SERIAL_DEBUG
				log_ser(dbg_modemcontrol,"RTS %x.",new_rts);
				log_ser(dbg_modemcontrol,"DTR %x.",new_dtr);
#endif
				setRTSDTR(new_rts, new_dtr);
			} else {
				// only RTS

#if SERIAL_DEBUG
				log_ser(dbg_modemcontrol,"RTS %x.",new_rts);
#endif
				setRTS(new_rts);
			}
		} else if (new_dtr != dtr) {
			// only DTR
#if SERIAL_DEBUG
				log_ser(dbg_modemcontrol,"%DTR %x.",new_dtr);
#endif
			setDTR(new_dtr);
		}
	}
	// interrupt logic: if new_OP2 is 0, the IRQ line is tristated (pulled high)
	// which turns off the IRQ generation.
	if ((!op2) && new_op2) {
		// irq has been enabled (tristate high -> irq level)
		// Generate one if ComputeInterrupts has set irq_active to true
		if (irq_active) PIC_ActivateIRQ(irq);
	} else if (op2 && (!new_op2)) {
		// irq has been disabled (irq level -> tristate) 
		// Remove the IRQ signal if the irq was being generated before
		if (irq_active) PIC_DeActivateIRQ(irq); 
	}

	dtr=new_dtr;
	rts=new_rts;
	op1=new_op1;
	op2=new_op2;
	loopback=new_loopback;
}

/*****************************************************************************/
/* Line Status Register (r)                                                 **/
/*****************************************************************************/
// errors, tx registers status, rx register status
// modified by:
// - event from real serial port
// - loopback
uint32_t CSerial::Read_LSR()
{
	uint32_t retval = LSR & (LSR_ERROR_MASK | LSR_TX_EMPTY_MASK);
	if (txfifo->isEmpty())
		retval |= LSR_TX_HOLDING_EMPTY_MASK;
	if (!(rxfifo->isEmpty()))
		retval |= LSR_RX_DATA_READY_MASK;
	if (errors_in_fifo)
		retval |= FIFO_ERROR;
	LSR &= (~LSR_ERROR_MASK); // clear error bits on read
	clear(ERROR_PRIORITY);
	return retval;
}

void CSerial::Write_MSR(uint8_t val)
{
	d_cts = (val & MSR_dCTS_MASK) ? true : false;
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
uint32_t CSerial::Read_MSR()
{
	uint8_t retval = 0;

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
uint32_t CSerial::Read_SPR()
{
	return SPR;
}

void CSerial::Write_SPR(uint8_t data)
{
	SPR = data;
}

/*****************************************************************************/
/* Write_reserved                                                           **/
/*****************************************************************************/
void CSerial::Write_reserved(uint8_t data, uint8_t address)
{
	(void)data;    // unused, but required for API compliance
	(void)address; // unused, but required for API compliance
	/*LOG_UART("SERIAL: Port %" PRIu8 " : Write to reserved register, "
	 *         "value 0x%x, register %x.",
	 *         GetPortNumber(), data, address);
	 */
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
		log_ser(dbg_modemcontrol,"%RI  %x.",value);
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
		log_ser(dbg_modemcontrol,"DSR %x.",value);
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
		log_ser(dbg_modemcontrol,"CD  %x.",value);
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
		log_ser(dbg_modemcontrol,"CTS %x.",value);
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

static constexpr std::tuple<uint8_t, uint8_t> baud_to_regs(uint32_t baud_rate)
{
	// Cap the lower-bound to 300 baud. Although the first 1950s modem
	// offered 110 baud, by the time DOS was available 8-bit ISA modems
	// offered at least 300 and even 1200 baud.
	baud_rate = std::max(300u, baud_rate);

	constexpr auto max_baud = 115200u;
	const auto delay_ratio = static_cast<uint16_t>(max_baud / baud_rate);

	const uint8_t transmit_reg = delay_ratio & 0xff;  // bottom byte
	const uint8_t interrupt_reg = delay_ratio >> 8;   // top byte
	return std::make_tuple(transmit_reg, interrupt_reg);
}

static uint8_t line_control_to_reg(const uint8_t data_bits,
                                   const char parity_type,
                                   const uint8_t stop_bits)
{
	uint8_t reg = 0;

	switch (data_bits) {
	case 5: reg |= LCR_DATABITS_5; break;
	case 6: reg |= LCR_DATABITS_6; break;
	case 7: reg |= LCR_DATABITS_7; break;
	default: reg |= LCR_DATABITS_8; break;
	}

	// Explanation of the parity types:
	// - Odd: If an odd number of 1s in the data then parity bit is 1
	// - Even: If an even number of 1s in the data then parity bit is 1
	// - Mark: the parity bit is expected to always be 1
	// - Space: the parity bit is expected to always be 0
	// - None: no parity bit is present or transmitted, which is the default
	assert(isupper(parity_type));
	switch (parity_type) {
	case 'O': reg |= LCR_PARITY_ODD; break;
	case 'E': reg |= LCR_PARITY_EVEN; break;
	case 'M': reg |= LCR_PARITY_MARK; break;
	case 'S': reg |= LCR_PARITY_SPACE; break;
	default: reg |= LCR_PARITY_NONE; break;
	}

	if (stop_bits <= 1)
		reg |= LCR_STOPBITS_1;
	else
		reg |= LCR_STOPBITS_MORE_THAN_1;

	return reg;
}

/*****************************************************************************/
/* Initialisation                                                           **/
/*****************************************************************************/
void CSerial::Init_Registers () {
	// The "power on" settings
	irq_active=false;
	waiting_interrupts = 0x0;

	// Initialize at 9600 baud, 8-N-1 (data, parity, stop)
	const auto line_reg = line_control_to_reg(8, 'N', 1);
	constexpr auto baud_spec = baud_to_regs(9600);
	constexpr uint8_t transmit_reg = std::get<0>(baud_spec);
	constexpr uint8_t interrupt_reg = std::get<1>(baud_spec);

	IER = 0;
	ISR = 0x1;
	LCR = 0;
	//MCR = 0xff;
	loopback = true;
	dtr=true;
	rts=true;
	op1=true;
	op2=true;

	sync_guardtime=false;
	FCR=0xff;
	Write_FCR(0x00);


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

	Write_MCR (0);
	Write_LCR (LCR_DIVISOR_Enable_MASK);
	Write_THR(transmit_reg);
	Write_IER(interrupt_reg);
	Write_LCR(line_reg);
	updateMSR();
	Read_MSR();
	PIC_DeActivateIRQ(irq);
}

CSerial::CSerial(const uint8_t port_idx, CommandLine *cmd)
        : port_index(port_idx)
{
	const uint16_t base = serial_baseaddr[port_index];

	irq = serial_defaultirq[port_index];

	const bool configured = getUintFromString("irq:", irq, cmd);
	// Only change the port's IRQ if it's outside the conflict range
	if (configured && (irq < 2 || irq > 15))
		irq = serial_defaultirq[port_index];

#if SERIAL_DEBUG
	dbg_serialtraffic = cmd->FindExist("dbgtr", false);
	dbg_modemcontrol  = cmd->FindExist("dbgmd", false);
	dbg_register      = cmd->FindExist("dbgreg", false);
	dbg_interrupt     = cmd->FindExist("dbgirq", false);
	dbg_aux			  = cmd->FindExist("dbgaux", false);
	
	if(cmd->FindExist("dbgall", false)) {
		dbg_serialtraffic= 
		dbg_modemcontrol= 
		dbg_register=
		dbg_interrupt=
		dbg_aux= true;
	}


	if(dbg_serialtraffic|dbg_modemcontrol|dbg_register|dbg_interrupt|dbg_aux)
		debugfp=OpenCaptureFile("serlog",".serlog.txt");
	else debugfp=0;

	if(debugfp == 0) {
		dbg_serialtraffic= 
		dbg_modemcontrol= 
		dbg_register=
		dbg_interrupt=
		dbg_aux= false;
	} else {
		std::string cleft;
		cmd->GetStringRemain(cleft);

		log_ser(true, "SERIAL: Port %" PRIu8 " base %3x, IRQ %" PRIu32
		        ", initstring \"%s\"\r\n\r\n",
		        GetPortNumber(), base, irq, cleft.c_str());
	}
#endif
	errorfifo = new Fifo(fifo_size);
	rxfifo = new Fifo(fifo_size);
	txfifo = new Fifo(fifo_size);

	mydosdevice=new device_COM(this);
	DOS_AddDevice(mydosdevice);

	errormsg_pending=false;
	framingErrors=0;
	parityErrors=0;
	overrunErrors=0;
	txOverrunErrors=0;
	overrunIF0=0;
	breakErrors=0;

	for (uint32_t i = 0; i < SERIAL_IO_HANDLERS; ++i) {
		WriteHandler[i].Install (i + base, SERIAL_Write, IO_MB);
		ReadHandler[i].Install(i + base, SERIAL_Read, IO_MB);
	}
}

bool CSerial::getUintFromString(const char *name, uint32_t &data, CommandLine *cmd)
{
	bool result = false;
	std::string tmpstring;
	if (cmd->FindStringBegin(name, tmpstring, false))
		result = (sscanf(tmpstring.c_str(), "%" PRIu32, &data) == 1);
	return result;
}

CSerial::~CSerial() {
	DOS_DelDevice(mydosdevice);
	for (uint16_t i = 0; i <= SERIAL_BASE_EVENT_COUNT; i++)
		removeEvent(i);

	// Free the fifos and devices
	delete(errorfifo);
	errorfifo = nullptr;
	delete(rxfifo);
	rxfifo = nullptr;
	delete(txfifo);
	txfifo = nullptr;

	// Uninstall the IO handlers
	for (uint32_t i = 0; i < SERIAL_IO_HANDLERS; ++i) {
		WriteHandler[i].Uninstall();
		ReadHandler[i].Uninstall();
	}
}

static bool idle(const double start, const uint32_t timeout)
{
	CALLBACK_Idle();
	return PIC_FullIndex() - start > timeout;
}

bool CSerial::Getchar(uint8_t *data, uint8_t *lsr, bool wait_dsr, uint32_t timeout)
{
	const double starttime = PIC_FullIndex();
	bool timed_out = false;

	// Wait until we're ready to receive (or we've timed out)
	const uint32_t ready_flag = (wait_dsr ? MSR_DSR_MASK : 0x0);
	while ((Read_MSR() & ready_flag) != ready_flag && !timed_out)
		timed_out = idle(starttime, timeout);

	// wait for a byte to arrive (or we've timed out)
	*lsr = static_cast<uint8_t>(Read_LSR());
	while (!(*lsr & LSR_RX_DATA_READY_MASK) && !timed_out) {
		timed_out = idle(starttime, timeout);
		*lsr = static_cast<uint8_t>(Read_LSR());
	}

	if (timed_out) {
#if SERIAL_DEBUG
		log_ser(dbg_aux,"Getchar data timeout: MSR 0x%x",Read_MSR());
#endif
		return false;
	}

	*data = static_cast<uint8_t>(Read_RHR());
#if SERIAL_DEBUG
	log_ser(dbg_aux,"Getchar read 0x%x",*data);
#endif
	return true;
}

/*
Three criteria need to be met before we can send the next character to the
receiver:
- our transfer queue needs to be empty (Tx hold high)
- the receiver says it's ready (DSR high), provided DSR was enabled
- the receiver says we are clear to send (CTS high), provided CTS was enabled
*/
bool CSerial::Putchar(uint8_t data, bool wait_dsr, bool wait_cts, uint32_t timeout)
{
	const double start_time = PIC_FullIndex();
	bool timed_out = false;

	// Wait until our transfer queue is empty (or we've timed out)
	while (!(Read_LSR() & LSR_TX_HOLDING_EMPTY_MASK) && !timed_out)
		timed_out = idle(start_time, timeout);

	// Wait until the receiver is ready (or we've timed out)
	const uint32_t ready_flags = (wait_dsr ? MSR_DSR_MASK : 0x0) |
	                             (wait_cts ? MSR_CTS_MASK : 0x0);
	while ((Read_MSR() & ready_flags) != ready_flags && !timed_out)
		timed_out = idle(start_time, timeout);

	if (timed_out) {
#if SERIAL_DEBUG
		log_ser(dbg_aux, "Putchar timeout: MSR 0x%x", Read_MSR());
#endif
		return false;
	}

	Write_THR(data);
#if SERIAL_DEBUG
	log_ser(dbg_aux, "Putchar 0x%x", data);
#endif
	return true;
}

class SERIALPORTS:public Module_base {
public:
	SERIALPORTS (Section * configuration):Module_base (configuration) {
		uint16_t biosParameter[SERIAL_MAX_PORTS] = {0};
		Section_prop *section = static_cast <Section_prop*>(configuration);

#if C_MODEM
		const Prop_path *pbFilename = section->Get_path("phonebookfile");
		MODEM_ReadPhonebook(pbFilename->realpath);
#endif

		char s_property[] = "serialx";
		for (uint8_t i = 0; i < SERIAL_MAX_PORTS; ++i) {
			// get the configuration property
			s_property[6] = '1' + static_cast<char>(i);
			Prop_multival* p = section->Get_multival(s_property);
			std::string type = p->GetSection()->Get_string("type");
			CommandLine cmd(0,p->GetSection()->Get_string("parameters"));
			
			// detect the type
			if (type=="dummy") {
				serialports[i] = new CSerialDummy (i, &cmd);
			}
#ifdef DIRECTSERIAL_AVAILIBLE
			else if (type=="directserial") {
				serialports[i] = new CDirectSerial (i, &cmd);
				if (!serialports[i]->InstallationSuccessful)  {
					// serial port name was wrong or already in use
					delete serialports[i];
					serialports[i] = NULL;
				}
			}
#endif
#if C_MODEM
			else if(type=="modem") {
				serialports[i] = new CSerialModem (i, &cmd);
				if (!serialports[i]->InstallationSuccessful)  {
					delete serialports[i];
					serialports[i] = NULL;
				}
			}
			else if(type=="nullmodem") {
				serialports[i] = new CNullModem (i, &cmd);
				if (!serialports[i]->InstallationSuccessful)  {
					delete serialports[i];
					serialports[i] = NULL;
				}
			}
#endif
			else if(type=="disabled") {
				serialports[i] = NULL;
			} else {
				serialports[i] = NULL;
				LOG_MSG("SERIAL: Port %" PRIu8 " invalid type \"%s\".",
				        static_cast<uint8_t>(i + 1), type.c_str());
			}
			if(serialports[i]) biosParameter[i] = serial_baseaddr[i];
		} // for 1-4
		BIOS_SetComPorts (biosParameter);
	}

	~SERIALPORTS()
	{
		for (uint8_t i = 0; i < SERIAL_MAX_PORTS; ++i) {
			if (serialports[i]) {
				delete serialports[i];
				serialports[i] = 0;
			}
		}
#if C_MODEM
		MODEM_ClearPhonebook();
#endif
	}
};

static SERIALPORTS *testSerialPortsBaseclass;

void SERIAL_Destroy(Section *sec)
{
	(void)sec; // unused, but required for API compliance
	delete testSerialPortsBaseclass;
	testSerialPortsBaseclass = NULL;
}

void SERIAL_Init (Section * sec) {
	// should never happen
	if (testSerialPortsBaseclass) delete testSerialPortsBaseclass;
	testSerialPortsBaseclass = new SERIALPORTS (sec);
	sec->AddDestroyFunction (&SERIAL_Destroy, true);
}
