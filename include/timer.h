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
#include <cstdlib>

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

typedef void (*TIMER_TickHandler)(void);

/* Register a function that gets called every time if 1 or more ticks pass */
void TIMER_AddTickHandler(TIMER_TickHandler handler);
void TIMER_DelTickHandler(TIMER_TickHandler handler);

/* This will add 1 milliscond to all timers */
void TIMER_AddTick(void);

static inline int64_t GetTicks()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
	               std::chrono::steady_clock::now().time_since_epoch())
	        .count();
}

static inline int64_t GetTicksUs()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
	               std::chrono::steady_clock::now().time_since_epoch())
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

// The duration to use for precise sleep
static constexpr int precise_delay_duration_us = 100;

// based on work from:
// https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
static inline void DelayPrecise(const int milliseconds)
{
	// The estimate of how long the sleep should take (microseconds)
	static auto estimate = 5e-5;
	// Use the estimate value as the default mean time taken
	static auto mean = 5e-5;
	static auto m2 = 0.0;
	static int64_t count = 1;

	// Original code operated on seconds, convert
	auto seconds = static_cast<double>(milliseconds) / 1e3;

	// sleep as long as we can, then spinlock the rest
	while (seconds > estimate) {
		const auto start = GetTicksUs();
		DelayUs(precise_delay_duration_us);
		// Original code operated on seconds, convert
		const auto observed = static_cast<double>(GetTicksUsSince(start)) / 1e6;
		seconds -= observed;

		++count;
		const auto delta = observed - mean;
		mean += delta / static_cast<double>(count);
		m2 += delta * (observed - mean);
		const auto stddev = std::sqrt(m2 / static_cast<double>(count - 1));
		estimate = mean + stddev;
	}

	// spin lock
	const auto spin_start = GetTicksUs();
	const auto spin_remain = lround(seconds * 1e6);
	do {
		std::this_thread::yield();
	} while (GetTicksUsSince(spin_start) <= spin_remain);
}

static inline bool CanDelayPrecise()
{
	// The tolerance to allow for sleep variation
	constexpr int precise_delay_tolerance_us = precise_delay_duration_us;

	bool is_precise = true;

	for (int i=0;i<10;i++) {
		const auto start = GetTicksUs();
		DelayUs(precise_delay_duration_us);
		const auto elapsed = GetTicksUsSince(start);
		if (std::abs(elapsed - precise_delay_duration_us) >
		    precise_delay_tolerance_us)
			is_precise = false;
	}

	return is_precise;
}

#endif
