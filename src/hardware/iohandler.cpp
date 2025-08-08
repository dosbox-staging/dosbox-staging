// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "inout.h"

#include <cassert>
#include <limits>
#include <cstring>
#include <unordered_map>

#include "callback.h"
#include "cpu.h"
#include "cpu/lazyflags.h"
#include "setup.h"

//#define ENABLE_PORTLOG

// type-sized IO handler containers
extern std::unordered_map<io_port_t, io_read_f> io_read_handlers[io_widths];
extern std::unordered_map<io_port_t, io_write_f> io_write_handlers[io_widths];

// type-sized IO handler API
uint8_t read_byte_from_port(const io_port_t port);
uint16_t read_word_from_port(const io_port_t port);
uint32_t read_dword_from_port(const io_port_t port);
void write_byte_to_port(const io_port_t port, const uint8_t val);
void write_word_to_port(const io_port_t port, const uint16_t val);
void write_dword_to_port(const io_port_t port, const uint32_t val);


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
	if (delaycyc > CPU_Cycles) {
		delaycyc = CPU_Cycles;
	}
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

inline void IO_USEC_write_delay() {
	auto delaycyc = CPU_CycleMax / IODELAY_WRITE_MICROSk;
	if (delaycyc > CPU_Cycles) {
		delaycyc = CPU_Cycles;
	}
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

#ifdef ENABLE_PORTLOG
static uint8_t crtc_index = 0;

void log_io(io_width_t width, bool write, io_port_t port, io_val_t val)
{
	switch(width) {
	case io_width_t::byte: val &= 0xff; break;
	case io_width_t::word: val &= 0xffff; break;
	}
	if (write) {
		// skip the video cursor position spam
		if (port==0x3d4) {
			if (width==io_width_t::byte) crtc_index = (uint8_t)val;
			else if(width==io_width_t::word) crtc_index = (uint8_t)(val>>8);
		}
		if (crtc_index==0xe || crtc_index==0xf) {
			if((width==io_width_t::byte && (port==0x3d4 || port==0x3d5))||(width==io_width_t::word && port==0x3d4))
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
			LOG_MSG("IOSBUS: write width=%u bytes, % 4x % 4x, cs:ip %04x:%04x",
			        static_cast<uint8_t>(width), port, val, SegValue(cs), reg_eip);
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
			LOG_MSG("IOBUS: read width=%u bytes % 4x % 4x,\t\tcs:ip %04x:%04x",
			        static_cast<uint8_t>(width), port, val, SegValue(cs), reg_eip);
			break;
		}
	}
}
#else
#define log_io(W, X, Y, Z)
#endif

void IO_WriteB(io_port_t port, uint8_t val)
{
	log_io(io_width_t::byte, true, port, val);
	if (GETFLAG(VM) && (CPU_IO_Exception(port, 1))) {
		const auto old_lflags = lflags;
		const auto old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		uint8_t old_al = reg_al;
		uint16_t old_dx = reg_dx;
		reg_al = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSegment(icb));
		reg_eip = RealOffset(icb)+0x08;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_al = old_al;
		reg_dx = old_dx;
		lflags = old_lflags;
		cpudecoder=old_cpudecoder;
	} else {
		IO_USEC_write_delay();
		write_byte_to_port(port, val);
	}
}

void IO_WriteW(io_port_t port, uint16_t val)
{
	log_io(io_width_t::word, true, port, val);
	if (GETFLAG(VM) && (CPU_IO_Exception(port, 2))) {
		const auto old_lflags = lflags;
		const auto old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		uint16_t old_ax = reg_ax;
		uint16_t old_dx = reg_dx;
		reg_ax = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSegment(icb));
		reg_eip = RealOffset(icb)+0x0a;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_ax = old_ax;
		reg_dx = old_dx;
		lflags = old_lflags;
		cpudecoder=old_cpudecoder;
	} else {
		IO_USEC_write_delay();
		write_word_to_port(port, val);
	}
}

void IO_WriteD(io_port_t port, uint32_t val)
{
	log_io(io_width_t::dword, true, port, val);
	if (GETFLAG(VM) && (CPU_IO_Exception(port, 4))) {
		const auto old_lflags = lflags;
		const auto old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		uint32_t old_eax = reg_eax;
		uint16_t old_dx = reg_dx;
		reg_eax = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSegment(icb));
		reg_eip = RealOffset(icb)+0x0c;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_eax = old_eax;
		reg_dx = old_dx;
		lflags = old_lflags;
		cpudecoder=old_cpudecoder;
	} else {
		write_dword_to_port(port, val);
	}
}

uint8_t IO_ReadB(io_port_t port)
{
	uint8_t retval;
	if (GETFLAG(VM) && (CPU_IO_Exception(port, 1))) {
		const auto old_lflags = lflags;
		const auto old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		uint8_t old_al = reg_al;
		uint16_t old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSegment(icb));
		reg_eip = RealOffset(icb)+0x00;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_al;
		reg_al = old_al;
		reg_dx = old_dx;
		lflags = old_lflags;
		cpudecoder=old_cpudecoder;
		return retval;
	} else {
		IO_USEC_read_delay();
		retval = read_byte_from_port(port);
	}
	log_io(io_width_t::byte, false, port, retval);
	return retval;
}

uint16_t IO_ReadW(io_port_t port)
{
	uint16_t retval;
	if (GETFLAG(VM) && (CPU_IO_Exception(port, 2))) {
		const auto old_lflags = lflags;
		const auto old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		uint16_t old_ax = reg_ax;
		uint16_t old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSegment(icb));
		reg_eip = RealOffset(icb)+0x02;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_ax;
		reg_ax = old_ax;
		reg_dx = old_dx;
		lflags = old_lflags;
		cpudecoder=old_cpudecoder;
	} else {
		IO_USEC_read_delay();
		retval = read_word_from_port(port);
	}
	log_io(io_width_t::word, false, port, retval);
	return retval;
}

uint32_t IO_ReadD(io_port_t port)
{
	uint32_t retval;
	if (GETFLAG(VM) && (CPU_IO_Exception(port, 4))) {
		const auto old_lflags = lflags;
		const auto old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		uint32_t old_eax = reg_eax;
		uint16_t old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSegment(icb));
		reg_eip = RealOffset(icb)+0x04;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_eax;
		reg_eax = old_eax;
		reg_dx = old_dx;
		lflags = old_lflags;
		cpudecoder=old_cpudecoder;
	} else {
		retval = read_dword_from_port(port);
	}

	log_io(io_width_t::dword, false, port, retval);
	return retval;
}


class IO final : public ModuleBase {
public:
	IO(Section* configuration):ModuleBase(configuration){
		iof_queue.used = 0;
	}
	~IO()
	{
		[[maybe_unused]] size_t total_bytes = 0u;
		for (uint8_t i = 0; i < io_widths; ++i) {
			const auto readers = io_read_handlers[i].size();
			const auto writers = io_write_handlers[i].size();
			LOG_DEBUG("IOBUS: Releasing %d read and %d write %d-bit port handlers",
			          static_cast<int>(readers),
			          static_cast<int>(writers),
			          8 << i);

			total_bytes += readers * sizeof(io_read_f) + sizeof(io_read_handlers[i]);
			total_bytes += writers * sizeof(io_write_f) + sizeof(io_write_handlers[i]);
			io_read_handlers[i].clear();
			io_write_handlers[i].clear();
		}
		LOG_DEBUG("IOBUS: Handlers consumed %d total bytes",
		          static_cast<int>(total_bytes));
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
