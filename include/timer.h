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

#include <cassert>
#include <cstdlib>

#include <chrono>
#include <limits>
#include <thread>

/* underlying clock rate in HZ */

#define PIT_TICK_RATE 1193182

typedef void (*TIMER_TickHandler)(void);

/* Register a function that gets called every time if 1 or more ticks pass */
void TIMER_AddTickHandler(TIMER_TickHandler handler);
void TIMER_DelTickHandler(TIMER_TickHandler handler);

/* This will add 1 milliscond to all timers */
void TIMER_AddTick(void);

static inline int64_t GetTicks(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
	               std::chrono::steady_clock::now().time_since_epoch())
	        .count();
}

static inline int64_t GetTicksUs(void)
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
	               std::chrono::steady_clock::now().time_since_epoch())
	        .count();
}

static inline int GetTicksDiff(int64_t new_ticks, int64_t old_ticks)
{
	assert(new_ticks >= old_ticks);
	assert((new_ticks - old_ticks) <= std::numeric_limits<int>::max());
	return static_cast<int>(new_ticks - old_ticks);
}

static inline int GetTicksSince(int64_t old_ticks)
{
	const auto now = GetTicks();
	assert((now - old_ticks) <= std::numeric_limits<int>::max());
	return GetTicksDiff(now, old_ticks);
}

static inline int GetTicksUsSince(int64_t old_ticks)
{
	const auto now = GetTicksUs();
	assert((now - old_ticks) <= std::numeric_limits<int>::max());
	return GetTicksDiff(now, old_ticks);
}

static constexpr int precise_delay_interval_us = 100;
static constexpr int precise_delay_tolerance_us = precise_delay_interval_us; 
static constexpr double precise_delay_default_estimate = 5e-5;
static constexpr double precise_delay_default_mean = 5e-5;

static inline void Delay(int milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

static inline void DelayUs(int microseconds)
{
	std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

static inline void DelayPrecise(int milliseconds)
{
    static double estimate = precise_delay_default_estimate;
    static double mean = precise_delay_default_mean;
    static double m2 = 0;
    static int64_t count = 1;

	double seconds = milliseconds / 1e3;

    while (seconds > estimate) {
        const auto start = GetTicksUs();
        DelayUs(precise_delay_interval_us);
        const double observed = GetTicksUsSince(start) / 1e6;
        seconds -= observed;

        ++count;
        const double delta = observed - mean;
        mean += delta / count;
        m2   += delta * (observed - mean);
        const double stddev = sqrt(m2 / (count - 1));
        estimate = mean + stddev;
    }

    // spin lock
    const auto spin_start = GetTicksUs();
	const int spin_remain = static_cast<int>(seconds * 1e6);
    while (GetTicksUsSince(spin_start) <= spin_remain);
}

static inline bool CanDelayPrecise(void)
{
	bool is_precise = true;

	for (int i=0;i<10;i++) {
		const auto start = GetTicksUs();
		DelayUs(precise_delay_interval_us);
		const auto elapsed = GetTicksUsSince(start);
		if (std::abs(elapsed - precise_delay_interval_us) > precise_delay_tolerance_us) is_precise = false;
	}

	return is_precise;
}
#endif
