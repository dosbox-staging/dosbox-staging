/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2025  The DOSBox Staging Team
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

#include "program_mem.h"

#include "callback.h"
#include "program_more_output.h"
#include "regs.h"

void MEM::Run(void)
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MEM_HELP_LONG"));
		output.Display();
		return;
	}
	/* Show conventional Memory */
	WriteOut("\n");

	uint16_t umb_start   = dos_infoblock.GetStartOfUMBChain();
	uint8_t umb_flag     = dos_infoblock.GetUMBChainState();
	uint8_t old_memstrat = DOS_GetMemAllocStrategy() & 0xff;
	if (umb_start != 0xffff) {
		if ((umb_flag & 1) == 1) {
			DOS_LinkUMBsToMemChain(0);
		}
		DOS_SetMemAllocStrategy(0);
	}

	uint16_t seg, blocks;
	blocks = 0xffff;
	DOS_AllocateMemory(&seg, &blocks);
	WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"), blocks * 16 / 1024);

	if (umb_start != 0xffff) {
		DOS_LinkUMBsToMemChain(1);
		DOS_SetMemAllocStrategy(0x40); // search in UMBs only

		uint16_t largest_block = 0, total_blocks = 0, block_count = 0;
		for (;; block_count++) {
			blocks = 0xffff;
			DOS_AllocateMemory(&seg, &blocks);
			if (blocks == 0) {
				break;
			}
			total_blocks += blocks;
			if (blocks > largest_block) {
				largest_block = blocks;
			}
			DOS_AllocateMemory(&seg, &blocks);
		}

		uint8_t current_umb_flag = dos_infoblock.GetUMBChainState();
		if ((current_umb_flag & 1) != (umb_flag & 1)) {
			DOS_LinkUMBsToMemChain(umb_flag);
		}
		DOS_SetMemAllocStrategy(old_memstrat); // restore strategy

		if (block_count > 0) {
			WriteOut(MSG_Get("PROGRAM_MEM_UPPER"),
			         total_blocks * 16 / 1024,
			         block_count,
			         largest_block * 16 / 1024);
		}
	}

	/* Test for and show free XMS */
	reg_ax = 0x4300;
	CALLBACK_RunRealInt(0x2f);
	if (reg_al == 0x80) {
		reg_ax = 0x4310;
		CALLBACK_RunRealInt(0x2f);

		const uint16_t xms_seg = SegValue(es);
		const uint16_t xms_off = reg_bx;

		reg_ah = 0x88;
		CALLBACK_RunRealFar(xms_seg, xms_off);
		WriteOut(MSG_Get("PROGRAM_MEM_EXTEND"), reg_edx);
	}
	/* Test for and show free EMS */
	uint16_t handle;
	char emm[9] = {'E', 'M', 'M', 'X', 'X', 'X', 'X', '0', 0};
	if (DOS_OpenFile(emm, 0, &handle)) {
		DOS_CloseFile(handle);
		reg_ah = 0x42;
		CALLBACK_RunRealInt(0x67);
		WriteOut(MSG_Get("PROGRAM_MEM_EXPAND"), reg_bx * 16);
	}
}

void MEM::AddMessages()
{
	MSG_Add("PROGRAM_MEM_HELP_LONG",
	        "Display the DOS memory information.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mem[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  This command has no parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  This command shows the DOS memory status, including the free conventional\n"
	        "  memory, UMB (upper) memory, XMS (extended) memory, and EMS (expanded) memory.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]mem[reset]\n"
	        "\n");

	MSG_Add("PROGRAM_MEM_CONVEN", "%10d KB free conventional memory\n\n");
	MSG_Add("PROGRAM_MEM_EXTEND", "%10d KB free extended memory\n\n");
	MSG_Add("PROGRAM_MEM_EXPAND", "%10d KB free expanded memory\n\n");

	MSG_Add("PROGRAM_MEM_UPPER",
	        "%10d KB free upper memory in %d blocks (largest UMB %d KB)\n\n");
}
