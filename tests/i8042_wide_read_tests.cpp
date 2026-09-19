// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/input/private/intel8042.h"

#include <cstdint>

#include <gtest/gtest.h>

#include "hardware/port.h"

// The i8042 data (0x60) and status (0x64) ports are registered as dword-width
// handlers so the VMware backdoor can hijack dword accesses. That wide
// registration shadows the IO layer's byte-composition fallback, so a word or
// dword access of these ports must compose the adjacent ports into the high
// bytes itself (matching real ISA hardware) rather than returning a
// zero-extended low byte (reads) or dropping the high bytes (writes). These
// tests lock in that composition.

namespace {

// Ports the wide composition reads from / writes to around 0x60 and 0x64.
constexpr io_port_t adjacent_ports[] = {0x61, 0x62, 0x63, 0x65, 0x66, 0x67};

// Byte value served by each adjacent port, indexed by port & 7.
uint8_t adjacent_value[8] = {};

// Byte captured from each adjacent port by wide writes, indexed by port & 7.
uint8_t adjacent_written[8] = {};

uint8_t read_adjacent(io_port_t port, io_width_t)
{
	return adjacent_value[port & 0x7];
}

void write_adjacent(io_port_t port, io_val_t value, io_width_t)
{
	adjacent_written[port & 0x7] = static_cast<uint8_t>(value);
}

// A controller command byte that touches no external subsystem and no
// persistent controller state: it is unknown, outside every command range, and
// not remapped by the AMI BIOS aliasing, so write_command_port only logs a
// warning for it. Used as the low byte of wide writes to 0x64 so the assertions
// can focus on the forwarded high bytes.
constexpr uint8_t InertCommand = 0x9c;

class I8042WideRead : public ::testing::Test {
protected:
	void SetUp() override
	{
		// Registers 0x60/0x64 as dword-width handlers.
		I8042_Init();

		// Serve / capture known values on the ports the composition
		// touches.
		for (const io_port_t port : adjacent_ports) {
			IO_RegisterReadHandler(port, read_adjacent, io_width_t::byte);
			IO_RegisterWriteHandler(port, write_adjacent, io_width_t::byte);
		}
		for (auto& value : adjacent_value) {
			value = 0;
		}
		for (auto& value : adjacent_written) {
			value = 0;
		}
	}

	void TearDown() override
	{
		// The IO handler maps are process-wide and shared with every
		// other test in the binary, so remove everything this fixture
		// installed to avoid leaking into later tests.
		IO_FreeReadHandler(port_num_i8042_data, io_width_t::dword);
		IO_FreeReadHandler(port_num_i8042_status, io_width_t::dword);
		IO_FreeWriteHandler(port_num_i8042_data, io_width_t::byte);
		IO_FreeWriteHandler(port_num_i8042_command, io_width_t::dword);

		for (const io_port_t port : adjacent_ports) {
			IO_FreeReadHandler(port, io_width_t::byte);
			IO_FreeWriteHandler(port, io_width_t::byte);
		}
	}
};

TEST_F(I8042WideRead, word_read_of_data_port_composes_0x61)
{
	adjacent_value[0x61 & 0x7] = 0xa5;

	// High byte comes from port 0x61 (0x00 before the fix).
	EXPECT_EQ((IO_ReadW(0x60) >> 8) & 0xff, 0xa5);
}

TEST_F(I8042WideRead, dword_read_of_data_port_composes_0x61_0x62_0x63)
{
	adjacent_value[0x61 & 0x7] = 0xa5;
	adjacent_value[0x62 & 0x7] = 0x5a;
	adjacent_value[0x63 & 0x7] = 0x3c;

	const auto value = IO_ReadD(0x60);
	EXPECT_EQ((value >> 8) & 0xff, 0xa5);
	EXPECT_EQ((value >> 16) & 0xff, 0x5a);
	EXPECT_EQ((value >> 24) & 0xff, 0x3c);
}

TEST_F(I8042WideRead, word_read_of_status_port_composes_0x65)
{
	adjacent_value[0x65 & 0x7] = 0xc3;

	// High byte comes from port 0x65 (0x00 before the fix).
	EXPECT_EQ((IO_ReadW(0x64) >> 8) & 0xff, 0xc3);
}

TEST_F(I8042WideRead, dword_read_of_status_port_composes_0x65_0x66_0x67)
{
	adjacent_value[0x65 & 0x7] = 0xc3;
	adjacent_value[0x66 & 0x7] = 0x3c;
	adjacent_value[0x67 & 0x7] = 0x99;

	// The low byte must be the status register (proving the dword dispatch
	// reaches read_status_register rather than read_data_port); the high
	// bytes come from the composed adjacent ports.
	const auto value = IO_ReadD(0x64);
	EXPECT_EQ(value & 0xff, IO_ReadB(0x64));
	EXPECT_EQ((value >> 8) & 0xff, 0xc3);
	EXPECT_EQ((value >> 16) & 0xff, 0x3c);
	EXPECT_EQ((value >> 24) & 0xff, 0x99);
}

TEST_F(I8042WideRead, word_write_of_command_port_forwards_0x65)
{
	// High byte must be forwarded to port 0x65 (dropped before the fix).
	const uint16_t value = static_cast<uint16_t>((0xa5 << 8) | InertCommand);
	IO_WriteW(0x64, value);

	EXPECT_EQ(adjacent_written[0x65 & 0x7], 0xa5);
}

TEST_F(I8042WideRead, dword_write_of_command_port_forwards_0x65_0x66_0x67)
{
	// High bytes must be forwarded to ports 0x65/0x66/0x67 in order.
	const uint32_t value = (0x3cu << 24) | (0x5au << 16) | (0xa5u << 8) |
	                       InertCommand;
	IO_WriteD(0x64, value);

	EXPECT_EQ(adjacent_written[0x65 & 0x7], 0xa5);
	EXPECT_EQ(adjacent_written[0x66 & 0x7], 0x5a);
	EXPECT_EQ(adjacent_written[0x67 & 0x7], 0x3c);
}

} // namespace
