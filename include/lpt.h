// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PARALLEL_PORT_H_
#define PARALLEL_PORT_H_

#include "bit_view.h"

// There are three hexadecimal addresses commonly
// used for parallel ports: 378h, 278h, 3BCh.
// These are absolute addresses, fixed in memory.

// They can be distinguished from them “logical”
// addresses accessed by users and many programs:
// LPT 1, LPT 2, LPT 3, ...
// These logical addresses
// can be interpreted as “1st Line Printer, 2nd
// Line Printer, 3rd Line Printer,…”

// Consequently, one cannot have a “2nd Line
// Printer,” without having a “1st Line Printer.” –
// ie: You can’t get a LPT 2, unless you already
// have a LPT 1.
// Ref: http://faq.lavalink.com/2006/11/understanding-parallel-port-addressing/

enum LptPorts : io_port_t {
	Lpt1Port = 0x378,
	Lpt2Port = 0x278,
	Lpt3Port = 0x3bc,
};

// The Parallel port has three registers:
// Name      Read/write   Port offset
// ~~~~      ~~~~~~~~~~   ~~~~~~~~~~~
// Data      write-only   0
// Status    read-only    1
// Control   write-only   2

union LptStatusRegister {
	uint8_t data = 0xff;
	bit_view<0, 2> reserved;
	bit_view<2, 1> irq;
	bit_view<3, 1> error;
	bit_view<4, 1> select_in;
	bit_view<5, 1> paper_out;
	bit_view<6, 1> ack;
	bit_view<7, 1> busy;
};
// The ERROR, ACK and BUSY signals are active-low when reading from the IO port.

union LptControlRegister {
	uint8_t data = 0;
	bit_view<0, 1> strobe;
	bit_view<1, 1> auto_lf;
	bit_view<2, 1> initialize;
	bit_view<3, 1> select;
	bit_view<4, 1> irq_ack;
	bit_view<5, 1> bidi;
	bit_view<6, 1> bit6; // unused
	bit_view<7, 1> bit7; // unused
};
// The INITIALISE signal is active low when writing to the IO port.

// The STROBE signal is for handshaking and alerts the printer to data
// being ready at the data port.

// AUTO_LF is the Automatic Line-Feed signal. If this is set and the
// printer receives a Carriage-Return character (0x0D), the printer will
// automatically perform a Line-Feed (character 0x0A) as well.

// INITIALISE, sometimes called PRIME, alerts the device that data that
// a data conversation is about to start. This signal may result in a
// printer performing a reset and any buffers being flushed.

// Protocol: data is sent to the connected device by writing the byte to
// the data port, then pulsing the STROBE signal. This pulse informs the
// device that data is ready to be read. The device will respond by
// raising its BUSY signal and then reading the data and performing some
// processing on it. Once this processing is complete, the device will
// lower the Busy signal and may raise a brief ACK signal to indicate
// that it has finished.

// Ref: https://wiki.osdev.org/Parallel_port

#endif
