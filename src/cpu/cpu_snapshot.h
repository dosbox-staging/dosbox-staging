// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CPU_SNAPSHOT_H
#define DOSBOX_CPU_SNAPSHOT_H

#include <cstdint>

struct CpuRegisters {
	uint32_t eax   = 0;
	uint32_t ebx   = 0;
	uint32_t ecx   = 0;
	uint32_t edx   = 0;
	uint32_t esi   = 0;
	uint32_t edi   = 0;
	uint32_t ebp   = 0;
	uint32_t esp   = 0;
	uint32_t eip   = 0;
	uint32_t flags = 0;
	uint16_t cs    = 0;
	uint16_t ds    = 0;
	uint16_t es    = 0;
	uint16_t ss    = 0;
	uint16_t fs    = 0;
	uint16_t gs    = 0;
};

CpuRegisters CPU_CaptureRegisters();

#endif // DOSBOX_CPU_SNAPSHOT_H
