/*
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

/* ARMv4 (little endian) backend by M-HT (common data/functions) */

// some configuring defines that specify the capabilities of this architecture
// or aspects of the recompiling

// protect FC_ADDR over function calls if necessaray
// #define DRC_PROTECT_ADDR_REG

// try to use non-flags generating functions if possible
#define DRC_FLAGS_INVALIDATION
// try to replace _simple functions by code
#define DRC_FLAGS_INVALIDATION_DCODE

// calling convention modifier
#define DRC_CALL_CONV	/* nothing */
#define DRC_FC			/* nothing */

// use FC_REGS_ADDR to hold the address of "cpu_regs" and to access it using FC_REGS_ADDR
#define DRC_USE_REGS_ADDR
// use FC_SEGS_ADDR to hold the address of "Segs" and to access it using FC_SEGS_ADDR
#define DRC_USE_SEGS_ADDR

// register mapping
typedef Bit8u HostReg;

// "lo" registers
#define HOST_r0		 0
#define HOST_r1		 1
#define HOST_r2		 2
#define HOST_r3		 3
#define HOST_r4		 4
#define HOST_r5		 5
#define HOST_r6		 6
#define HOST_r7		 7
// "hi" registers
#define HOST_r8		 8
#define HOST_r9		 9
#define HOST_r10	10
#define HOST_r11	11
#define HOST_r12	12
#define HOST_r13	13
#define HOST_r14	14
#define HOST_r15	15

// register aliases
// "lo" registers
#define HOST_a1 HOST_r0
#define HOST_a2 HOST_r1
#define HOST_a3 HOST_r2
#define HOST_a4 HOST_r3
#define HOST_v1 HOST_r4
#define HOST_v2 HOST_r5
#define HOST_v3 HOST_r6
#define HOST_v4 HOST_r7
// "hi" registers
#define HOST_v5 HOST_r8
#define HOST_v6 HOST_r9
#define HOST_v7 HOST_r10
#define HOST_v8 HOST_r11
#define HOST_ip HOST_r12
#define HOST_sp HOST_r13
#define HOST_lr HOST_r14
#define HOST_pc HOST_r15

static void cache_block_closing(MAYBE_UNUSED const Bit8u *block_start, MAYBE_UNUSED Bitu block_size) { }
