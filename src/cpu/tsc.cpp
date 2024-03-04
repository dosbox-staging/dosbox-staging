/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include "dosbox.h"

#include "checks.h"
#include "cpu.h"
#include "pic.h"
#include "regs.h"

#include <algorithm>
#include <map>
#include <vector>

CHECK_NARROWING();

// ***************************************************************************
// Hardcoded values
// ***************************************************************************

// List of typical CPU clock values, in MHz - table has to be sorted!

static const std::vector<double> typical_mhz_values = {
        // clang-format off
	0.10, 0.25, 0.50, 1.00, 2.00, 3.00, 4.00, 4.77, 6.00, 8.00,

	10.0, 12.5, 16.0, 20.0, 25.0, 33.3, 40.0, 60.0, 66.6, 75.0, 80.0, 90.0,

	100.0, 120.0, 133.3, 150.0, 166.6, 180.0, 200.0, 233.3, 266.6, 300.0,
	333.3, 350.0, 366.6, 400.0, 433.3, 450.0, 466.6, 475.0, 500.0, 533.3,
	550.0, 600.0, 650.0, 666.6, 700.0, 733.3, 750.0, 800.0, 850.0, 866.6,
	900.0, 933.3,

	1000.0, 1100.0, 1133.3, 1200.0, 1266.6, 1300.0, 1333.3, 1400.0, 1450.0,
	1466.6, 1500.0, 1583.3, 1600.0, 1666.6, 1700.0, 1750.0, 1800.0, 1833.3,
	1866.6, 1900.0, 2000.0, 2133.3, 2200.0, 2266.6, 2300.0, 2333.3, 2400.0,
	2500.0, 2533.3, 2600.0, 2666.6, 2700.0, 2800.0, 2833.3, 2900.0, 2933.3,
	3000.0, 3066.6, 3100.0, 3166.6, 3200.0, 3300.0, 3333.3, 3400.0, 3466.6,
	3500.0, 3600.0, 3700.0, 3733.3, 3800.0
        // clang-format on
};

// Maps of emulated CPU cycles to CPU frequency in MHz; if not stated otherwise,
// values were taken from DOSBox-X wiki:
// https://dosbox-x.com/wiki/Guide%3ACPU-settings-in-DOSBox%E2%80%90X#_cycles

using CyclesMhzMap = std::map<int32_t, double>;

static const CyclesMhzMap Intel86_Map = {{240, 4.77}};

static const CyclesMhzMap Intel286_Map = {{750, 8}, {1510, 12}, {3300, 25}};

static const CyclesMhzMap Intel386_Map = {{4595, 25}, {6075, 33}};

static const CyclesMhzMap Intel486_Map = {{12019, 33},
                                          {23880, 66},
                                          {33445, 100},
                                          {47810, 133}};

static const CyclesMhzMap IntelPentium_Map = {{31545, 60},
                                              {35620, 66},
                                              {43500, 75},
                                              {52000, 90},
                                              {60000, 100},
                                              {74000, 120},
                                              {80000, 133}};

static const CyclesMhzMap IntelPentiumMmx_Map = {{97240, 166}};

// DOSBox-X Wiki contains also the following estimations:
// - Intel Pentium II  - 200000 = 300 Mhz
// - Intel Pentium III - 407000 = 866 Mhz
// - AMD K6            - 110000 = 166 Mhz
// - AMD K6            - 130000 = 200 Mhz
// - AMD K6-2          - 193000 = 300 Mhz
// - AMD Athlon        - 306000 = 600 Mhz

// ***************************************************************************
// Implementation
// ***************************************************************************

// Return a typical MHz of a fast CPU of the currently emulated type
static double get_typical_cpu_mhz()
{
	if (CPU_ArchitectureType < ArchitectureType::Intel186) {
		return 10.0; // 8086 was max 10 MHz
	} else if (CPU_ArchitectureType < ArchitectureType::Intel386Slow) {
		return 25.0; // 186 and 286 were max 25 MHz
	} else if (CPU_ArchitectureType < ArchitectureType::Intel486OldSlow) {
		return 40.0; // 386 was max 40 MHz
	} else if (CPU_ArchitectureType < ArchitectureType::Pentium ||
	           CPU_ArchitectureType == ArchitectureType::Mixed) {
		return 133.0; // 486 was max 133 MHz
	} else if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) {
		return 166.0; // Pentium was max 200 Mhz, but it was
		              // a very rare CPU, 166 Mhz was common
	} else {
		return 233.0; // Pentium MMX was max 233 MHz
	}
}

