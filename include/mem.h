/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_MEM_H
#define DOSBOX_MEM_H

#include "dosbox.h"

#include "mem_host.h"
#include "mem_unaligned.h"
#include "types.h"

typedef uint32_t PhysPt;
typedef uint8_t *HostPt;
typedef uint32_t RealPt;
typedef int32_t MemHandle;

#define MEM_PAGESIZE 4096

extern HostPt MemBase;
HostPt GetMemBase();

bool MEM_A20_Enabled();
void MEM_A20_Enable(bool enable);

/* Memory management / EMS mapping */
HostPt MEM_GetBlockPage();
Bitu MEM_FreeTotal();                      // Free 4 KiB pages
Bitu MEM_FreeLargest();                    // Largest free 4 KiB pages block
Bitu MEM_TotalPages();                     // Total amount of 4 KiB pages
Bitu MEM_AllocatedPages(MemHandle handle); // amount of allocated pages of handle
MemHandle MEM_AllocatePages(Bitu pages, bool sequence);
MemHandle MEM_GetNextFreePage();
PhysPt MEM_AllocatePage();
void MEM_ReleasePages(MemHandle handle);
bool MEM_ReAllocatePages(MemHandle &handle, Bitu pages, bool sequence);
void MEM_RemoveEMSPageFrame();
void MEM_PreparePCJRCartRom();

MemHandle MEM_NextHandle(MemHandle handle);
MemHandle MEM_NextHandleAt(MemHandle handle, Bitu where);

static inline void var_write(uint8_t *var, uint8_t val)
{
	host_writeb(var, val);
}

static inline void var_write(uint16_t *var, uint16_t val)
{
	host_writew((HostPt)var, val);
}

static inline void var_write(uint32_t *var, uint32_t val)
{
	host_writed((HostPt)var, val);
}

static inline uint16_t var_read(uint16_t *var)
{
	return host_readw((HostPt)var);
}

static inline uint32_t var_read(uint32_t *var)
{
	return host_readd((HostPt)var);
}

/* The Following six functions are slower but they recognize the paged memory
 * system */

uint8_t mem_readb(PhysPt pt);
uint16_t mem_readw(PhysPt pt);
uint32_t mem_readd(PhysPt pt);

void mem_writeb(PhysPt pt, uint8_t val);
void mem_writew(PhysPt pt, uint16_t val);
void mem_writed(PhysPt pt, uint32_t val);

static inline void phys_writeb(PhysPt addr, uint8_t val)
{
	host_writeb(MemBase + addr, val);
}

static inline void phys_writew(PhysPt addr, uint16_t val)
{
	host_writew(MemBase + addr, val);
}

static inline void phys_writed(PhysPt addr, uint32_t val)
{
	host_writed(MemBase + addr, val);
}

static inline uint8_t phys_readb(PhysPt addr)
{
	return host_readb(MemBase + addr);
}

static inline uint16_t phys_readw(PhysPt addr)
{
	return host_readw(MemBase + addr);
}

static inline uint32_t phys_readd(PhysPt addr)
{
	return host_readd(MemBase + addr);
}

/* These don't check for alignment, better be sure it's correct */

void MEM_BlockWrite(PhysPt pt, const void *data, size_t size);
void MEM_BlockRead(PhysPt pt, void *data, Bitu size);
void MEM_BlockCopy(PhysPt dest, PhysPt src, Bitu size);
void MEM_StrCopy(PhysPt pt, char *data, Bitu size);

void mem_memcpy(PhysPt dest, PhysPt src, Bitu size);
Bitu mem_strlen(PhysPt pt);
void mem_strcpy(PhysPt dest, PhysPt src);

/* The following functions are all shortcuts to the above functions using
 * physical addressing */

static inline uint8_t real_readb(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	return mem_readb(base + off);
}

static inline uint16_t real_readw(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	return mem_readw(base + off);
}

static inline uint32_t real_readd(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	return mem_readd(base + off);
}

static inline void real_writeb(uint16_t seg, uint16_t off, uint8_t val)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	mem_writeb(base + off, val);
}

static inline void real_writew(uint16_t seg, uint16_t off, uint16_t val)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	mem_writew(base + off, val);
}

static inline void real_writed(uint16_t seg, uint16_t off, uint32_t val)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	mem_writed(base + off, val);
}

static inline uint16_t RealSeg(RealPt pt)
{
	return pt >> 16;
}

static inline uint16_t RealOff(RealPt pt)
{
	return static_cast<uint16_t>(pt & 0xffff);
}

static inline PhysPt Real2Phys(RealPt pt)
{
	const auto base = static_cast<uint32_t>(RealSeg(pt) << 4);
	return base + RealOff(pt);
}

static inline PhysPt PhysMake(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	return base + off;
}

static inline RealPt RealMake(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 16);
	return base + off;
}

static inline void RealSetVec(uint8_t vec, RealPt pt)
{
	const auto target = static_cast<uint16_t>(vec << 2);
	mem_writed(target, pt);
}

// TODO: consider dropping this function. The three places where it's called all
// ignore the 3rd updated parameter.
static inline void RealSetVec(uint8_t vec, RealPt pt, RealPt &old)
{
	const auto target = static_cast<uint16_t>(vec << 2);
	old = mem_readd(target);
	mem_writed(target, pt);
}

static inline RealPt RealGetVec(uint8_t vec)
{
	const auto target = static_cast<uint16_t>(vec << 2);
	return mem_readd(target);
}

#endif
