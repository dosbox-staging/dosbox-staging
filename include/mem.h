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

#include "types.h"
#include "mem_host.h"
#include "mem_unaligned.h"

typedef uint32_t PhysPt;
typedef uint8_t * HostPt;
typedef uint32_t RealPt;

typedef int32_t MemHandle;

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

static inline void var_write(uint8_t *var, uint8_t val)
{
	host_writeb(var, val);
}

static inline void var_write(uint16_t * var, uint16_t val) {
	host_writew((HostPt)var, val);
}

static inline void var_write(uint32_t * var, uint32_t val) {
	host_writed((HostPt)var, val);
}

static inline uint16_t var_read(uint16_t * var) {
	return host_readw((HostPt)var);
}

static inline uint32_t var_read(uint32_t * var) {
	return host_readd((HostPt)var);
}

/* The Folowing six functions are slower but they recognize the paged memory system */

uint8_t  mem_readb(PhysPt pt);
uint16_t mem_readw(PhysPt pt);
uint32_t mem_readd(PhysPt pt);

void mem_writeb(PhysPt pt,uint8_t val);
void mem_writew(PhysPt pt,uint16_t val);
void mem_writed(PhysPt pt,uint32_t val);

static inline void phys_writeb(PhysPt addr,uint8_t val) {
	host_writeb(MemBase+addr,val);
}
static inline void phys_writew(PhysPt addr,uint16_t val){
	host_writew(MemBase+addr,val);
}
static inline void phys_writed(PhysPt addr,uint32_t val){
	host_writed(MemBase+addr,val);
}

static inline uint8_t phys_readb(PhysPt addr) {
	return host_readb(MemBase+addr);
}
static inline uint16_t phys_readw(PhysPt addr){
	return host_readw(MemBase+addr);
}
static inline uint32_t phys_readd(PhysPt addr){
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

static inline uint8_t real_readb(uint16_t seg,uint16_t off) {
	return mem_readb((seg<<4)+off);
}
static inline uint16_t real_readw(uint16_t seg,uint16_t off) {
	return mem_readw((seg<<4)+off);
}
static inline uint32_t real_readd(uint16_t seg,uint16_t off) {
	return mem_readd((seg<<4)+off);
}

static inline void real_writeb(uint16_t seg,uint16_t off,uint8_t val) {
	mem_writeb(((seg<<4)+off),val);
}
static inline void real_writew(uint16_t seg,uint16_t off,uint16_t val) {
	mem_writew(((seg<<4)+off),val);
}
static inline void real_writed(uint16_t seg,uint16_t off,uint32_t val) {
	mem_writed(((seg<<4)+off),val);
}


static inline uint16_t RealSeg(RealPt pt) {
	return (uint16_t)(pt>>16);
}

static inline uint16_t RealOff(RealPt pt) {
	return (uint16_t)(pt&0xffff);
}

static inline PhysPt Real2Phys(RealPt pt) {
	return (RealSeg(pt)<<4) +RealOff(pt);
}

static inline PhysPt PhysMake(uint16_t seg,uint16_t off) {
	return (seg<<4)+off;
}

static inline RealPt RealMake(uint16_t seg,uint16_t off) {
	return (seg<<16)+off;
}

static inline void RealSetVec(uint8_t vec,RealPt pt) {
	mem_writed(vec<<2,pt);
}

static inline void RealSetVec(uint8_t vec,RealPt pt,RealPt &old) {
	old = mem_readd(vec<<2);
	mem_writed(vec<<2,pt);
}

static inline RealPt RealGetVec(uint8_t vec) {
	return mem_readd(vec<<2);
}	

#endif

