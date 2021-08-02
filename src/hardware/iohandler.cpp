/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#include "inout.h"

#include <cassert>
#include <limits>
#include <cstring>
#include <unordered_map>

#include "setup.h"
#include "cpu.h"
#include "../src/cpu/lazyflags.h"
#include "callback.h"

// To-be-removed when Bitu-based IO handler API is deprecated:
extern std::unordered_map<io_port_t, IO_WriteHandler> io_writehandlers[IO_SIZES];
extern std::unordered_map<io_port_t, IO_ReadHandler> io_readhandlers[IO_SIZES];
void port_within_proposed(io_port_t port);
void val_within_proposed(io_val_t val);
io_val_t ReadPort(uint8_t req_bytes, io_port_t port);
void WritePort(uint8_t put_bytes, io_port_t port, io_val_t val);

// type-sized IO handler containers
extern std::unordered_map<io_port_t_proposed, io_read_f> io_read_handlers[io_widths];
extern std::unordered_map<io_port_t_proposed, io_write_f> io_write_handlers[io_widths];

// type-sized IO handler API
uint8_t read_byte_from_port(const io_port_t_proposed port);
uint16_t read_word_from_port(const io_port_t_proposed port);
uint32_t read_dword_from_port(const io_port_t_proposed port);
void write_byte_to_port(const io_port_t_proposed port, const uint8_t val);
void write_word_to_port(const io_port_t_proposed port, const uint16_t val);
void write_dword_to_port(const io_port_t_proposed port, const uint32_t val);


//#define ENABLE_PORTLOG

struct IOF_Entry {
	Bitu cs;
	Bitu eip;
};

#define IOF_QUEUESIZE 16
static struct {
	Bitu used;
	IOF_Entry entries[IOF_QUEUESIZE];
} iof_queue;

static Bits IOFaultCore(void) {
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bits ret=CPU_Core_Full_Run();
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in IO-fault core?");
	if (ret)
		return ret;
	if (!iof_queue.used) E_Exit("IO-faul Core without IO-faul");
	IOF_Entry * entry=&iof_queue.entries[iof_queue.used-1];
	if (entry->cs == SegValue(cs) && entry->eip==reg_eip)
		return -1;
	return 0;
}


/* Some code to make io operations take some virtual time. Helps certain
 * games with their timing of certain operations
 */

constexpr double IODELAY_READ_MICROS = 1.0;
constexpr double IODELAY_WRITE_MICROS = 0.75;

constexpr int32_t IODELAY_READ_MICROSk = static_cast<int32_t>(
        1024 / IODELAY_READ_MICROS);
constexpr int32_t IODELAY_WRITE_MICROSk = static_cast<int32_t>(
        1024 / IODELAY_WRITE_MICROS);

