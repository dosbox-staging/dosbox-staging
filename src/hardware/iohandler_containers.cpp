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

void IO_ReadHandleObject::Uninstall(){
	if(!installed) return;

	IO_FreeReadHandler(m_port, m_width, m_range);
	installed = false;
}

IO_ReadHandleObject::~IO_ReadHandleObject(){
	Uninstall();
}

void IO_WriteHandleObject::Uninstall() {
	if(!installed) return;

	IO_FreeWriteHandler(m_port, m_width, m_range);
	installed = false;
}

IO_WriteHandleObject::~IO_WriteHandleObject(){
	Uninstall();
	// LOG_MSG("IOBUS: FreeWritehandler called with port %04x",
	// static_cast<uint32_t>(m_port));
}

// type-sized IO handlers
std::unordered_map<io_port_t, io_read_f> io_read_handlers[io_widths] = {};
constexpr auto &io_read_byte_handler = io_read_handlers[0];
constexpr auto &io_read_word_handler = io_read_handlers[1];
constexpr auto &io_read_dword_handler = io_read_handlers[2];

std::unordered_map<io_port_t, io_write_f> io_write_handlers[io_widths] = {};
constexpr auto &io_write_byte_handler = io_write_handlers[0];
constexpr auto &io_write_word_handler = io_write_handlers[1];
constexpr auto &io_write_dword_handler = io_write_handlers[2];

// type-sized IO handler API
static uint8_t no_read(const io_port_t port)
{
	LOG(LOG_IO, LOG_WARN)("IOBUS: Unexpected read from %04xh; blocking", port);
	return 0xff;
}

uint8_t read_byte_from_port(const io_port_t port)
{
	const auto reader = io_read_byte_handler.find(port);
	const io_val_t value = reader != io_read_byte_handler.end()
	                           ? (reader->second(port, io_width_t::byte) & 0xff)
	                           : no_read(port);
	assert(value <= UINT8_MAX);
	return static_cast<uint8_t>(value);
}

uint16_t read_word_from_port(const io_port_t port)
{
	const auto reader = io_read_word_handler.find(port);
	const auto value = reader != io_read_word_handler.end()
	                           ? (reader->second(port, io_width_t::word) & 0xffff)
	                           : static_cast<io_val_t>(
	                                     read_byte_from_port(port) |
	                                     (read_byte_from_port(port + 1) << 8));
	assert(value <= UINT16_MAX);
	return static_cast<uint16_t>(value);
}

uint32_t read_dword_from_port(const io_port_t port)
{
	const auto reader = io_read_dword_handler.find(port);
	const auto value = reader != io_read_dword_handler.end()
	                           ? reader->second(port, io_width_t::dword)
	                           : static_cast<io_val_t>(
	                                     read_word_from_port(port) |
	                                     (read_word_from_port(port + 2) << 16));
	assert(value <= UINT32_MAX);
	return static_cast<uint32_t>(value);
}

static void no_write(const io_port_t port, const uint8_t val)
{
	LOG(LOG_IO, LOG_WARN)("IOBUS: Unexpected write of %u to %04xh; blocking", val, port);
}

void write_byte_to_port(const io_port_t port, const uint8_t val)
{
	const auto writer = io_write_byte_handler.find(port);
	if (writer != io_write_byte_handler.end())
		writer->second(port, val, io_width_t::byte);
	else
		no_write(port, val);
}

void write_word_to_port(const io_port_t port, const uint16_t val)
{
	const auto writer = io_write_word_handler.find(port);
	if (writer != io_write_word_handler.end()) {
		writer->second(port, val, io_width_t::word);
	} else {
		write_byte_to_port(port, val & 0xff);
		write_byte_to_port(port + 1, val >> 8);
	}
}

void write_dword_to_port(const io_port_t port, const uint32_t val)
{
	const auto writer = io_write_dword_handler.find(port);
	if (writer != io_write_dword_handler.end()) {
		writer->second(port, val, io_width_t::dword);
	} else {
		write_word_to_port(port, val & 0xffff);
		write_word_to_port(port + 2, val >> 16);
	}
}

void IO_RegisterReadHandler(io_port_t port,
                            const io_read_f handler,
                            const io_width_t max_width,
                            io_port_t range)
{
	// assert(port != 39656);
	while (range--) {
		io_read_byte_handler[port] = handler;
		if (max_width == io_width_t::word || max_width == io_width_t::dword)
			io_read_word_handler[port] = handler;
		if (max_width == io_width_t::dword)
			io_read_dword_handler[port] = handler;
		++port;
	}
}

void IO_RegisterWriteHandler(io_port_t port,
                             const io_write_f handler,
                             const io_width_t max_width,
                             io_port_t range)
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

void IO_FreeReadHandler(io_port_t port,
                        const io_width_t max_width,
                        io_port_t range)
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

void IO_FreeWriteHandler(io_port_t port,
                         const io_width_t width,
                         io_port_t range)
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

void IO_ReadHandleObject::Install(const io_port_t port,
                                  const io_read_f handler,
                                  const io_width_t max_width,
                                  const io_port_t range)
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

void IO_WriteHandleObject::Install(const io_port_t port,
                                   const io_write_f handler,
                                   const io_width_t max_width,
                                   const io_port_t range)
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
