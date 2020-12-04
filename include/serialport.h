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

#ifndef DOSBOX_SERIALPORT_H
#define DOSBOX_SERIALPORT_H

#include "dosbox.h"

#include <algorithm>
#include <vector>

#ifndef DOSBOX_INOUT_H
#include "inout.h"
#endif
#ifndef DOSBOX_TIMER_H
#include "timer.h"
#endif
#ifndef DOSBOX_DOS_INC_H
#include "dos_inc.h"
#endif
#ifndef DOSBOX_PROGRAMS_H
#include "programs.h"
#endif

#include "../src/hardware/serialport/fifo.h"

// set this to 1 for serial debugging in release mode
#define SERIAL_DBG_FORCED 0

#if (C_DEBUG || SERIAL_DBG_FORCED)
#define SERIAL_DEBUG 1
#endif

#if SERIAL_DEBUG
#include "hardware.h"
#endif

constexpr uint8_t SERIAL_IO_HANDLERS = 8;

class CSerial {
public:
	CSerial(const CSerial &) = delete;            // prevent copying
	CSerial &operator=(const CSerial &) = delete; // prevent assignment

#if SERIAL_DEBUG
	FILE * debugfp;
	bool dbg_modemcontrol = false; // RTS,CTS,DTR,DSR,RI,CD
	bool dbg_serialtraffic = false;
	bool dbg_register = false;
	bool dbg_interrupt = false;
	bool dbg_aux = false;
	void log_ser(bool active, char const *format, ...);
#endif

	static bool getUintFromString(const char *name,
	                              uint32_t &data,
	                              CommandLine *cmd);

	bool InstallationSuccessful = false; // check after constructing. If
	                                     // something was wrong, delete it
	                                     // right away.

	/*
	 * Communication port index is typically 0-3, but logically limited
	 * to the number of physical interrupts available on the system.
	 */
	CSerial(const uint8_t port_idx, CommandLine *cmd);
	virtual ~CSerial();

	IO_ReadHandleObject ReadHandler[SERIAL_IO_HANDLERS];
	IO_WriteHandleObject WriteHandler[SERIAL_IO_HANDLERS];

	float bytetime = 0.0f; // how long a byte takes to transmit/receive in
	                       // milliseconds
	void changeLineProperties();
	const uint8_t port_index = 0;

	void setEvent(uint16_t type, float duration);
	void removeEvent(uint16_t type);
	void handleEvent(uint16_t type);
	virtual void handleUpperEvent(uint16_t type) = 0;

	// defines for event type
#define SERIAL_TX_LOOPBACK_EVENT 0
#define SERIAL_THR_LOOPBACK_EVENT 1
#define SERIAL_ERRMSG_EVENT 2

#define SERIAL_TX_EVENT 3
#define SERIAL_RX_EVENT 4
#define SERIAL_POLLING_EVENT 5
#define SERIAL_THR_EVENT 6
#define SERIAL_RX_TIMEOUT_EVENT 7

#define	SERIAL_BASE_EVENT_COUNT 7
#define SERIAL_MAX_PORTS        4
// Note: The code currently only handles four ports.
//       To allow more, add more UARTs in SERIAL_Read(...)

	uint32_t irq = 0;

	// CSerial requests an update of the input lines
	virtual void updateMSR()=0;

	// Control lines from prepherial to serial port
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

	// From serial port to prepherial
	// set output lines
	virtual void setRTSDTR(bool rts, bool dtr)=0;
	virtual void setRTS(bool val)=0;
	virtual void setDTR(bool val)=0;

	// Register access
	void Write_THR(uint8_t data);
	void Write_IER(uint8_t data);
	void Write_FCR(uint8_t data);
	void Write_LCR(uint8_t data);
	void Write_MCR(uint8_t data);
	// Really old hardware seems to have the delta part of this register
	// writable
	void Write_MSR(uint8_t data);
	void Write_SPR(uint8_t data);
	void Write_reserved(uint8_t data, uint8_t address);

	uint32_t Read_RHR();
	uint32_t Read_IER();
	uint32_t Read_ISR();
	uint32_t Read_LCR();
	uint32_t Read_MCR();
	uint32_t Read_LSR();
	uint32_t Read_MSR();
	uint32_t Read_SPR();