inline void IO_USEC_read_delay() {
	auto delaycyc = CPU_CycleMax / IODELAY_READ_MICROSk;
	if (GCC_UNLIKELY(delaycyc > CPU_Cycles))
		delaycyc = CPU_Cycles;
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

inline void IO_USEC_write_delay() {
	auto delaycyc = CPU_CycleMax / IODELAY_WRITE_MICROSk;
	if (GCC_UNLIKELY(delaycyc > CPU_Cycles))
		delaycyc = CPU_Cycles;
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

#ifdef ENABLE_PORTLOG
static Bit8u crtc_index = 0;
const char* const len_type[] = {" 8","16","32"};
void log_io(Bitu width, bool write, io_port_t port, io_val_t val)
{
	port_within_proposed(port);
	val_within_proposed(val);

	switch(width) {
	case 0:
		val&=0xff;
		break;
	case 1:
		val&=0xffff;
		break;
	}
	if (write) {
		// skip the video cursor position spam
		if (port==0x3d4) {
			if (width==0) crtc_index = (Bit8u)val;
			else if(width==1) crtc_index = (Bit8u)(val>>8);
		}
		if (crtc_index==0xe || crtc_index==0xf) {
			if((width==0 && (port==0x3d4 || port==0x3d5))||(width==1 && port==0x3d4))
				return;
		}

		switch(port) {
		//case 0x020: // interrupt command
		//case 0x040: // timer 0
		//case 0x042: // timer 2
		//case 0x043: // timer control
		//case 0x061: // speaker control
		case 0x3c8: // VGA palette
		case 0x3c9: // VGA palette
		// case 0x3d4: // VGA crtc
		// case 0x3d5: // VGA crtc
		// case 0x3c4: // VGA seq
		// case 0x3c5: // VGA seq
			break;
		default:
			LOG_MSG("IOSBUS: iow%s % 4x % 4x, cs:ip %04x:%04x",
			        len_type[width], static_cast<uint32_t>(port),
			        val, SegValue(cs), reg_eip);
			break;
		}
	} else {
		switch(port) {
		//case 0x021: // interrupt status
		//case 0x040: // timer 0
		//case 0x042: // timer 2
		//case 0x061: // speaker control
		case 0x201: // joystick status
		case 0x3c9: // VGA palette
		// case 0x3d4: // VGA crtc index
		// case 0x3d5: // VGA crtc
		case 0x3da: // display status - a real spammer
			// don't log for the above cases
			break;
		default:
			LOG_MSG("IOBUS: ior%s % 4x % 4x,\t\tcs:ip %04x:%04x",
			        len_type[width], static_cast<uint32_t>(port),
			        val, SegValue(cs), reg_eip);
			break;
		}
	}
}
#else
#define log_io(W, X, Y, Z)
#endif

void IO_WriteB(io_port_t port, io_val_t val)
{
	port_within_proposed(port);
	val_within_proposed(val);

	log_io(0, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit8u old_al = reg_al;
		Bit16u old_dx = reg_dx;
		reg_al = static_cast<uint8_t>(val);
		reg_dx = static_cast<uint16_t>(port);
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x08;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_al = old_al;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_write_delay();

		// To-be-removed when Bitu-based IO handler API is deprecated:
		WritePort(IO_MB, port, val);

		// Assert and cast will be removed after deprecating Bitu API
		assert(val <= UINT8_MAX);
		assert(port <= UINT16_MAX);
		write_byte_to_port(static_cast<uint16_t>(port),
		                   static_cast<uint8_t>(val));
	}
}

void IO_WriteW(io_port_t port, io_val_t val)
{
	port_within_proposed(port);
	val_within_proposed(val);

	log_io(1, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_ax = reg_ax;
		Bit16u old_dx = reg_dx;
		reg_ax = static_cast<uint16_t>(val);
		reg_dx = static_cast<uint16_t>(port);
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x0a;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_ax = old_ax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_write_delay();

		// To-be-removed when Bitu-based IO handler API is deprecated:
		WritePort(IO_MW, port, val);

		// Assert and cast will be removed after deprecating Bitu API
		assert(val <= UINT16_MAX);
		assert(port <= UINT16_MAX);
		write_word_to_port(static_cast<uint16_t>(port),
		                   static_cast<uint16_t>(val));
	}
}

void IO_WriteD(io_port_t port, io_val_t val)
{
	port_within_proposed(port);
	val_within_proposed(val);

	log_io(2, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit32u old_eax = reg_eax;
		Bit16u old_dx = reg_dx;
		reg_eax = static_cast<uint32_t>(val);
		reg_dx = static_cast<uint16_t>(port);
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x0c;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_eax = old_eax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	} else {
		// To-be-removed when Bitu-based IO handler API is deprecated:
		WritePort(IO_MD, port, val);

		// Assert and cast will be removed after deprecating Bitu API
		assert(val <= UINT32_MAX);
		assert(port <= UINT16_MAX);
		write_dword_to_port(static_cast<uint16_t>(port),
		                    static_cast<uint32_t>(val));
	}
}

io_val_t IO_ReadB(io_port_t port)
{
	port_within_proposed(port);

	io_val_t retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit8u old_al = reg_al;
		Bit16u old_dx = reg_dx;
		reg_dx = static_cast<uint16_t>(port);
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x00;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_al;
		reg_al = old_al;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
		return retval;
	}
	else {
		IO_USEC_read_delay();

		// To-be-removed when Bitu-based IO handler API is deprecated:
		if (io_readhandlers[0].find(port) != io_readhandlers[0].end()) {
			retval = ReadPort(IO_MB, port);
		} else {
			assert(port <= UINT16_MAX);
			retval = read_byte_from_port(static_cast<uint16_t>(port));
		}
	}
	log_io(0, false, port, retval);
	return retval;
}

io_val_t IO_ReadW(io_port_t port)
{
	port_within_proposed(port);

	io_val_t retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_ax = reg_ax;
		Bit16u old_dx = reg_dx;
		reg_dx = static_cast<uint16_t>(port);
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x02;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_ax;
		reg_ax = old_ax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_read_delay();
		
		// To-be-removed when Bitu-based IO handler API is deprecated:
		if (io_readhandlers[1].find(port) != io_readhandlers[1].end()) {
			retval = ReadPort(IO_MW, port);
		} else {
			assert(port <= UINT16_MAX);
			retval = read_word_from_port(static_cast<uint16_t>(port));
		}
	}
	log_io(1, false, port, retval);
	return retval;
}

