// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TIMER_H
#define DOSBOX_TIMER_H

#include "bit_view.h"

#include <cassert>
#include <chrono>
#include <limits>
#include <thread>

// underlying clock rate in HZ
constexpr int PIT_TICK_RATE = 1193182;

// Short-hand unit conversions
constexpr auto PIT_TICK_RATE_KHZ = static_cast<double>(PIT_TICK_RATE) / 1000.0;
// The rate at which a repeating event occurs is the frequency, measured in Hertz.

// The inverse of frequency is the time between events, called 'period'.
// In this case, we want the period of every 1000 PIT tick events.
constexpr auto period_of_1k_pit_ticks = 1000.0 / static_cast<double>(PIT_TICK_RATE);
constexpr auto period_of_1k_pit_ticks_f = static_cast<float>(period_of_1k_pit_ticks);

// PIT operating modes represented in 3 bits:
// 1 to 3       Operating mode :
//                 0 0 0 = Mode 0 (interrupt on terminal count)
//                 0 0 1 = Mode 1 (hardware re-triggerable one-shot)
//                 0 1 0 = Mode 2 (rate generator)
//                 0 1 1 = Mode 3 (square wave generator)
//                 1 0 0 = Mode 4 (software triggered strobe)
//                 1 0 1 = Mode 5 (hardware triggered strobe)
//                 1 1 0 = Mode 2 (rate generator, same as 010b)
//                 1 1 1 = Mode 3 (square wave generator, same as 011b)
// Refs: http://www.osdever.net/bkerndev/Docs/pit.htm
//       https://wiki.osdev.org/Programmable_Interval_Timer#Operating_Modes
enum class PitMode : uint8_t {
	InterruptOnTerminalCount = 0b0'0'0,
	OneShot                  = 0b0'0'1,
	RateGenerator            = 0b0'1'0,
	SquareWave               = 0b0'1'1,
	SoftwareStrobe           = 0b1'0'0,
	HardwareStrobe           = 0b1'0'1,
	RateGeneratorAlias       = 0b1'1'0,
	SquareWaveAlias          = 0b1'1'1,
	Inactive,
};

//  PPI Port B Control Register
//  Bit System   Description
//  ~~~ ~~~~~~   ~~~~~~~~~~~
//  0   XT & PC  Timer 2 gate to speaker output (read+write)
//  1   XT & PC  Speaker data state (read+write)
//  4   XT & PC  Toggles with each read
//  5   XT-only  Toggles with each read
//      PC-only  Mirrors timer 2 gate to speaker output
//  7   XT-only  Clear keyboard buffer
//
//
union PpiPortB {
	uint8_t data = {0};
	bit_view<0, 1> timer2_gating;
	bit_view<1, 1> speaker_output;
	bit_view<4, 1> read_toggle;
	bit_view<5, 1> xt_read_toggle;
	bit_view<5, 1> timer2_gating_alias;
	bit_view<7, 1> xt_clear_keyboard;

	// virtual view of the timer and speaker output fields
	bit_view<0, 2> timer2_gating_and_speaker_out;
};

const char* pit_mode_to_string(const PitMode mode);

// PC speaker functions, tightly related to the timer functions
void PCSPEAKER_SetCounter(const int count, const PitMode pit_mode);
void PCSPEAKER_SetType(const PpiPortB& port_b);
void PCSPEAKER_SetPITControl(const PitMode pit_mode);

typedef void (*TIMER_TickHandler)(void);

// Register a function that gets called every time if 1 or more ticks pass
void TIMER_AddTickHandler(TIMER_TickHandler handler);
void TIMER_DelTickHandler(TIMER_TickHandler handler);

// This will add 1 milliscond to all timers
void TIMER_AddTick(void);

extern const std::chrono::steady_clock::time_point system_start_time;

static inline int64_t GetTicks()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
	               std::chrono::steady_clock::now() - system_start_time)
	        .count();
}

static inline int64_t GetTicksUs()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
	               std::chrono::steady_clock::now() - system_start_time)
	        .count();
}

static inline int64_t GetTicksDiff(const int64_t new_ticks, const int64_t old_ticks)
{
	assert(new_ticks >= old_ticks);
	return new_ticks - old_ticks;
}

static inline int64_t GetTicksSince(const int64_t old_ticks)
{
	const auto now = GetTicks();
	return GetTicksDiff(now, old_ticks);
}

static inline int64_t GetTicksUsSince(const int64_t old_ticks)
{
	const auto now = GetTicksUs();
	return GetTicksDiff(now, old_ticks);
}

static inline void Delay(const int64_t milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

static inline void DelayUs(const int64_t microseconds)
{
	std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

#endif
