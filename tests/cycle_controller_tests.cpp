// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/cycle_controller.h"

#include <gtest/gtest.h>

namespace {

constexpr int MaxCycles     = 2'000'000;
constexpr int InitialCycles = 60'000;

CycleObservation window(const int64_t work_us    = 90'000,
                        const int64_t guest_us   = 100'000,
                        const int64_t elapsed_us = 100'000)
{
	return {.elapsed_us    = elapsed_us,
	        .work_us       = work_us,
	        .guest_us      = guest_us,
	        .budget_cycles = InitialCycles * (guest_us / 1000)};
}

auto observe(CycleController& controller, const CycleObservation& sample,
             const int cycles = InitialCycles, const int percent = 100,
             const int limit = -1)
{
	return controller.Observe(sample, cycles, percent, limit, MaxCycles);
}

// A deterministic host: one guest millisecond costs overhead_us +
// cycles / cycles_per_us. The pacer sleeps if it completes early.
int run_host(CycleController& controller, int cycles, const double cycles_per_us,
             const int ticks = 10'000, const int64_t overhead_us = 0)
{
	for (auto i = 0; i < ticks; ++i) {
		const auto work_us = overhead_us +
		                     static_cast<int64_t>(
		                             std::lround(cycles / cycles_per_us));
		const CycleObservation sample = {
		        .elapsed_us    = std::max(int64_t{1000}, work_us),
		        .work_us       = work_us,
		        .guest_us      = 1000,
		        .budget_cycles = cycles,
		};
		if (const auto result = observe(controller, sample, cycles)) {
			cycles = result->cycles;
		}
	}
	return cycles;
}

} // namespace

TEST(CycleController, KeepsStableBudgetInsideDeadBand)
{
	CycleController controller = {};
	for (const auto work_us : {89'000, 90'000, 91'000}) {
		const auto result = observe(controller, window(work_us));
		ASSERT_TRUE(result);
		EXPECT_EQ(result->cycles, InitialCycles);
	}
}

TEST(CycleController, AccumulatesSubMillisecondMeasurements)
{
	CycleController controller    = {};
	const CycleObservation sample = {.elapsed_us    = 1000,
	                                 .work_us       = 501,
	                                 .guest_us      = 1000,
	                                 .budget_cycles = InitialCycles};
	for (auto i = 0; i < 99; ++i) {
		EXPECT_FALSE(observe(controller, sample));
	}
	const auto result = observe(controller, sample);
	ASSERT_TRUE(result);
	EXPECT_EQ(result->observation.work_us, 50'100);
	EXPECT_DOUBLE_EQ(result->load, 0.501);
	EXPECT_EQ(result->cycles, 107'784);
}

TEST(CycleController, IsolatedHostStallDoesNotReduceCycles)
{
	CycleController controller = {};
	const auto stall           = window(200'000, 100'000, 200'000);
	EXPECT_EQ(observe(controller, stall)->cycles, InitialCycles);
	EXPECT_EQ(observe(controller, window())->cycles, InitialCycles);
	EXPECT_EQ(observe(controller, stall)->cycles, InitialCycles);
}

TEST(CycleController, SustainedOverloadReducesCycles)
{
	CycleController controller = {};
	const auto overloaded      = window(100'000, 50'000);
	EXPECT_EQ(observe(controller, overloaded)->cycles, InitialCycles);
	EXPECT_EQ(observe(controller, overloaded)->cycles, InitialCycles / 2);
}

TEST(CycleController, ExtremeSustainedOverloadIsNotIgnored)
{
	CycleController controller = {};
	const auto overloaded      = window(1'000'000, 1000, 1'000'000);
	EXPECT_EQ(observe(controller, overloaded)->cycles, InitialCycles);
	EXPECT_EQ(observe(controller, overloaded)->cycles, InitialCycles / 2);
}

TEST(CycleController, SlowGuestProgressGuardsExcludedPresentationWaits)
{
	CycleController controller = {};
	// Little measured work, but only half a second of guest time per
	// wall-clock second: excluded waits still consume real deadlines.
	const auto blocked = window(10'000, 50'000);
	EXPECT_EQ(observe(controller, blocked)->cycles, InitialCycles);
	EXPECT_EQ(observe(controller, blocked)->cycles, InitialCycles / 2);
}

TEST(CycleController, PacingWaitDoesNotMasqueradeAsCpuWork)
{
	CycleController controller = {};
	const auto result          = observe(controller, window(40'000));
	ASSERT_TRUE(result);
	EXPECT_DOUBLE_EQ(result->guest_rate, 1.0);
	EXPECT_DOUBLE_EQ(result->load, 0.4);
	EXPECT_GT(result->cycles, InitialCycles);
}

TEST(CycleController, IdleAndIoDelayBudgetsDoNotDriveIncreases)
{
	for (const auto removed : {2'000'000, 6'000'000, 9'000'000}) {
		CycleController controller = {};
		auto sample                = window(1000);
		sample.removed_cycles      = removed;
		for (auto i = 0; i < 10; ++i) {
			EXPECT_EQ(observe(controller, sample)->cycles, InitialCycles);
		}
	}
}

TEST(CycleController, IdleGuestCanStillBeThrottledWhenOverloaded)
{
	CycleController controller = {};
	auto sample                = window(150'000, 100'000, 150'000);
	sample.removed_cycles      = sample.budget_cycles;
	observe(controller, sample);
	EXPECT_LT(observe(controller, sample)->cycles, InitialCycles);
}

TEST(CycleController, DroppedCatchupDebtPreventsSpeculativeIncrease)
{
	CycleController controller = {};
	auto sample                = window(40'000);
	sample.dropped_us          = 10'000;
	EXPECT_EQ(observe(controller, sample)->cycles, InitialCycles);
}

TEST(CycleController, HonorsExplicitAndHardLimits)
{
	CycleController controller = {};
	EXPECT_EQ(
	        observe(controller, window(10'000), InitialCycles, 100, 61'000)->cycles,
	        61'000);
	EXPECT_EQ(observe(controller, window(10'000), MaxCycles)->cycles, MaxCycles);
	EXPECT_EQ(observe(controller, window(10'000), InitialCycles, 100, 1)->cycles,
	          1);
}

TEST(CycleController, HonorsLowerRequestedUsage)
{
	CycleController controller = {};
	const auto sample          = window(70'000);
	EXPECT_EQ(observe(controller, sample, InitialCycles, 50)->cycles,
	          InitialCycles);
	EXPECT_LT(observe(controller, sample, InitialCycles, 50)->cycles,
	          InitialCycles);
}

TEST(CycleController, DoesNotInventCapacityWithoutCompletedGuestTime)
{
	CycleController controller = {};
	EXPECT_FALSE(observe(controller, window(100'000, 0)));
}

TEST(CycleController, ResetDiscardsPartialWindowAndOverloadHistory)
{
	CycleController controller = {};
	const auto stall           = window(200'000, 100'000, 200'000);
	observe(controller, stall);
	controller.Reset();
	EXPECT_EQ(observe(controller, stall)->cycles, InitialCycles);
	controller.Reset();
	EXPECT_FALSE(observe(controller, window(50'000, 50'000, 50'000)));
	controller.Reset();
	EXPECT_EQ(observe(controller, window())->cycles, InitialCycles);
}

TEST(CycleController, ConvergesOnSustainableCapacity)
{
	CycleController controller = {};
	// 100 cycles/us gives capacity 100,000 cycles per guest millisecond.
	const auto cycles = run_host(controller, 3000, 100.0);
	EXPECT_NEAR(cycles, 90'000, 2000);
	EXPECT_EQ(run_host(controller, cycles, 100.0), cycles);
}

TEST(CycleController, DiscoversCapacityWithinOneSecond)
{
	CycleController controller = {};
	// A low starting cycle budget should not spend most of a short
	// workload ramping up while the host is mostly unused.
	const auto cycles = run_host(controller, 3000, 100.0, 1000);
	EXPECT_NEAR(cycles, 90'000, 2000);
}

TEST(CycleController, UsesSmallerIncreasesNearCapacity)
{
	CycleController controller = {};
	const auto probing         = observe(controller, window(30'000));
	const auto tracking        = observe(controller, window(80'000));
	ASSERT_TRUE(probing);
	ASSERT_TRUE(tracking);
	EXPECT_GT(probing->cycles, tracking->cycles);
	EXPECT_LE(probing->cycles, InitialCycles * 2.0);
	EXPECT_LE(tracking->cycles, InitialCycles * 1.25);
}

TEST(CycleController, AccountsForFixedDeviceAndRenderingOverhead)
{
	CycleController controller = {};
	// 200 us of required work leaves 700 us for the CPU at 90% load.
	const auto cycles = run_host(controller, 3000, 100.0, 10'000, 200);
	EXPECT_NEAR(cycles, 70'000, 2000);
}

TEST(CycleController, AdaptsToCapacityLossAndRecovery)
{
	CycleController controller = {};
	const auto initial         = run_host(controller, InitialCycles, 100.0);
	const auto slower          = run_host(controller, initial, 40.0);
	EXPECT_NEAR(slower, 36'000, 1000);
	const auto recovered = run_host(controller, slower, 100.0);
	EXPECT_NEAR(recovered, 90'000, 2000);
}

TEST(CycleController, HigherPercentageRequestsStillTargetNinetyPercent)
{
	for (const auto requested : {90, 95, 100}) {
		CycleController controller = {};
		EXPECT_EQ(
		        observe(controller, window(), InitialCycles, requested)->cycles,
		        InitialCycles);
		const auto high_load = window(96'000);
		observe(controller, high_load, InitialCycles, requested);
		EXPECT_LT(
		        observe(controller, high_load, InitialCycles, requested)->cycles,
		        InitialCycles);
	}
}

TEST(CycleController, AggressiveIncreasesRespectPredictedTarget)
{
	for (const auto work_us : {10'000, 30'000, 60'000, 80'000}) {
		CycleController controller = {};
		const auto result = observe(controller, window(work_us));
		ASSERT_TRUE(result);
		EXPECT_GT(result->cycles, InitialCycles);
		const auto predicted_load = result->load * result->cycles /
		                            InitialCycles;
		EXPECT_LE(predicted_load, 0.90 + 0.00001);
	}
}
