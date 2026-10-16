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
		if (int64_t{index} + write.size_bytes - 1 > write.limit) {
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

TEST(CpuSegmentLimits, FullSizeLimitNotEnforced)
{
	// Like on real CPUs, a word at the last byte of a 4 GB segment wraps
	// around to 0 instead of faulting
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffffffff,
	                                         .size_bytes = 2,
	                                         .count      = 1,
	                                         .di_index   = 0xffffffff,
	                                         .add_mask   = 0xffffffff}),
	          1u);

	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffffffff,
	                                         .size_bytes = 4,
	                                         .count      = 10,
	                                         .di_index   = 0xfffffff2,
	                                         .add_mask   = 0xffffffff}),
	          10u);
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

// Intel SDM Vol. 3A, "Limit Checking": a byte at offset > limit, a word at
// offset > limit - 1 and a dword at offset > limit - 3 raise #GP
TEST(CpuSegmentLimits, WholeOperandMustFitWithinLimit)
{
	constexpr uint32_t limit = 0xffff;
	auto count = [&](const int size_bytes, const uint32_t di_index) {
		return CPU_GetStringCountWithinLimit({.limit      = limit,
		                                      .size_bytes = size_bytes,
		                                      .count      = 1,
		                                      .di_index   = di_index,
		                                      .add_mask = 0xffffffff});
	};
	EXPECT_EQ(count(1, limit), 1u);
	EXPECT_EQ(count(1, limit + 1), 0u);
	EXPECT_EQ(count(2, limit - 1), 1u);
	EXPECT_EQ(count(2, limit), 0u);
	EXPECT_EQ(count(4, limit - 3), 1u);
	EXPECT_EQ(count(4, limit - 2), 0u);
}

TEST(CpuSegmentLimits, ForwardCountIsLimitedByRemainingBytes)
{
	auto count = [](const int size_bytes) {
		return CPU_GetStringCountWithinLimit({.limit      = 0xffff,
		                                      .size_bytes = size_bytes,
		                                      .count      = 1000,
		                                      .di_index   = 0xff00,
		                                      .add_mask = 0xffffffff});
	};
	EXPECT_EQ(count(1), 256u);
	EXPECT_EQ(count(2), 128u);
	EXPECT_EQ(count(4), 64u);
}

// With a 16-bit address size the index wraps to 0 after 0xffff and stays
// within the segment; with a 32-bit address size it runs past the limit
TEST(CpuSegmentLimits, SixteenBitIndexWrapsWithinSegment)
{
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffff,
	                                         .size_bytes = 4,
	                                         .count      = 3,
	                                         .di_index   = 0xfffc,
	                                         .add_mask   = 0xffff}),
	          3u);
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffff,
	                                         .size_bytes = 4,
	                                         .count      = 3,
	                                         .di_index   = 0xfffc,
	                                         .add_mask   = 0xffffffff}),
	          1u);
}

TEST(CpuSegmentLimits, BackwardStopsWhenIndexWrapsBelowZero)
{
	// 32-bit address size: the write after offset 0 lands at 0xfffffffc
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffff,
	                                         .direction  = -1,
	                                         .size_bytes = 4,
	                                         .count      = 4,
	                                         .di_index   = 8,
	                                         .add_mask   = 0xffffffff}),
	          3u);
	// 16-bit address size: it lands at 0xfffc, which a 64 KB segment covers
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0xffff,
	                                         .direction  = -1,
	                                         .size_bytes = 4,
	                                         .count      = 4,
	                                         .di_index   = 8,
	                                         .add_mask   = 0xffff}),
	          4u);
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0x7fff,
	                                         .direction  = -1,
	                                         .size_bytes = 4,
	                                         .count      = 4,
	                                         .di_index   = 8,
	                                         .add_mask   = 0xffff}),
	          3u);
}

TEST(CpuSegmentLimits, FullCountDoesNotOverflow)
{
	EXPECT_EQ(CPU_GetStringCountWithinLimit({.limit      = 0x7fffffff,
	                                         .size_bytes = 4,
	                                         .count      = 0xffffffff,
	                                         .di_index   = 0,
	                                         .add_mask   = 0xffffffff}),
	          0x20000000u);
}

} // namespace
