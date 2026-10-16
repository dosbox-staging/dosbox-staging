// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/cpu.h"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>

namespace {

// Straightforward reference: walk the writes one by one
uint32_t count_within_limit_reference(const StringWrite& write)
{
	if (write.limit == NoSegmentLimit) {
		return write.count;
	}
	const auto add_index = static_cast<uint32_t>(write.direction *
	                                             write.size_bytes);

	auto index = write.di_index;
	for (uint32_t i = 0; i < write.count; ++i) {
		if (int64_t{index} + write.size_bytes - 1 >
		    static_cast<int64_t>(write.limit)) {
			return i;
		}
		index = (index + add_index) & write.add_mask;
	}
	return write.count;
}

TEST(CpuSegmentLimits, NoLimit)
{
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = NoSegmentLimit,
	                                         .size_bytes = 4,
	                                         .count      = 100000,
	                                         .di_index   = 0xfffffff0,
	                                         .add_mask   = 0xffffffff}),
	          100000u);
}

TEST(CpuSegmentLimits, ZeroCount)
{
	EXPECT_EQ(CPU_GetStringCountWithinLimit(
	                  {.limit = 0xffff, .count = 0, .di_index = 0x10000}),
	          0u);
}

TEST(CpuSegmentLimits, ViperBankSwitch)
{
	// REP MOVSD starting past the end of a 64 KB segment faults at once
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffff,
	                                         .size_bytes = 4,
	                                         .count      = 200,
	                                         .di_index   = 0x1007c,
	                                         .add_mask   = 0xffffffff}),
	          0u);

	// REP MOVSD crossing the end executes the dwords that fit
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffff,
	                                         .size_bytes = 4,
	                                         .count      = 200,
	                                         .di_index   = 0xff00,
	                                         .add_mask   = 0xffffffff}),
	          64u);
}

TEST(CpuSegmentLimits, PartialWriteFaults)
{
	// A dword straddling the limit doesn't fit
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffff,
	                                         .size_bytes = 4,
	                                         .count      = 1,
	                                         .di_index   = 0xfffe,
	                                         .add_mask   = 0xffffffff}),
	          0u);
}

TEST(CpuSegmentLimits, FullSizeLimitEnforced)
{
	// STOSW at the last byte of a 4 GB segment straddles the limit
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffffffff,
	                                         .size_bytes = 2,
	                                         .count      = 1,
	                                         .di_index   = 0xffffffff,
	                                         .add_mask   = 0xffffffff}),
	          0u);

	// The last dword of a 4 GB segment fits
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffffffff,
	                                         .size_bytes = 4,
	                                         .count      = 1,
	                                         .di_index   = 0xfffffffc,
	                                         .add_mask   = 0xffffffff}),
	          1u);

	// REP STOSD stops at the dword straddling the end of a 4 GB segment
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffffffff,
	                                         .size_bytes = 4,
	                                         .count      = 10,
	                                         .di_index   = 0xfffffff2,
	                                         .add_mask   = 0xffffffff}),
	          3u);
}

void expect_matches_reference(const StringWrite& write)
{
	EXPECT_EQ(CPU_GetStringCountWithinLimit(write),
	          count_within_limit_reference(write))
	        << "limit=" << write.limit << " direction=" << write.direction
	        << " size=" << write.size_bytes << " count=" << write.count
	        << " index=" << write.di_index << " mask=" << write.add_mask;
}

// Every combination of count, address size and start index around the limit
// and the wraparound points
void expect_all_writes_match_reference(const uint32_t limit,
                                       const int direction, const int size_bytes)
{
	constexpr auto counts = std::to_array<uint32_t>(
	        {1, 2, 3, 5, 0x10, 0x4000, 0x10001});
	constexpr auto indices = std::to_array<uint32_t>(
	        {0, 1, 2, 0x7ffc, 0xfff0, 0xfffc, 0xffff, 0x10000, 0xfffffff8, 0xffffffff});

	for (const auto count : counts) {
		for (const uint32_t add_mask : {0xffffu, 0xffffffffu}) {
			for (const auto index : indices) {
				expect_matches_reference({.limit = limit,
				                          .direction = direction,
				                          .size_bytes = size_bytes,
				                          .count = count,
				                          .di_index = index & add_mask,
				                          .add_mask = add_mask});
			}
		}
	}
}

TEST(CpuSegmentLimits, MatchesReference)
{
	constexpr auto limits = std::to_array<uint32_t>(
	        {0, 1, 0x7fff, 0xfffd, 0xffff, 0x10003, 0xfffffffe, 0xffffffff});

	for (const auto limit : limits) {
		for (const auto direction : {1, -1}) {
			for (const auto size_bytes : {1, 2, 4}) {
				expect_all_writes_match_reference(limit,
				                                  direction,
				                                  size_bytes);
			}
		}
	}
}

} // namespace
