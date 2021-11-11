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
#include <cmath>

#include <chrono>
#include <limits>
#include <cmath>
#include <thread>

/* underlying clock rate in HZ */

constexpr int PIT_TICK_RATE = 1193182;

// Short-hand unit conversions
constexpr auto PIT_TICK_RATE_KHZ = static_cast<double>(PIT_TICK_RATE) / 1000.0;
// The rate at which a repeating event occurs is the frequency, measured in Hertz.

// The inverse of frequency is the time between events, called 'period'.
// In this case, we want the period of every 1000 PIT tick events.
constexpr auto PERIOD_OF_1K_PIT_TICKS = 1000.0 / static_cast<double>(PIT_TICK_RATE);

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

static inline void DelayPrecise(double seconds) {
    static double estimate = 5e-3;
    static double mean = 5e-3;
    static double m2 = 0;
    static int64_t count = 1;

    while (seconds > estimate) {
        const auto start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto end = std::chrono::steady_clock::now();

        const auto observed = (end - start).count() * 1e-9;
        seconds -= observed;

        ++count;
        const auto delta = observed - mean;
        mean += delta / count;
        m2   += delta * (observed - mean);
        const auto stddev = std::sqrt(m2 / (count - 1));
        estimate = mean + stddev;
    }

    // spin lock
    const auto start = std::chrono::steady_clock::now();
    while ((std::chrono::steady_clock::now() - start).count() * 1e-9 < seconds);
}

#endif
