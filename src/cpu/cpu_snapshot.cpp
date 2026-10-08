// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu_snapshot.h"

#include "cpu/registers.h"

CpuRegisters CPU_CaptureRegisters()
{
	CpuRegisters regs = {};
	regs.eax          = reg_eax;
	regs.ebx          = reg_ebx;
	regs.ecx          = reg_ecx;
	regs.edx          = reg_edx;
	regs.esi          = reg_esi;
	regs.edi          = reg_edi;
	regs.ebp          = reg_ebp;
	regs.esp          = reg_esp;
	regs.eip          = reg_eip;
	regs.flags        = reg_flags;
	regs.cs           = SegValue(cs);
	regs.ds           = SegValue(ds);
	regs.es           = SegValue(es);
	regs.ss           = SegValue(ss);
	regs.fs           = SegValue(fs);
	regs.gs           = SegValue(gs);
	return regs;
}
