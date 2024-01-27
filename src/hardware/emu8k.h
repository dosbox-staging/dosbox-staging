/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_EMU8000_H
#define DOSBOX_EMU8000_H

#include "dosbox.h"

#include "bit_view.h"


namespace Emu8kPortOffset {

constexpr Data0   = 0x0400; // read and write of DWORD data
constexpr Data1   = 0x0800; // read and write of WORD and DWORD data
constexpr Data2   = 0x0802; // read and write of WORD data
constexpr Data3   = 0x0c00; // read and write of WORD data
constexpr Pointer = 0x0c02; // read and write of register pointer value (WORD)

}; // end namespace Emu8kPortOffset


enum Emu8kPort {
	Data0, Data1, Data2, Data3, Pointer	
}


namespace Emu8kRegister {

union Pointer {
	uint16_t data = 0;
	bit_view<0, 5> channel_number;
	bit_view<5, 3> register_number;
	bit_view<8, 8> unused; // conventionally zero for writes, random data for reads
}

// Current Pitch and Fractional Address
union CPF {
	uint32_t data = 0;
	bit_view<0, 16> curr_pitch;  // current pitch, 0x4000 = no pitch shift
	bit_view<16, 16> fract_address; // fractional address
};

// Pitch Target, Rvb Send, and Aux Byte
union PTRX {
	uint32_t data = 0;
	bit_view<0, 8> aux_data; // unused
	bit_view<8, 8> reverb_send;
	bit_view<16, 16> pitch_target;
};

// Current Volume and Filter Cutoff
union CVCF {
	uint32_t data = 0;
	bit_view<0, 16> curr_filter_cutoff;
	bit_view<16, 16> curr_volume;
};

// Volume and Filter Cutoff Targets
union VTFT {
	uint32_t data = 0;
	bit_view<0, 16> filter_cutoff_target;
	bit_view<16, 16> volume_target;
};

// Pan Send and Loop Start Address
union PSST {
	uint32_t data = 0;
	bit_view<0, 24> loop_start_address; // actual loop start point is one greater
	bit_view<24, 8> pan_send; // 0x00 = right
							  // 0xff = left
};

// Chorus Send and Loop End Address
union CSL {
	uint32_t data = 0;
	bit_view<0, 24> loop_end_address; // actual loop end point is one greater
	bit_view<24, 8> chorus_send; // 0x00 = none
								 // 0xff = maximum
};

// Q, Control Bits, and Current Address
union CCCA {
	uint32_t data = 0;
	bit_view<0, 24> curr_address;
	bit_view<24, 1> right; // 1 = right, 0 = left
	bit_view<25, 1> write; // 1 = write, 0 = read
	bit_view<26, 1> dma;
	bit_view<27, 1> zero; // always zero
	bit_view<28, 4> filter_resonance; // 0 = no resonance
									  // 15 = ~24dB resonance
};

// Sound emory Address for Left/Right Reads/Writes
// These four registers have the same layout: SMALR, SMARR, SMALW, SMARW
union SMA {
	uint32_t data = 0;
	bit_view<0, 24> sound_memory_address;
	bit_view<24, 7> zero; // always zero
	bit_view<31, 1> empty;
};

// Volume/Modulation Envelope Sustain and Decay
// These two registers have the same layout: DCYSUSV, DCYSUS
union DCYSUS {
	uint16_t data = 0;
	bit_view<0, 7> decay_or_release_rate;
	bit_view<7, 0> zero; // always zero
	bit_view<8, 7> sustain_level; // 0x7f = no attenuation
								  // 0x00 = zero level
	bit_view<15, 1> ph1; // 0 = decay, 1 = release
};

// Volume/Modulation Envelope Hold and Attack
// These two registers have the same layout: ATKHLDV, ATKHLD 
union ATKHLD {
	uint16_t data = 0;
	bit_view<0, 7> attac,_time; // 0x00 = no attack
								// 0x01 = 11.88s
								// 0x7f = 6msec
	bit_view<7, 0> zero1; // always zero
	bit_view<8, 6> hold_time; // 0x7f = no hold time
							  // 0x00 = 11.68s
	bit_view<15, 1> zero2; // always zero
};

// Initial Filter Cutoff and Attenuation
union IFATN {
	uint16_t data = 0;
	bit_view<0, 7> attenuation; // 0.375dB steps
								// 0x00 = no attenuation
								// 0xff = 96dB
	bit_view<8, 16> initial_filter_cutoff; // quarter semitone steps
										   // 0x00 = 125Hz
										   // 0xff = 8kHz (or off it filter_resonance == 0)
	bit_view<8, 6> mod_env_hold_time; // 0x7f = no hold time
									  // 0x00 = 11.68s
	bit_view<15, 1> zero2; // always zero
};

}; // end namespace Emu8kRegister


class Emu8k {
public:

private:
	uint8_t curr_register = {};
	uint8_t curr_channel = {};
};

#endif
