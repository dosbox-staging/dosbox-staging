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
static INLINE uint8_t host_readb(const uint8_t *var)
{
	return *var;
}
static INLINE void host_writeb(uint8_t *var, const uint8_t val)
{
	*var = val;
}

// host_to_le functions allow for byte order conversion on big endian
// architectures while respecting memory alignment on low endian.
//
// It is extremely unlikely that we'll ever try to compile on big endian arch
// with a compiler missing __builtin_bswap*, so let's not overcomplicate
// things.
//
// __builtin_bswap* is supported since GCC 4.3 and Clang 3.4

constexpr static INLINE uint8_t host_to_le(uint8_t val) {
	return val;
}

#if defined(WORDS_BIGENDIAN)

constexpr static INLINE int16_t host_to_le(int16_t val) {
	return __builtin_bswap16(val);
}

constexpr static INLINE uint16_t host_to_le(uint16_t val) {
	return __builtin_bswap16(val);
}

constexpr static INLINE uint32_t host_to_le(uint32_t val) {
	return __builtin_bswap32(val);
}

constexpr static INLINE uint64_t host_to_le(uint64_t val)
{
	return __builtin_bswap64(val);
}

#else

constexpr static INLINE int16_t host_to_le(int16_t val) {
	return val;
}

constexpr static INLINE uint16_t host_to_le(uint16_t val) {
	return val;
}

constexpr static INLINE uint32_t host_to_le(uint32_t val) {
	return val;
}

constexpr static INLINE uint64_t host_to_le(uint64_t val)
{
	return val;
}

#endif

constexpr static INLINE uint8_t le_to_host(uint8_t val)
{
	return host_to_le(val);
}

constexpr static INLINE int16_t le_to_host(int16_t val)
{
	return host_to_le(val);
}

constexpr static INLINE uint16_t le_to_host(uint16_t val)
{
	return host_to_le(val);
}

constexpr static INLINE uint32_t le_to_host(uint32_t val)
{
	return host_to_le(val);
}

constexpr static INLINE uint64_t le_to_host(uint64_t val)
{
	return host_to_le(val);
}

// Read, write, and add using 16-bit words
static INLINE uint16_t host_readw(const uint8_t *arr)
{
	uint16_t val;
	memcpy(&val, arr, sizeof(val));
	// array sequence was DOS little-endian, so convert value to host-type
	return le_to_host(val);
}

static INLINE void host_writew(uint8_t *arr, uint16_t val)
{
	// Convert the host-type value to little-endian before filling array
	val = host_to_le(val);
	memcpy(arr, &val, sizeof(val));
}

static INLINE void host_addw(uint8_t *arr, const uint16_t incr)
{
	const uint16_t val = host_readw(arr) + incr;
	host_writew(arr, val);
}

// Read, write, and add using 32-bit double-words
static INLINE uint32_t host_readd(const uint8_t *arr)
{
	uint32_t val;
	memcpy(&val, arr, sizeof(val));
	// array sequence was DOS little-endian, so convert value to host-type
	return le_to_host(val);
}

static INLINE void host_writed(uint8_t *arr, uint32_t val)
{
	// Convert the host-type value to little-endian before filling array
	val = host_to_le(val);
	memcpy(arr, &val, sizeof(val));
}

static INLINE void host_addd(uint8_t *arr, const uint32_t incr)
{
	const uint32_t val = host_readd(arr) + incr;
	host_writed(arr, val);
}

// Read and write using 64-bit quad-words
static INLINE uint64_t host_readq(const uint8_t *arr)
{
	uint64_t val;
	memcpy(&val, arr, sizeof(val));
	// array sequence was DOS little-endian, so convert value to host-type
	return le_to_host(val);
}

static INLINE void host_writeq(uint8_t *arr, uint64_t val)
{
	// Convert the host-type value to little-endian before filling array
	val = host_to_le(val);
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

