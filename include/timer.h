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

#ifndef DOSBOX_TIMER_H
#define DOSBOX_TIMER_H

#include "bit_view.h"

#include <cassert>
#include <chrono>
#include <limits>
#include <thread>

/* underlying clock rate in HZ */

constexpr int PIT_TICK_RATE = 1193182;

// Short-hand unit conversions
constexpr auto PIT_TICK_RATE_KHZ = static_cast<double>(PIT_TICK_RATE) / 1000.0;
// The rate at which a repeating event occurs is the frequency, measured in Hertz.

// The inverse of frequency is the time between events, called 'period'.
// In this case, we want the period of every 1000 PIT tick events.
constexpr auto PERIOD_OF_1K_PIT_TICKS = 1000.0 / static_cast<double>(PIT_TICK_RATE);

/* PIT operating modes represented in 3 bits:
1 to 3       Operating mode :
                0 0 0 = Mode 0 (interrupt on terminal count)
                0 0 1 = Mode 1 (hardware re-triggerable one-shot)
                0 1 0 = Mode 2 (rate generator)
                0 1 1 = Mode 3 (square wave generator)
                1 0 0 = Mode 4 (software triggered strobe)
                1 0 1 = Mode 5 (hardware triggered strobe)
                1 1 0 = Mode 2 (rate generator, same as 010b)
                1 1 1 = Mode 3 (square wave generator, same as 011b)
Refs: http://www.osdever.net/bkerndev/Docs/pit.htm
      https://wiki.osdev.org/Programmable_Interval_Timer#Operating_Modes
*/
enum class PitMode : uint8_t {
	InterruptOnTC      = 0b0'0'0,
	OneShot            = 0b0'0'1,
	RateGenerator      = 0b0'1'0,
	SquareWave         = 0b0'1'1,
	SoftwareStrobe     = 0b1'0'0,
	HardwareStrobe     = 0b1'0'1,
	RateGeneratorAlias = 0b1'1'0,
	SquareWaveAlias    = 0b1'1'1,
	Inactive,
};

/*  PPI Port B Control Register
    Bit System   Description
    ~~~ ~~~~~~   ~~~~~~~~~~~
    0   XT & PC  Timer 2 gate to speaker output (read+write)
    1   XT & PC  Speaker data state (read+write)
    4   XT & PC  Toggles with each read
    5   XT-only  Toggles with each read
        PC-only  Mirrors timer 2 gate to speaker output
    7   XT-only  Clear keyboard buffer
*/

union PpiPortB {
	uint8_t data = {0};
	bit_view<0, 1> timerGateOutput;
	bit_view<1, 1> speakerOutput;
	bit_view<4, 1> toggleOnRead;
	bit_view<5, 1> toggleOnReadXt;
	bit_view<5, 1> timerGateOutputAlias;
	bit_view<7, 1> clearKeyboardXt;
	// virtual view of the timer and speaker fields
	bit_view<0, 2> timerGateAndSpeakerOutput;
};

const char *PitModeToString(const PitMode mode);

/* PC Speakers functions, tightly related to the timer functions */
void PCSPEAKER_SetCounter(int cntr, PitMode pit_mode);
void PCSPEAKER_SetType(const PpiPortB &port_b_state);
void PCSPEAKER_SetPITControl(PitMode pit_mode);

typedef void (*TIMER_TickHandler)(void);

/* Register a function that gets called every time if 1 or more ticks pass */
void TIMER_AddTickHandler(TIMER_TickHandler handler);
void TIMER_DelTickHandler(TIMER_TickHandler handler);

/* This will add 1 milliscond to all timers */
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

static inline int GetTicksDiff(const int64_t new_ticks, const int64_t old_ticks)
{
	assert(new_ticks >= old_ticks);
	assert((new_ticks - old_ticks) <= std::numeric_limits<int>::max());
	return static_cast<int>(new_ticks - old_ticks);
}

static inline int GetTicksSince(const int64_t old_ticks)
{
	const auto now = GetTicks();
	assert((now - old_ticks) <= std::numeric_limits<int>::max());
	return GetTicksDiff(now, old_ticks);
}

static inline int GetTicksUsSince(const int64_t old_ticks)
{
	const auto now = GetTicksUs();
	assert((now - old_ticks) <= std::numeric_limits<int>::max());
	return GetTicksDiff(now, old_ticks);
}

static inline void Delay(const int milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

static inline void DelayUs(const int microseconds)
{
	std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

#endif