	// If a byte comes from loopback or prepherial, put it in here.
	void receiveByte(uint8_t data);
	void receiveByteEx(uint8_t data, uint8_t error);

	// If an error was received, put it here (in LSR register format)
	void receiveError(uint8_t errorword);

	// depratched
	// connected device checks, if port can receive data:
	bool CanReceiveByte();
	
	// when THR was shifted to TX
	void ByteTransmitting();
	
	// When done sending, notify here
	void ByteTransmitted();

	// Transmit byte to prepherial
	virtual void transmitByte(uint8_t val, bool first) = 0;

	// switch break state to the passed value
	virtual void setBreak(bool value)=0;
	
	// change baudrate, number of bits, parity, word length al at once
	virtual void updatePortConfig(uint16_t divider, uint8_t lcr) = 0;

	void Init_Registers();

	bool Putchar(uint8_t data, bool wait_dtr, bool wait_rts, uint32_t timeout);
	bool Getchar(uint8_t *data, uint8_t *lsr, bool wait_dsr, uint32_t timeout);
	uint8_t GetPortNumber() const { return port_index + 1; }

private:
	DOS_Device *mydosdevice = nullptr;

	// I used this spec: st16c450v420.pdf

	void ComputeInterrupts();
	
	// a sub-interrupt is triggered
	void rise(uint8_t priority);

	// clears the pending sub-interrupt
	void clear(uint8_t priority);

#define ERROR_PRIORITY   4    // overrun, parity error, frame error, break
#define RX_PRIORITY      1    // a byte has been received
#define TX_PRIORITY      2    // tx buffer has become empty
#define MSR_PRIORITY     8    // CRS, DSR, RI, DCD change 
#define TIMEOUT_PRIORITY 0x10
#define NONE_PRIORITY    0

	uint8_t waiting_interrupts = 0; // these are on, but maybe not enabled

	// 16C550
	//				read/write		name

	uint16_t baud_divider = 0;

 	#define RHR_OFFSET 0	// r Receive Holding Register, also LSB of Divisor Latch (r/w)
							// Data: whole byte
	#define THR_OFFSET 0	// w Transmit Holding Register
							// Data: whole byte
	uint8_t IER = 0; //	r/w		Interrupt Enable Register, also
	                 // MSB of Divisor Latch
#define IER_OFFSET 1

	bool irq_active = false;

#define RHR_INT_Enable_MASK          0x1
#define THR_INT_Enable_MASK          0x2
#define Receive_Line_INT_Enable_MASK 0x4
#define Modem_Status_INT_Enable_MASK 0x8

	uint8_t ISR = 0; //	r				Interrupt Status
	                 // Register
#define ISR_OFFSET 2

#define ISR_CLEAR_VAL       0x1
#define ISR_FIFOTIMEOUT_VAL 0xc
#define ISR_ERROR_VAL       0x6
#define ISR_RX_VAL          0x4
#define ISR_TX_VAL          0x2
#define ISR_MSR_VAL         0x0
public:
	uint8_t LCR = 0; //	r/w				Line Control
	                 // Register
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

#define LCR_BREAK_MASK          0x40
#define LCR_DIVISOR_Enable_MASK 0x80
#define LCR_PORTCONFIG_MASK     0x3F

#define LCR_PARITY_NONE  0x0
#define LCR_PARITY_ODD   0x8
#define LCR_PARITY_EVEN  0x18
#define LCR_PARITY_MARK  0x28
#define LCR_PARITY_SPACE 0x38

#define LCR_DATABITS_5 0x0
#define LCR_DATABITS_6 0x1
#define LCR_DATABITS_7 0x2
#define LCR_DATABITS_8 0x3

#define LCR_STOPBITS_1           0x0
#define LCR_STOPBITS_MORE_THAN_1 0x4

// Modem Control Register
// r/w
#define MCR_OFFSET 4
	bool dtr = false;      // bit0: DTR
	bool rts = false;      // bit1: RTS
	bool op1 = false;      // bit2: OP1
	bool op2 = false;      // bit3: OP2
	bool loopback = false; // bit4: loop back enable

#define MCR_DTR_MASK             0x1
#define MCR_RTS_MASK             0x2
#define MCR_OP1_MASK             0x4
#define MCR_OP2_MASK             0x8
#define MCR_LOOPBACK_Enable_MASK 0x10
public:
	uint8_t LSR = 0; //	r				Line Status
	                 // Register
private:
#define LSR_OFFSET 5

#define LSR_RX_DATA_READY_MASK    0x1
#define LSR_OVERRUN_ERROR_MASK    0x2
#define LSR_PARITY_ERROR_MASK     0x4
#define LSR_FRAMING_ERROR_MASK    0x8
#define LSR_RX_BREAK_MASK         0x10
#define LSR_TX_HOLDING_EMPTY_MASK 0x20
#define LSR_TX_EMPTY_MASK         0x40

#define LSR_ERROR_MASK 0x1e

