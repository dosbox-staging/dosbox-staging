// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2022 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "intel8255.h"
#include "dosbox.h"

#include "audio/mixer.h"
#include "util/bitops.h"
#include "util/checks.h"
#include "hardware/timer.h"
#include "hardware/inout.h"

CHECK_NARROWING();

// ***************************************************************************
// Bochs: 8255 Programmable Peripheral Interface
// ***************************************************************************

// 0061    w   KB controller port B (ISA, EISA)   (PS/2 port A is at 0092)
// system control port for compatibility with 8255
// bit 7      (1= IRQ 0 reset )
// bit 6-4    reserved
// bit 3 = 1  channel check enable
// bit 2 = 1  parity check enable
// bit 1 = 1  speaker data enable
// bit 0 = 1  timer 2 gate to speaker enable

// 0061    w   PPI  Programmable Peripheral Interface 8255 (XT only)
// system control port
// bit 7 = 1  clear keyboard
// bit 6 = 0  hold keyboard clock low
// bit 5 = 0  I/O check enable
// bit 4 = 0  RAM parity check enable
// bit 3 = 0  read low switches
// bit 2      reserved, often used as turbo switch
// bit 1 = 1  speaker data enable
// bit 0 = 1  timer 2 gate to speaker enable

void TIMER_SetGate2(bool);

static PpiPortB port_b = {0};

static void write_p61(io_port_t, io_val_t value, io_width_t)
{
	const PpiPortB new_port_b = {check_cast<uint8_t>(value)};

	// Determine how the state changed
	const auto output_changed = new_port_b.timer2_gating_and_speaker_out !=
	                            port_b.timer2_gating_and_speaker_out;
	const auto timer_changed = new_port_b.timer2_gating != port_b.timer2_gating;

	// Update the state
	port_b.data = new_port_b.data;

	if (!is_machine_ega_or_better() && port_b.xt_clear_keyboard) {
		// On XT only, bit 7 is a request to clear keyboard. This is
		// only a pulse, and is normally kept at 0. We "ack" the
		// request by switching the bit back normal (0) state. However,
		// we leave the keyboard as is, because clearing it can cause
		// duplicate key strokes in AlleyCat.
		port_b.xt_clear_keyboard.clear();
	}

	if (!output_changed) {
		return;
	}

	if (timer_changed) {
		TIMER_SetGate2(port_b.timer2_gating);
	}

	PCSPEAKER_SetType(port_b);
}

// Bochs: 8255 Programmable Peripheral Interface

// 0061    r   KB controller port B control register (ISA, EISA)
// system control port for compatibility with 8255
// bit 7    parity check occurred
// bit 6    channel check occurred
// bit 5    mirrors timer 2 output condition
// bit 4    toggles with each refresh request
// bit 3    channel check status
// bit 2    parity check status
// bit 1    speaker data status
// bit 0    timer 2 gate to speaker status

bool TIMER_GetOutput2();

static uint8_t read_p61(io_port_t, io_width_t)
{
	// Bit 4 must be toggled each request
	port_b.read_toggle.flip();

	// On PC/AT systems, bit 5 sets the timer 2 output status
	if (is_machine_ega_or_better()) {
		port_b.timer2_gating_alias = TIMER_GetOutput2();
	} else {
		// On XT systems always toggle bit 5 (Spellicopter CGA)
		port_b.xt_read_toggle.flip();
	}

	return port_b.data;
}

// Bochs: 8255 Programmable Peripheral Interface

// 0062    r/w PPI (XT only)
// bit 7 = 1  RAM parity check
// bit 6 = 1  I/O channel check
// bit 5 = 1  timer 2 channel out
// bit 4      reserved
// bit 3 = 1  system board RAM size type 1
// bit 2 = 1  system board RAM size type 2
// bit 1 = 1  coprocessor installed
// bit 0 = 1  loop in POST

static uint8_t read_p62(io_port_t, io_width_t)
{
	using namespace bit::literals;

	auto ret = bit::all<uint8_t>();

	if (!TIMER_GetOutput2()) {
		bit::clear(ret, b5);
	}

	return ret;
}

// ***************************************************************************
// Initialization
// ***************************************************************************

void I8255_Init()
{
	IO_RegisterWriteHandler(port_num_i8255_1, write_p61, io_width_t::byte);
	IO_RegisterReadHandler(port_num_i8255_1, read_p61, io_width_t::byte);
	if (is_machine_cga() || is_machine_hercules()) {
		IO_RegisterReadHandler(port_num_i8255_2,
		                       read_p62,
		                       io_width_t::byte);
	}

	write_p61(0, 0, io_width_t::byte);
}
