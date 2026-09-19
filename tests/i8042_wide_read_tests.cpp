// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/input/private/intel8042.h"

#include <cstdint>

#include <gtest/gtest.h>

#include "hardware/port.h"

// The i8042 data (0x60) and status (0x64) ports are registered as dword-width
// handlers so the VMware backdoor can hijack dword accesses. That wide
// registration shadows the IO layer's byte-composition fallback, so a word or
// dword read of these ports must compose the adjacent ports into the high
// bytes itself (matching real ISA hardware) rather than returning a
// zero-extended low byte. These tests lock in that composition.

namespace {

// Byte value served by each port adjacent to 0x60/0x64, indexed by port & 7.
uint8_t adjacent_value[8] = {};

uint8_t read_adjacent(io_port_t port, io_width_t)
{
	return adjacent_value[port & 0x7];
}

class I8042WideRead : public ::testing::Test {
protected:
	void SetUp() override
	{
		// Registers 0x60/0x64 as dword-width read handlers.
		I8042_Init();

		// Serve known values on the ports the composition reads.
		for (const io_port_t port : {0x61, 0x62, 0x63, 0x65, 0x66, 0x67}) {
			IO_RegisterReadHandler(port, read_adjacent, io_width_t::byte);
		}
		for (auto& value : adjacent_value) {
			value = 0;
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

} // namespace
