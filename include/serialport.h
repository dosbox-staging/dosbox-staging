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

#if !defined __SERIALPORT_H
#define __SERIALPORT_H

#include <dosbox.h>

//If it's too high you overflow terminal clients buffers i think
#define FIFO_SIZE  (1024)

// Serial port interface //

#define M_CTS 0x01
#define M_DSR 0x02
#define M_RI  0x04
#define M_DCD 0x08

enum INT_TYPES {
	INT_MS,
	INT_TX,
	INT_RX,
	INT_RX_FIFO,
	INT_LS,
	INT_NONE,
};

typedef void MControl_Handler(Bitu mc);

class CSerial {
public:

	// Constructor takes base port (0x3f0, 0x2f0, 0x2e0, etc.), IRQ, and initial bps //
	CSerial (Bit16u initbase, Bit8u initirq, Bit32u initbps);
	virtual ~CSerial();

	// External port functions for IOHandlers //
	void write_port(Bit32u port, Bit8u val);
	Bit8u read_port(Bit32u port);

	static void write_serial(Bit32u port,Bit8u val);
	static Bit8u read_serial(Bit32u port);

	void SetMCHandler(MControl_Handler * mcontrol);
	
	/* Allow the modem to change the modem status bits */
	void setmodemstatus(Bit8u status);
	Bit8u getmodemstatus(void);
	Bit8u getlinestatus(void);

	void checkint(void);
	void raiseint(INT_TYPES type);
	void lowerint(INT_TYPES type);

	/* Access to the receive fifo */
	Bitu  rx_free();
	void  rx_addb(Bit8u byte);
	void  rx_adds(Bit8u * data,Bitu size);
	Bitu  rx_size();
	Bit8u rx_readb(void);

	/* Access to the transmit fifo */
	Bitu  tx_free();
	void  tx_addb(Bit8u byte);
	Bitu  tx_size();
	Bit8u tx_readb(void);


	//  These variables maintain the status of the serial port
	Bit16u base;
	Bit16u irq;
	Bit32u bps;

	bool FIFOenabled;
	Bit16u FIFOsize;

private:

	void setdivisor(Bit8u dmsb, Bit8u dlsb);
	void checkforirq(void);
	struct {
		Bitu used;
		Bitu pos;
		Bit8u data[FIFO_SIZE];
	} rx_fifo,tx_fifo;
	struct {
		Bitu requested;
		Bitu enabled;
		INT_TYPES active;
	} ints;

	Bitu rx_lastread;
	Bit8u irq_pending;
	
	Bit8u scratch;
	Bit8u dlab;
	Bit8u divisor_lsb;
	Bit8u divisor_msb;
	Bit8u wordlen;
	Bit8u dtr;
	Bit8u rts;
	Bit8u out1;
	Bit8u out2;
	Bit8u local_loopback;
	Bit8u linectrl;
	Bit8u intid;
	Bit8u ierval;
	Bit8u mstatus;

	Bit8u txval;
	Bit8u timeout;

	MControl_Handler * mc_handler;
	char remotestr[4096];
};

// This function returns the CSerial objects for ports 1-4 //
CSerial *getComport(Bitu portnum);

#endif

