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

#include <assert.h>

#include  "dosbox.h"

//If it's too high you overflow terminal clients buffers i think
#define QUEUE_SIZE 1024

// Serial port interface //

#define MS_CTS 0x01
#define MS_DSR 0x02
#define MS_RI  0x04
#define MS_DCD 0x08

#define MC_DTR 0x1
#define MC_RTS 0x2


class CFifo {
public:
	CFifo(Bitu _size) {
		size=_size;
		pos=used=0;
		data=new Bit8u[size];
	}
	~CFifo() {
		delete[] data;
	}
	INLINE Bitu left(void) {
		return size-used;
	}
	INLINE Bitu inuse(void) {
		return used;
	}
	void clear(void) {
		used=pos=0;
	}
	bool isFull() {
		return (used >= size);
	}
	void addb(Bit8u _val) {
		assert(used<size);
		Bitu where=pos+used;
		if (where>=size) where-=size;
		data[where]=_val;
		used++;
	}
	void adds(Bit8u * _str,Bitu _len) {
		assert((used+_len)<=size);
		Bitu where=pos+used;
		used+=_len;
		while (_len--) {
			if (where>=size) where-=size;
			data[where++]=*_str++;
		}
	}
	Bit8u getb(void) {
		if (!used) return data[pos];
		Bitu where=pos;
		if (++pos>=size) pos-=size;
		used--;
		return data[where];
	}
	void gets(Bit8u * _str,Bitu _len) {
		assert(used>=_len);
		used-=_len;
		while (_len--) {
			*_str++=data[pos];
			if (++pos>=size) pos-=size;
		}
	}
private:
	Bit8u * data;
	Bitu size,pos,used;
};

class CSerial {
public:

	CSerial() {

	}
	// Constructor takes base port (0x3f0, 0x2f0, 0x2e0, etc.), IRQ, and initial bps //
	CSerial (Bit16u initbase, Bit8u initirq, Bit32u initbps);
	virtual ~CSerial();

	void write_reg(Bitu reg, Bitu val);
	Bitu read_reg(Bitu reg);

	void SetModemStatus(Bit8u status);
	virtual bool CanRecv(void)=0;
	virtual bool CanSend(void)=0;
	virtual void Send(Bit8u val)=0;
	virtual Bit8u Recv(Bit8u val)=0;
	virtual void Timer(void);

	void checkint(void);

	Bitu base;
	Bitu irq;
	Bitu bps;
	Bit8u mctrl;

	CFifo *rqueue;
	CFifo *tqueue;

private:
	void UpdateBaudrate(void);
	bool FIFOenabled;
	Bit8u FIFOsize;
	bool dotxint;
	
	Bit8u scratch;
	Bit8u dlab;
	Bit8u divisor_lsb;
	Bit8u divisor_msb;
	Bit8u local_loopback;
	Bit8u iir;
	Bit8u ier;
	Bit8u mstatus;

	Bit8u linectrl;
	Bit8u errors;
};

#include <list>

typedef std::list<CSerial *> CSerialList;
typedef std::list<CSerial *>::iterator CSerial_it;

extern CSerialList seriallist;

#endif