	// error printing
	bool errormsg_pending = false;
	uint32_t framingErrors = 0;
	uint32_t parityErrors = 0;
	uint32_t overrunErrors = 0;
	uint32_t txOverrunErrors = 0;
	uint32_t overrunIF0 = 0;
	uint32_t breakErrors = 0;

	// Modem Status Register
	//	r
	#define MSR_OFFSET 6
	bool d_cts = false; // bit0: deltaCTS
	bool d_dsr = false; // bit1: deltaDSR
	bool d_ri = false;  // bit2: deltaRI
	bool d_cd = false;  // bit3: deltaCD
	bool cts = false;   // bit4: CTS
	bool dsr = false;   // bit5: DSR
	bool ri = false;    // bit6: RI
	bool cd = false;    // bit7: CD

#define MSR_delta_MASK 0xf
#define MSR_LINE_MASK  0xf0

#define MSR_dCTS_MASK 0x1
#define MSR_dDSR_MASK 0x2
#define MSR_dRI_MASK  0x4
#define MSR_dCD_MASK  0x8
#define MSR_CTS_MASK  0x10
#define MSR_DSR_MASK  0x20
#define MSR_RI_MASK   0x40
#define MSR_CD_MASK   0x80

	uint8_t SPR = 0; //	r/w				Scratchpad Register
#define SPR_OFFSET 7

	// For loopback purposes...
	uint8_t loopback_data = 0;
	void transmitLoopbackByte(uint8_t val, bool value);

private:
	// Universal Asynchronous Receiver Transmitter (UARTs) were largely
	// defined by their buffer sizes:
	// - 8250, 16450, and early 16550: 1-byte buffer and are obsolete
	// - 16550A and 16C552: 16-byte buffer
	// - 16650: 32-byte buffer
	// - 16750: 64-byte buffer
	// - 16850 and 16C850: 128-byte buffer
	// - 16950: up to 512-byte buffer
	// - Hayes ESP accelerator: 1024-byte buffer
	const uint16_t fifo_size = 16; // emulate the 16550A
	Fifo errorfifo;
	Fifo rxfifo;
	Fifo txfifo;
	uint32_t errors_in_fifo = 0;
	uint32_t rx_interrupt_threshold = 0;
	uint8_t FCR = 0;
	bool sync_guardtime = false;

	#define FIFO_STATUS_ACTIVE 0xc0 // FIFO is active AND works ;)
	#define FIFO_ERROR 0x80
	#define FCR_ACTIVATE 0x01
	#define FCR_CLEAR_RX 0x02
	#define FCR_CLEAR_TX 0x04
	#define FCR_OFFSET 2
	#define FIFO_FLOWCONTROL 0x20
};

extern CSerial* serialports[];
const uint8_t serial_defaultirq[] = {4, 3, 4, 3};
const uint16_t serial_baseaddr[] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};
const char *const serial_comname[] = {"COM1", "COM2", "COM3", "COM4"};

// the COM devices

class device_COM : public DOS_Device {
public:
	device_COM(const device_COM &) = delete;            // prevent copying
	device_COM &operator=(const device_COM &) = delete; // prevent assignment

	// Creates a COM device that communicates with the num-th parallel port,
	// i.e. is LPTnum
	device_COM(class CSerial* sc);
	~device_COM();
	bool Read(uint8_t *data, uint16_t *size);
	bool Write(uint8_t *data, uint16_t *size);
	bool Seek(uint32_t *pos, uint32_t type);
	bool Close();
	uint16_t GetInformation();

private:
	CSerial *sclass = nullptr;
};

#endif
