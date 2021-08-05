/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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

#include "../src/hardware/iohandler_containers.cpp"

#include <cassert>
#include <cstdint>

#include <gtest/gtest.h>

// Constants for all tests
// ~~~~~~~~~~~~~~~~~~~~~~~
constexpr uint16_t value_step_size = 4;
constexpr uint16_t port_step_size = 256;
constexpr uint16_t byte_port_start = 4;
constexpr uint16_t word_port_start = byte_port_start * 2;
constexpr uint16_t dword_port_start = word_port_start * 2;

// Byte IO handler functions (Bitu and sized)
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static uint8_t byte_val_new = 0;
static uint8_t read_byte_new(io_port_t, io_width_t)
{
	return byte_val_new;
}
static void write_byte_new(io_port_t, uint8_t val, io_width_t)
{
	byte_val_new = val;
}

// Word IO handler functions (Bitu and sized)
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static uint16_t word_val_new = 0;
static uint16_t read_word_new(io_port_t, io_width_t)
{
	return word_val_new;
}
static void write_word_new(io_port_t, uint16_t val, io_width_t)
{
	word_val_new = val;
}

// Dword IO handler functions (Bitu and sized)
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static uint32_t dword_val_new = 0;
static uint32_t read_dword_new(io_port_t, io_width_t)
{
	return dword_val_new;
}
static void write_dword_new(io_port_t, uint32_t val, io_width_t)
{
	dword_val_new = val;
}

namespace {

TEST(iohandler_containers, valid_bytes)
{
	for (uint32_t p = byte_port_start; p <= UINT16_MAX; p += port_step_size) {
		const auto port = static_cast<uint16_t>(p);

		IO_RegisterWriteHandler(port, write_byte_new, io_width_t::byte);
		IO_RegisterReadHandler(port, read_byte_new, io_width_t::byte);

		for (uint16_t v = 0; v <= UINT8_MAX; v += value_step_size) {
			const auto value = static_cast<uint8_t>(v);

			write_byte_to_port(port, value);
			EXPECT_EQ(v, read_byte_from_port(port));
		}
	}
}

TEST(iohandler_containers, valid_words)
{
	for (uint32_t p = word_port_start; p <= UINT16_MAX; p += port_step_size) {
		const auto port = static_cast<uint16_t>(p);

		IO_RegisterWriteHandler(port, write_word_new, io_width_t::word);
		IO_RegisterReadHandler(port, read_word_new, io_width_t::word);

		for (uint32_t v = 0; v <= UINT16_MAX; v += (value_step_size << 8)) {
			const auto value = static_cast<uint16_t>(v);

			write_word_to_port(port, value);
			EXPECT_EQ(v, read_word_from_port(port));
		}
	}
}

TEST(iohandler_containers, valid_dwords)
{
	for (uint32_t p = dword_port_start; p <= UINT16_MAX; p += port_step_size) {
		const auto port = static_cast<uint16_t>(p);

		IO_RegisterWriteHandler(port, write_dword_new, io_width_t::dword);
		IO_RegisterReadHandler(port, read_dword_new, io_width_t::dword);

		for (uint64_t v = 0; v <= UINT32_MAX; v += (value_step_size << 20)) {
			const auto value = static_cast<uint32_t>(v);

			write_dword_to_port(port, value);
			EXPECT_EQ(v, read_dword_from_port(port));
		}
	}
}

TEST(iohandler_containers, empty_reads)
{
	constexpr uint16_t unregistered = 0;
	EXPECT_EQ(static_cast<uint16_t>(-1),
	          read_word_from_port(unregistered));

	EXPECT_EQ(static_cast<uint32_t>(-1),
	          read_dword_from_port(unregistered));
}

TEST(iohandler_containers, empty_writes)
{
	constexpr uint16_t unregistered = 0;
	write_byte_to_port(unregistered, 0);
}

TEST(iohandler_containers, adjacent_word_read)
{
	constexpr uint8_t val = 0x1;

	write_byte_to_port(byte_port_start, val);

	auto expected = val | 0xff << 8;
	EXPECT_EQ(read_word_from_port(byte_port_start), expected);

	expected = 0xff | val << 8;
	EXPECT_EQ(read_word_from_port(byte_port_start - 1), expected);

	expected = val | 0xff << 8 | 0xff << 16 | 0xff << 24;
	EXPECT_EQ(read_dword_from_port(byte_port_start), expected);

	expected = 0xff | val << 8 | 0xff << 16 | 0xff << 24;
	EXPECT_EQ(read_dword_from_port(byte_port_start - 1), expected);

	expected = 0xff | 0xff << 8 | val << 16 | 0xff << 24;
	EXPECT_EQ(read_dword_from_port(byte_port_start - 2), expected);

	expected = 0xff | 0xff << 8 | 0xff << 16 | val << 24;
	EXPECT_EQ(read_dword_from_port(byte_port_start - 3), expected);
}

TEST(iohandler_containers, adjacent_dword_read)
{
	constexpr uint16_t val = 0x1;

	write_word_to_port(word_port_start, val);

	auto expected = val | 0xffff << 16;
	EXPECT_EQ(read_dword_from_port(word_port_start), expected);

	expected = 0xffff | val << 16;
	EXPECT_EQ(read_dword_from_port(word_port_start - 2), expected);
}

TEST(iohandler_containers, adjacent_word_write)
{
	constexpr uint16_t val = 2 << 8;

	write_word_to_port(byte_port_start - 1, val);

	EXPECT_EQ(read_byte_from_port(byte_port_start), val >> 8);
}

TEST(iohandler_containers, adjacent_dword_write)
{
	constexpr uint32_t val = 2 << 16;

	write_dword_to_port(word_port_start - 2, val);

	EXPECT_EQ(read_word_from_port(word_port_start), val >> 16);
}

} // namespace