// Round the value in MHz to one of the values typical for a real CPU
static double round_cpu_mhz(const double cpu_mhz)
{
	const auto iter_upper = std::upper_bound(typical_mhz_values.begin(),
	                                         typical_mhz_values.end(),
	                                         cpu_mhz);

	if (iter_upper == typical_mhz_values.end()) {
		constexpr double resolution = 100.0;
		return std::round(cpu_mhz / resolution) * resolution;
	}

	if (iter_upper == typical_mhz_values.begin()) {
		return typical_mhz_values[0];
	}

	const auto iter_lower = std::prev(iter_upper);

	const auto distance_lower = cpu_mhz - *iter_lower;
	const auto distance_upper = *iter_upper - cpu_mhz;

	assert(distance_lower >= 0.0);
	assert(distance_upper >= 0.0);

	if (distance_lower < distance_upper) {
		return *iter_lower;
	} else {
		return *iter_upper;
	}
}

// Estimate the CPU speed in MHz given the amount of cycles emulated
static double get_estimated_cpu_mhz(const int32_t cycles)
{
	static ArchitectureType cached_arch = {};
	static int32_t cached_cycles        = {};
	static double cached_result         = {};

	static bool is_cache_valid = false;

	double coeff = {};

	// Return cached calculation result, if it's still valid
	if (is_cache_valid && cached_arch == CPU_ArchitectureType &&
	    cached_cycles == cycles) {
		return cached_result;
	}

	// Select CPU performance (cycles to MHz) map
	auto getCyclesMhzMap = []() -> const CyclesMhzMap& {
		if (CPU_ArchitectureType < ArchitectureType::Intel286) {
			return Intel86_Map;
		} else if (CPU_ArchitectureType < ArchitectureType::Intel386Slow) {
			return Intel286_Map;
		} else if (CPU_ArchitectureType < ArchitectureType::Intel486OldSlow) {
			return Intel386_Map;
		} else if (CPU_ArchitectureType < ArchitectureType::Pentium ||
		           CPU_ArchitectureType == ArchitectureType::Mixed) {
			return Intel486_Map;
		} else if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) {
			return IntelPentium_Map;
		} else {
			return IntelPentiumMmx_Map;
		}
	};

	const auto& cycles_mhz_map = getCyclesMhzMap();
	assert(!cycles_mhz_map.empty());

	// Calculate the coefficient to be used to convert cycles to MHz
	const auto iter_upper = cycles_mhz_map.upper_bound(cycles);
	if (iter_upper == cycles_mhz_map.end()) {
		const auto iter_lower = std::prev(cycles_mhz_map.end());
		coeff = iter_lower->second / iter_lower->first;
	} else if (iter_upper == cycles_mhz_map.begin()) {
		coeff = iter_upper->second / iter_upper->first;
	} else {
		const auto iter_lower = std::prev(iter_upper);

		const auto distance_lower = cycles - iter_lower->first;
		const auto distance_upper = iter_upper->first - cycles;

		assert(distance_lower >= 0);
		assert(distance_upper >= 0);

		const auto range = distance_upper + distance_lower;

		assert(range != 0);

		const double coeff_lower = iter_lower->second / iter_lower->first;
		const double coeff_upper = iter_lower->second / iter_upper->first;

		coeff = (coeff_lower * distance_upper + coeff_upper * distance_lower) / range;
	}

	// Update the cache, return the result
	cached_arch    = CPU_ArchitectureType;
	cached_cycles  = cycles;
	cached_result  = round_cpu_mhz(cycles * coeff);
	is_cache_valid = true;

	return cached_result;
}

static double get_estimated_cpu_mhz()
{
	if (CPU_CycleAutoAdjust) {
		if (CPU_CycleLimit > 0) {
			return get_estimated_cpu_mhz(CPU_CycleLimit);
		} else {
			// No desired number of cycles specified
			return get_typical_cpu_mhz();
		}
	} else {
		return get_estimated_cpu_mhz(CPU_CycleMax);
	}
}

void CPU_ReadTSC()
{
	const auto cpu_mhz = get_estimated_cpu_mhz();

	const auto tsc_precise = PIC_FullIndex() * cpu_mhz * 1000;
	const auto tsc_rounded = static_cast<int64_t>(std::round(tsc_precise));

	reg_edx = static_cast<uint32_t>(tsc_rounded >> 32);
	reg_eax = static_cast<uint32_t>(tsc_rounded & 0xffffffff);
}
