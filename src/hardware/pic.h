// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PIC_H
#define DOSBOX_PIC_H

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "cpu.h"

typedef void(PIC_EOIHandler)();
typedef void (*PIC_EventHandler)(uint32_t val);

extern uint32_t PIC_IRQCheck;

// Elapsed milliseconds since starting DOSBox
// Holds ~4.2 B milliseconds or ~48 days before rolling over
extern uint32_t PIC_Ticks;

extern std::atomic<double> atomic_pic_index;

// The number of cycles not done yet (ND)
static inline int32_t PIC_TickIndexND()
{
	return CPU_CycleMax - CPU_CycleLeft - CPU_Cycles;
}

// Returns the percent cycles completed within the current "millisecond tick" of
// the CPU
static inline double PIC_TickIndex()
{
	return static_cast<double>(PIC_TickIndexND()) / static_cast<double>(CPU_CycleMax);
}

static inline int32_t PIC_MakeCycles(double amount)
{
	const auto cycles = static_cast<double>(CPU_CycleMax) * amount;
	assert(cycles >= INT32_MIN && cycles <= INT32_MAX);
	return static_cast<int32_t>(cycles);
}

static inline double PIC_FullIndex()
{
	return static_cast<double>(PIC_Ticks) + PIC_TickIndex();
}

// Thread safe version of PIC_FullIndex()
// Callers on the main thread should prefer PIC_FullIndex() as it is more precise.
// I attempted to change this everywhere and had regressions from VGA code for example.
// It should be good enough for audio though.
static inline double PIC_AtomicIndex()
{
	return atomic_pic_index.load(std::memory_order_acquire);
}

static inline void PIC_UpdateAtomicIndex()
{
	atomic_pic_index.store(PIC_FullIndex(), std::memory_order_release);
}

void PIC_ActivateIRQ(uint8_t irq);
void PIC_DeActivateIRQ(uint8_t irq);

void PIC_runIRQs();
bool PIC_RunQueue();

//Delay in milliseconds
void PIC_AddEvent(PIC_EventHandler handler, double delay, uint32_t val = 0);
void PIC_RemoveEvents(PIC_EventHandler handler);
void PIC_RemoveSpecificEvents(PIC_EventHandler handler, uint32_t val);

void PIC_SetIRQMask(uint32_t irq, bool masked);
#endif
