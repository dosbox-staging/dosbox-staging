/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#include <cstring>

#include "types.h"
#include "byteorder.h"

typedef Bit32u PhysPt;
typedef Bit8u * HostPt;
typedef Bit32u RealPt;

typedef Bit32s MemHandle;

#define MEM_PAGESIZE 4096

extern HostPt MemBase;
HostPt GetMemBase(void);

bool MEM_A20_Enabled(void);
void MEM_A20_Enable(bool enable);

/* Memory management / EMS mapping */
HostPt MEM_GetBlockPage(void);
Bitu MEM_FreeTotal(void);			//Free 4 kb pages
Bitu MEM_FreeLargest(void);			//Largest free 4 kb pages block
Bitu MEM_TotalPages(void);			//Total amount of 4 kb pages
Bitu MEM_AllocatedPages(MemHandle handle); // amount of allocated pages of handle
MemHandle MEM_AllocatePages(Bitu pages,bool sequence);
MemHandle MEM_GetNextFreePage(void);
PhysPt MEM_AllocatePage(void);
void MEM_ReleasePages(MemHandle handle);
bool MEM_ReAllocatePages(MemHandle & handle,Bitu pages,bool sequence);

MemHandle MEM_NextHandle(MemHandle handle);
MemHandle MEM_NextHandleAt(MemHandle handle,Bitu where);

// Read and write single-byte values
constexpr uint8_t host_readb(const uint8_t *var)
{
	return *var;
}

static INLINE void host_writeb(uint8_t *var, const uint8_t val)
{
	*var = val;
}

/*  Read a uint16 from 8-bit native byte-ordered memory. Use this
 *  instead of *(uint16_t*)(8_bit_ptr) or *(uint16_t*)(8_bit_ptr + offset)
 */
