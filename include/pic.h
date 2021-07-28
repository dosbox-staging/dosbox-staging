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

#ifndef DOSBOX_PIC_H
#define DOSBOX_PIC_H


/* CPU Cycle Timing */
extern int32_t CPU_Cycles;
extern int32_t CPU_CycleLeft;
extern int32_t CPU_CycleMax;

typedef void(PIC_EOIHandler)();
typedef void (*PIC_EventHandler)(uint32_t val);

extern uint32_t PIC_IRQCheck;

// Elapsed milliseconds since starting DOSBox
// Holds ~4.2 B milliseconds or ~48 days before rolling over
extern uint32_t PIC_Ticks;

// The number of cycles not done yet (ND)
static INLINE int32_t PIC_TickIndexND()
{
	return CPU_CycleMax - CPU_CycleLeft - CPU_Cycles;
}

// Returns the percent cycles completed within the current "millisecond tick" of
// the CPU
static INLINE float PIC_TickIndex()
{
	return static_cast<float>(PIC_TickIndexND()) /
	       static_cast<float>(CPU_CycleMax);
}

static INLINE int32_t PIC_MakeCycles(float amount)
{
	return static_cast<int32_t>(static_cast<float>(CPU_CycleMax) * amount);
}

static INLINE float PIC_FullIndex()
{
	return static_cast<float>(PIC_Ticks) + PIC_TickIndex();
}

void PIC_ActivateIRQ(uint8_t irq);
void PIC_DeActivateIRQ(uint8_t irq);

void PIC_runIRQs();
bool PIC_RunQueue();

//Delay in milliseconds
void PIC_AddEvent(PIC_EventHandler handler, float delay, uint32_t val = 0);
void PIC_RemoveEvents(PIC_EventHandler handler);
void PIC_RemoveSpecificEvents(PIC_EventHandler handler, uint32_t val);

void PIC_SetIRQMask(uint32_t irq, bool masked);
#endif