io_val_t IO_ReadD(io_port_t port)
{
	port_within_proposed(port);

	io_val_t retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit32u old_eax = reg_eax;
		Bit16u old_dx = reg_dx;
		reg_dx = static_cast<uint16_t>(port);
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x04;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_eax;
		reg_eax = old_eax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	} else {

		// To-be-removed when Bitu-based IO handler API is deprecated:
		if (io_readhandlers[2].find(port) != io_readhandlers[2].end()) {
			retval = ReadPort(IO_MD, port);
		} else {
			assert(port <= UINT16_MAX);
			retval = read_dword_from_port(static_cast<uint16_t>(port));
		}
	}

	log_io(2, false, port, retval);
	return retval;
}


class IO final : public Module_base {
public:
	IO(Section* configuration):Module_base(configuration){
		iof_queue.used = 0;
	}
	~IO()
	{
		size_t total_bytes = 0u;
		for (uint8_t i = 0; i < IO_SIZES; ++i) {
			const size_t readers = io_readhandlers[i].size();
			const size_t writers = io_writehandlers[i].size();
			DEBUG_LOG_MSG("IOBUS-OLD: Releasing %zu read and %zu write %d-bit port handlers",
			              readers, writers, 8 << i);
			total_bytes += readers * sizeof(IO_ReadHandler) +
			               sizeof(io_readhandlers[i]);
			total_bytes += writers * sizeof(IO_WriteHandler) +
			               sizeof(io_readhandlers[i]);
			io_readhandlers[i].clear();
			io_writehandlers[i].clear();
		}
		DEBUG_LOG_MSG("IOBUS-OLD: Handlers consumed %zu total bytes", total_bytes);

		size_t new_total_bytes = 0u;
		for (uint8_t i = 0; i < io_widths; ++i) {
			const size_t readers = io_read_handlers[i].size();
			const size_t writers = io_write_handlers[i].size();
			DEBUG_LOG_MSG("IOBUS-NEW: Releasing %zu read and %zu write %d-bit port handlers",
			              readers, writers, 8 << i);
			new_total_bytes += readers * sizeof(io_read_f) + sizeof(io_read_handlers[i]);
			new_total_bytes += writers * sizeof(io_write_f) + sizeof(io_write_handlers[i]);
			io_read_handlers[i].clear();
			io_write_handlers[i].clear();
		}
		DEBUG_LOG_MSG("IOBUS-NEW: Handlers consumed %zu total bytes",
		              new_total_bytes);
	}
};

static IO* test;

void IO_Destroy(Section*) {
	delete test;
}

void IO_Init(Section * sect) {
	test = new IO(sect);
	sect->AddDestroyFunction(&IO_Destroy);
}
