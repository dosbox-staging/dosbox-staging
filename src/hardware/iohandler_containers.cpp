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

#include "dosbox.h"

#include <cassert>
#include <cstring>
#include <functional>
#include <limits>
#include <unordered_map>

#include "inout.h"

// To-be-removed when Bitu-based IO handler API is deprecated:
std::unordered_map<io_port_t, IO_WriteHandler> io_writehandlers[IO_SIZES] = {};
std::unordered_map<io_port_t, IO_ReadHandler> io_readhandlers[IO_SIZES] = {};

// To-be-removed when Bitu-based IO handler API is deprecated:
void port_within_proposed(io_port_t port) {
	assert(port < std::numeric_limits<io_port_t_proposed>::max());
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void val_within_proposed(io_val_t val) {
	assert(val <= std::numeric_limits<io_val_t_proposed>::max());
}

// To-be-removed when Bitu-based IO handler API is deprecated:
static io_val_t ReadBlocked(io_port_t /*port*/, Bitu /*iolen*/)
{
	return static_cast<io_val_t>(~0);
}

// To-be-removed when Bitu-based IO handler API is deprecated:
static void WriteBlocked(io_port_t /*port*/, io_val_t /*val*/, Bitu /*iolen*/)
{}

// To-be-removed when Bitu-based IO handler API is deprecated:
static io_val_t ReadDefault(io_port_t port, Bitu iolen);

// To-be-removed when Bitu-based IO handler API is deprecated:
static void WriteDefault(io_port_t port, io_val_t val, Bitu iolen);

// To-be-removed when Bitu-based IO handler API is deprecated:
// The ReadPort and WritePort functions lookup and call the handler
// at the desired port. If the port hasn't been assigned (and the
// lookup is empty), then the default handler is assigned and called.
io_val_t ReadPort(uint8_t req_bytes, io_port_t port)
{
	// Convert bytes to handler map index MB.0x1->0, MW.0x2->1, and MD.0x4->2
	const uint8_t idx = req_bytes >> 1;
	return io_readhandlers[idx].emplace(port, ReadDefault).first->second(port, req_bytes);
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void WritePort(uint8_t put_bytes, io_port_t port, io_val_t val)
{
	// Convert bytes to handler map index MB.0x1->0, MW.0x2->1, and MD.0x4->2
	const uint8_t idx = put_bytes >> 1;

	 // Convert bytes into a cut-off mask: 1->0xff, 2->0xffff, 4->0xffffff
	const auto mask = (1ul << (put_bytes * 8)) - 1;
	io_writehandlers[idx]
	        .emplace(port, WriteDefault)
	        .first->second(port, val & mask, put_bytes);
}

// To-be-removed when Bitu-based IO handler API is deprecated:
static io_val_t ReadDefault(io_port_t port, Bitu iolen)
{
	port_within_proposed(port);

	switch (iolen) {
	case 1:
		LOG(LOG_IO, LOG_WARN)("IOBUS: Unexpected read from %04xh; blocking",
		                      static_cast<uint32_t>(port));
		io_readhandlers[0][port] = ReadBlocked;
		return 0xff;
	case 2: return ReadPort(IO_MB, port) | (ReadPort(IO_MB, port + 1) << 8);
	case 4: return ReadPort(IO_MW, port) | (ReadPort(IO_MW, port + 2) << 16);
	}
	return 0;
}

// To-be-removed when Bitu-based IO handler API is deprecated:
static void WriteDefault(io_port_t port, io_val_t val, Bitu iolen)
{
	port_within_proposed(port);
	val_within_proposed(val);

	switch (iolen) {
	case 1:
		LOG(LOG_IO, LOG_WARN)("IOBUS: Unexpected write of %u to %04xh; blocking",
		                      static_cast<uint32_t>(val),
		                      static_cast<uint32_t>(port));
		io_writehandlers[0][port] = WriteBlocked;
		break;
	case 2:
		WritePort(IO_MB, port, val);
		WritePort(IO_MB, port + 1, val >> 8);
		break;
	case 4:
		WritePort(IO_MW, port, val);
		WritePort(IO_MW, port + 2, val >> 16);
		break;
	}
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void IO_RegisterReadHandler(io_port_t port, IO_ReadHandler handler, Bitu mask, Bitu range)
{
	port_within_proposed(port);

	while (range--) {
		if (mask&IO_MB) io_readhandlers[0][port]=handler;
		if (mask&IO_MW) io_readhandlers[1][port]=handler;
		if (mask&IO_MD) io_readhandlers[2][port]=handler;
		port++;
	}
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void IO_RegisterWriteHandler(io_port_t port, IO_WriteHandler handler, Bitu mask, Bitu range)
{
	port_within_proposed(port);

	while (range--) {
		if (mask&IO_MB) io_writehandlers[0][port]=handler;
		if (mask&IO_MW) io_writehandlers[1][port]=handler;
		if (mask&IO_MD) io_writehandlers[2][port]=handler;
		port++;
	}
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void IO_FreeReadHandler(io_port_t port, Bitu mask, Bitu range)
{
	port_within_proposed(port);

	while (range--) {
		if (mask & IO_MB)
			io_readhandlers[0].erase(port);
		if (mask & IO_MW)
			io_readhandlers[1].erase(port);
		if (mask & IO_MD)
			io_readhandlers[2].erase(port);
		port++;
	}
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void IO_FreeWriteHandler(io_port_t port, Bitu mask, Bitu range)
{
	port_within_proposed(port);

	while (range--) {
		if (mask & IO_MB)
			io_writehandlers[0].erase(port);
		if (mask & IO_MW)
			io_writehandlers[1].erase(port);
		if (mask & IO_MD)
			io_writehandlers[2].erase(port);
		port++;
	}
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void IO_ReadHandleObject::Install(io_port_t port, IO_ReadHandler handler, Bitu mask, Bitu range)
{
	port_within_proposed(port);

	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterReadHandler(port,handler,mask,range);
	} else
		E_Exit("IO_readHandler already installed port %#" PRIxPTR, port);
}

void IO_ReadHandleObject::Uninstall(){
	if(!installed) return;
	IO_FreeReadHandler(m_port,m_mask,m_range); // to be removed
	// casts will be removed after deprecating Bitu handlers
	IO_FreeReadHandler(static_cast<io_port_t_proposed>(m_port), m_width,
	                   static_cast<io_port_t_proposed>(m_range));
	installed = false;
}

IO_ReadHandleObject::~IO_ReadHandleObject(){
	Uninstall();
}

// To-be-removed when Bitu-based IO handler API is deprecated:
void IO_WriteHandleObject::Install(io_port_t port, IO_WriteHandler handler, Bitu mask, Bitu range)
{
	port_within_proposed(port);

	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterWriteHandler(port,handler,mask,range);
	} else
		E_Exit("IO_writeHandler already installed port %#" PRIxPTR, port);
}

void IO_WriteHandleObject::Uninstall() {
	if(!installed) return;
	IO_FreeWriteHandler(m_port,m_mask,m_range); // to be removed
	// casts will be removed after deprecating Bitu handlers
	IO_FreeWriteHandler(static_cast<io_port_t_proposed>(m_port), m_width,
	                    static_cast<io_port_t_proposed>(m_range));
	installed = false;
}

IO_WriteHandleObject::~IO_WriteHandleObject(){
	Uninstall();
	// LOG_MSG("IOBUS: FreeWritehandler called with port %04x",
	// static_cast<uint32_t>(m_port));
}


// type-sized IO handlers
std::unordered_map<io_port_t_proposed, io_read_f> io_read_handlers[io_widths] = {};
constexpr auto &io_read_byte_handler = io_read_handlers[0];
constexpr auto &io_read_word_handler = io_read_handlers[1];
constexpr auto &io_read_dword_handler = io_read_handlers[2];

std::unordered_map<io_port_t_proposed, io_write_f> io_write_handlers[io_widths] = {};
constexpr auto &io_write_byte_handler = io_write_handlers[0];
constexpr auto &io_write_word_handler = io_write_handlers[1];
constexpr auto &io_write_dword_handler = io_write_handlers[2];

// type-sized IO handler API
static uint8_t no_read(const io_port_t_proposed port)
{
	LOG(LOG_IO, LOG_WARN)("IOBUS: Unexpected read from %04xh; blocking", port);
	return 0xff;
}

uint8_t read_byte_from_port(const io_port_t_proposed port)
{
	const auto reader = io_read_byte_handler.find(port);
	const io_val_t_proposed value = reader != io_read_byte_handler.end()
	                           ? reader->second(port, io_width_t::byte)
	                           : no_read(port);
	assert(value <= UINT8_MAX);
	return static_cast<uint8_t>(value);
}

uint16_t read_word_from_port(const io_port_t_proposed port)
{
	const auto reader = io_read_word_handler.find(port);
	const auto value = reader != io_read_word_handler.end()
	                           ? reader->second(port, io_width_t::word)
	                           : static_cast<io_val_t_proposed>(
	                                     read_byte_from_port(port) |
	                                     (read_byte_from_port(port + 1) << 8));
	assert(value <= UINT16_MAX);
	return static_cast<uint16_t>(value);
}

uint32_t read_dword_from_port(const io_port_t_proposed port)
{
	const auto reader = io_read_dword_handler.find(port);
	const auto value = reader != io_read_dword_handler.end()
	                           ? reader->second(port, io_width_t::dword)
	                           : static_cast<io_val_t_proposed>(
	                                     read_word_from_port(port) |
	                                     (read_word_from_port(port + 2) << 16));
	assert(value <= UINT32_MAX);
	return static_cast<uint32_t>(value);
}

static void no_write(const io_port_t_proposed port, const uint8_t val)
{
	LOG(LOG_IO, LOG_WARN)("IOBUS: Unexpected write of %u to %04xh; blocking", val, port);
}

void write_byte_to_port(const io_port_t_proposed port, const uint8_t val)
{
	const auto writer = io_write_byte_handler.find(port);
	if (writer != io_write_byte_handler.end())
		writer->second(port, val, io_width_t::byte);
	else
		no_write(port, val);
}

void write_word_to_port(const io_port_t_proposed port, const uint16_t val)
{
	const auto writer = io_write_word_handler.find(port);
	if (writer != io_write_word_handler.end()) {
		writer->second(port, val, io_width_t::word);
	} else {
		write_byte_to_port(port, val & 0xff);
		write_byte_to_port(port + 1, val >> 8);
	}
}

void write_dword_to_port(const io_port_t_proposed port, const uint32_t val)
{
	const auto writer = io_write_dword_handler.find(port);
	if (writer != io_write_dword_handler.end()) {
		writer->second(port, val, io_width_t::dword);
	} else {
		write_word_to_port(port, val & 0xffff);
		write_word_to_port(port + 2, val >> 16);
	}
}

void IO_RegisterReadHandler(io_port_t_proposed port,
                            const io_read_f handler,
                            const io_width_t max_width,
                            io_port_t_proposed range)
{
	while (range--) {
		io_read_byte_handler[port] = handler;
		if (max_width == io_width_t::word || max_width == io_width_t::dword)
			io_read_word_handler[port] = handler;
		if (max_width == io_width_t::dword)
			io_read_dword_handler[port] = handler;
		++port;
	}
}

void IO_RegisterWriteHandler(io_port_t_proposed port,
                             const io_write_f handler,
                             const io_width_t max_width,
                             io_port_t_proposed range)
{
	while (range--) {
		io_write_byte_handler[port] = handler;
		if (max_width == io_width_t::word || max_width == io_width_t::dword)
			io_write_word_handler[port] = handler;
		if (max_width == io_width_t::dword)
			io_write_dword_handler[port] = handler;
		++port;
	}
}

void IO_FreeReadHandler(io_port_t_proposed port,
                        const io_width_t max_width,
                        io_port_t_proposed range)
{
	while (range--) {
		io_read_byte_handler.erase(port);
		if (max_width == io_width_t::word || max_width == io_width_t::dword)
			io_read_word_handler.erase(port);
		if (max_width == io_width_t::dword)
			io_read_dword_handler.erase(port);
		++port;
	}
}

void IO_FreeWriteHandler(io_port_t_proposed port,
                         const io_width_t width,
                         io_port_t_proposed range)
{
	while (range--) {
		io_write_byte_handler.erase(port);
		if (width == io_width_t::word || width == io_width_t::dword)
			io_write_word_handler.erase(port);
		if (width == io_width_t::dword)
			io_write_dword_handler.erase(port);
		++port;
	}
}

void IO_ReadHandleObject::Install(const io_port_t_proposed port,
                                  const io_read_f handler,
                                  const io_width_t max_width,
                                  const io_port_t_proposed range)
{
	if (!installed) {
		installed = true;
		m_port = port;
		m_width = max_width;
		m_range = range;
		IO_RegisterReadHandler(port, handler, max_width, range);
	} else
		E_Exit("io_read_f already installed port %u", port);
}

void IO_WriteHandleObject::Install(const io_port_t_proposed port,
                                   const io_write_f handler,
                                   const io_width_t max_width,
                                   const io_port_t_proposed range)
{
	if (!installed) {
		installed = true;
		m_port = port;
		m_width = max_width;
		m_range = range;
		IO_RegisterWriteHandler(port, handler, max_width, range);
	} else
		E_Exit("io_write_f already installed port %u", port);
}
