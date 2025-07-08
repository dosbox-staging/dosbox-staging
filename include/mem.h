// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MEM_H
#define DOSBOX_MEM_H

#include "dosbox.h"

#include "mem_host.h"
#include "mem_unaligned.h"
#include "types.h"

constexpr uint16_t MemPageSize     = 4096;
constexpr uint8_t  RealSegmentSize = 16;

typedef uint32_t PhysPt;
typedef uint8_t* HostPt;
typedef uint32_t RealPt;
typedef int32_t MemHandle;

using PhysAddress = struct PhysAddress {
	PhysAddress(const uint16_t segment, const uint16_t offset)
	        : segment(segment), offset(offset) {}

	uint16_t segment = {};
	uint16_t offset  = {};
};

using RealAddress = struct RealAddress {
	RealAddress(const uint16_t segment, const uint16_t offset)
	        : segment(segment), offset(offset) {}

	uint16_t segment = {};
	uint16_t offset  = {};
};

extern HostPt MemBase;
HostPt GetMemBase();

uint16_t MEM_GetMinMegabytes();
uint16_t MEM_GetMaxMegabytes();

bool MEM_A20_Enabled();
void MEM_A20_Enable(bool enable);

/* Memory management / EMS mapping */
HostPt MEM_GetBlockPage();
uint32_t MEM_FreeTotal();                      // free 4 KB pages
uint32_t MEM_FreeLargest();                    // largest free 4 KB pages block
uint32_t MEM_TotalPages();                     // total amount of 4 KB pages
uint32_t MEM_AllocatedPages(MemHandle handle); // amount of allocated pages of handle
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

static inline void var_write(uint64_t* var, uint64_t val)
{
	host_writeq((HostPt)var, val);
}

static inline uint16_t var_read(uint16_t* var)
{
	return host_readw((HostPt)var);
}

static inline uint32_t var_read(uint32_t *var)
{
	return host_readd((HostPt)var);
}

static inline uint64_t var_read(uint64_t* var)
{
	return host_readq((HostPt)var);
}

/* The Following six functions are slower but they recognize the paged memory
 * system */

enum class MemOpMode {
	WithBreakpoints,
	SkipBreakpoints,
};

template <MemOpMode op_mode = MemOpMode::WithBreakpoints>
uint8_t mem_readb(const PhysPt pt);

template <MemOpMode op_mode = MemOpMode::WithBreakpoints>
uint16_t mem_readw(const PhysPt pt);

template <MemOpMode op_mode = MemOpMode::WithBreakpoints>
uint32_t mem_readd(const PhysPt pt);

template <MemOpMode op_mode = MemOpMode::WithBreakpoints>
uint64_t mem_readq(const PhysPt pt);

void mem_writeb(PhysPt pt, uint8_t val);
void mem_writew(PhysPt pt, uint16_t val);
void mem_writed(PhysPt pt, uint32_t val);
void mem_writeq(PhysPt pt, uint64_t val);

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

static inline void phys_writeq(PhysPt addr, uint64_t val)
{
	host_writeq(MemBase + addr, val);
}

static inline void phys_writes(PhysPt addr, const std::string& string)
{
	auto destination = MemBase + addr;
	for (auto character : string) {
		host_writeb(destination++, character);
	}
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

static inline uint64_t phys_readq(PhysPt addr)
{
	return host_readq(MemBase + addr);
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

static inline uint64_t real_readq(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	return mem_readq(base + off);
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

static inline void real_writeq(uint16_t seg, uint16_t off, uint64_t val)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	mem_writeq(base + off, val);
}

static inline uint16_t RealSegment(RealPt pt)
{
	return static_cast<uint16_t>(pt >> 16);
}

static inline uint16_t RealOffset(RealPt pt)
{
	return static_cast<uint16_t>(pt & 0xffff);
}

static inline PhysPt RealToPhysical(RealPt pt)
{
	const auto base = static_cast<uint32_t>(RealSegment(pt) << 4);
	return base + RealOffset(pt);
}

static inline PhysPt PhysicalMake(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 4);
	return base + off;
}

static inline PhysPt PhysicalMake(const PhysAddress address)
{
	return PhysicalMake(address.segment, address.offset);
}

static inline RealPt RealMake(uint16_t seg, uint16_t off)
{
	const auto base = static_cast<uint32_t>(seg << 16);
	return base + off;
}

static inline PhysPt RealMake(const RealAddress address)
{
	return RealMake(address.segment, address.offset);
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