static INLINE uint16_t read_uint16(const uint8_t *arr)
{
	uint16_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

/*  Read an array-indexed uint16 from 8-bit native byte-ordered memory.
 *  Use this instead of ((uint16_t*)arr)[index]
 */
static INLINE uint16_t read_uint16_at(const uint8_t *arr, const uintptr_t index)
{
	return read_uint16(arr + index * sizeof(uint16_t));
}
static INLINE uint16_t host_readw_at(const uint8_t *arr, const uintptr_t index)
{
	return host_readw(arr + index * sizeof(uint16_t));
}

// Write a uint16 to 8-bit memory using native byte-ordering.
static INLINE void write_uint16(uint8_t *arr, uint16_t val)
{
	memcpy(arr, &val, sizeof(val));
}


// Write an array-indexed uint16 to 8-bit memory using native byte-ordering.
static INLINE void write_uint16_at(uint8_t *arr, const uintptr_t index, const uint16_t val)
{
	write_uint16(arr + index * sizeof(uint16_t), val);
}
static INLINE void host_writew_at(uint8_t *arr, const uintptr_t index, const uint16_t val)
{
	host_writew(arr + index * sizeof(uint16_t), val);
}

// Increment a uint16 value held in 8-bit native byte-ordered memory.
static INLINE void incr_uint16(uint8_t *arr, const uint16_t incr)
{
	write_uint16(arr, read_uint16(arr) + incr);
}

// Read, write, and add using 32-bit double-words
static INLINE uint32_t host_readd(const uint8_t *arr)
{
	uint32_t val;
	memcpy(&val, arr, sizeof(val));
// Read an array-indexed uint32 from 8-bit native byte-ordered memory.
static INLINE uint32_t read_uint32_at(const uint8_t *arr, const uintptr_t index)
{
	return read_uint32(arr + index * sizeof(uint32_t));
}

// Like the above, but allows index-style access assuming a 32-bit array
static INLINE uint32_t host_readd_at(const uint8_t *arr, const uintptr_t index)
{
	return host_readd(arr + index * sizeof(uint32_t));
}

// Write a uint32 to 8-bit memory using native byte-ordering.
static INLINE void write_uint32(uint8_t *arr, uint32_t val)
{
	memcpy(arr, &val, sizeof(val));
}
static INLINE void host_writed(uint8_t *arr, uint32_t val)
{
	// Convert the host-type value to little-endian before filling array
	val = host_to_le32(val);
	memcpy(arr, &val, sizeof(val));
}

static INLINE void host_writed_at(uint8_t *arr, const uintptr_t index, const uint32_t val)
{
	host_writed(arr + index * sizeof(uint32_t), val);
}

// Increment a uint32 value held in 8-bit native byte-ordered memory.
static INLINE void incr_uint32(uint8_t *arr, const uint32_t incr)
{
	write_uint32(arr, read_uint32(arr) + incr);
}
{
	const uint32_t val = host_readd(arr) + incr;
	host_writed(arr, val);
}

// Read a uint64 from 8-bit native byte-ordered memory.
static INLINE uint64_t read_uint64(const uint8_t *arr)
{
	uint64_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

// Write a uint64 to 8-bit memory using native byte-ordering.
static INLINE void write_uint64(uint8_t *arr, uint64_t val)
{
	memcpy(arr, &val, sizeof(val));
}

static INLINE void var_write(uint8_t *var, uint8_t val)
{
	host_writeb(var, val);
}

static INLINE void var_write(Bit16u * var, Bit16u val) {
	host_writew((HostPt)var, val);
}

static INLINE void var_write(Bit32u * var, Bit32u val) {
	host_writed((HostPt)var, val);
}

static INLINE Bit16u var_read(Bit16u * var) {
	return host_readw((HostPt)var);
}

static INLINE Bit32u var_read(Bit32u * var) {
	return host_readd((HostPt)var);
}

/* The Folowing six functions are slower but they recognize the paged memory system */

Bit8u  mem_readb(PhysPt pt);
Bit16u mem_readw(PhysPt pt);
Bit32u mem_readd(PhysPt pt);

void mem_writeb(PhysPt pt,Bit8u val);
void mem_writew(PhysPt pt,Bit16u val);
void mem_writed(PhysPt pt,Bit32u val);

static INLINE void phys_writeb(PhysPt addr,Bit8u val) {
	host_writeb(MemBase+addr,val);
}
static INLINE void phys_writew(PhysPt addr,Bit16u val){
	host_writew(MemBase+addr,val);
}
static INLINE void phys_writed(PhysPt addr,Bit32u val){
	host_writed(MemBase+addr,val);
}

static INLINE Bit8u phys_readb(PhysPt addr) {
	return host_readb(MemBase+addr);
}
static INLINE Bit16u phys_readw(PhysPt addr){
	return host_readw(MemBase+addr);
}
static INLINE Bit32u phys_readd(PhysPt addr){
	return host_readd(MemBase+addr);
}

/* These don't check for alignment, better be sure it's correct */

void MEM_BlockWrite(PhysPt pt,void const * const data,Bitu size);
void MEM_BlockRead(PhysPt pt,void * data,Bitu size);
void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size);
void MEM_StrCopy(PhysPt pt,char * data,Bitu size);

void mem_memcpy(PhysPt dest,PhysPt src,Bitu size);
Bitu mem_strlen(PhysPt pt);
void mem_strcpy(PhysPt dest,PhysPt src);

/* The folowing functions are all shortcuts to the above functions using physical addressing */

static INLINE Bit8u real_readb(Bit16u seg,Bit16u off) {
	return mem_readb((seg<<4)+off);
}
static INLINE Bit16u real_readw(Bit16u seg,Bit16u off) {
	return mem_readw((seg<<4)+off);
}
static INLINE Bit32u real_readd(Bit16u seg,Bit16u off) {
	return mem_readd((seg<<4)+off);
}

static INLINE void real_writeb(Bit16u seg,Bit16u off,Bit8u val) {
	mem_writeb(((seg<<4)+off),val);
}
static INLINE void real_writew(Bit16u seg,Bit16u off,Bit16u val) {
	mem_writew(((seg<<4)+off),val);
}
static INLINE void real_writed(Bit16u seg,Bit16u off,Bit32u val) {
	mem_writed(((seg<<4)+off),val);
}


static INLINE Bit16u RealSeg(RealPt pt) {
	return (Bit16u)(pt>>16);
}

static INLINE Bit16u RealOff(RealPt pt) {
	return (Bit16u)(pt&0xffff);
}

static INLINE PhysPt Real2Phys(RealPt pt) {
	return (RealSeg(pt)<<4) +RealOff(pt);
}

static INLINE PhysPt PhysMake(Bit16u seg,Bit16u off) {
	return (seg<<4)+off;
}

static INLINE RealPt RealMake(Bit16u seg,Bit16u off) {
	return (seg<<16)+off;
}

static INLINE void RealSetVec(Bit8u vec,RealPt pt) {
	mem_writed(vec<<2,pt);
}

static INLINE void RealSetVec(Bit8u vec,RealPt pt,RealPt &old) {
	old = mem_readd(vec<<2);
	mem_writed(vec<<2,pt);
}

static INLINE RealPt RealGetVec(Bit8u vec) {
	return mem_readd(vec<<2);
}	

#endif

