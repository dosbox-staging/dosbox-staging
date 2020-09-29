/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  The dosbox-staging team
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

#include "soft_limiter.h"

#include <gtest/gtest.h>

namespace {

TEST(SoftLimiter, FullFrames)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-3, -2, -1, 0, 1, 2};
	SoftLimiter<frames>::out_array_t out{};

	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected{-3, -2, -1, 0, 1, 2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, PartialFrames)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-3, -2, -1, 0, 1, 2};
	SoftLimiter<frames>::out_array_t out{};

	limiter.Apply(in, out, 1);
	const SoftLimiter<frames>::out_array_t expected{-3, -2};
	EXPECT_EQ(out[0], expected[0]);
	EXPECT_EQ(out[1], expected[1]);
}

TEST(SoftLimiter, ExcessiveFrames)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-3, -2, -1, 0, 1, 2};
	SoftLimiter<frames>::out_array_t out{};
	EXPECT_DEBUG_DEATH({ limiter.Apply(in, out, frames + 1); }, "");
}

TEST(SoftLimiter, PositiveOverrun)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-8.1f,    -3.1f, 65535.0f,
	                                         98304.1f, 4.1f,  6.1f};
	SoftLimiter<frames>::out_array_t out{};

	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected{-4,    -1, 32767,
	                                                32767, 2,  2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, NegativeOverrun)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-8.1f,     -3.1f, -65535.0f,
	                                         -98304.1f, 4.1f,  6.1f};
	SoftLimiter<frames>::out_array_t out{};

	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected{-4,     -1, -32767,
	                                                -32767, 2,  2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, MixedOverrun)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{40000.1f, -40000.1f,
	                                         65534.0f, -98301.0f,
	                                         40000.1f, -40000.1f};
	SoftLimiter<frames>::out_array_t out{};

	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected{20000,  -13333, 32767,
	                                                -32767, 20000,  -13333};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, BigScaleOneRelease)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-60000.0f, 80000.0f};
	SoftLimiter<frames>::out_array_t out{};
	limiter.Apply(in, out, 1);

	in[0] = static_cast<float>(out[0]);
	in[1] = static_cast<float>(out[1]);
	limiter.Apply(in, out, frames);

	const SoftLimiter<frames>::out_array_t expected{-17920, 13435};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, BigScaleSixHundredReleases)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-60000.0f, 80000.0f};
	SoftLimiter<frames>::out_array_t out{};
	limiter.Apply(in, out, 1);

	for (int i = 0; i < 600; ++i) {
		limiter.Apply(in, out, frames);
		in[0] = -32767;
		in[1] = 32768;
	}
	const SoftLimiter<frames>::out_array_t expected{-32767, 32767};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, SmallScaleTwoReleases)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-32800.0f, 32800.0f};
	SoftLimiter<frames>::out_array_t out{};

	for (int i = 0; i < 2; ++i) {
		limiter.Apply(in, out, frames);
		in[0] = -32767;
		in[1] = 32767;
	}
	const SoftLimiter<frames>::out_array_t expected{-32734, 32734};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, SmallScaleTenReleases)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-32800.0f, 32800.0f};
	SoftLimiter<frames>::out_array_t out{};

	for (int i = 0; i < 10; ++i) {
		limiter.Apply(in, out, frames);
		in[0] = -32767;
		in[1] = 32768;
	}
	const SoftLimiter<frames>::out_array_t expected{-32767, 32767};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, AdjustPrescaleDown)
{
	const auto frames = 1;
	AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-30000.1f, 30000.0f};
	SoftLimiter<frames>::out_array_t out{};

	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected_first{-30000, 30000};
	EXPECT_EQ(out, expected_first);

	// The limiter holds a reference to the prescaling struct so it can
	// be adjusted on-the-fly via callback. We simulate this callback here.
	prescale.left = 0.5f;
	prescale.right = 0.1f;
	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected_scaled{-15000, 3000};
	EXPECT_EQ(out, expected_scaled);
}

TEST(SoftLimiter, AdjustPrescaleUp)
{
	const auto frames = 1;
	AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-10000.1f, 10000.0f};
	SoftLimiter<frames>::out_array_t out{};

	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected_first{-10000, 10000};
	EXPECT_EQ(out, expected_first);

	// The limiter holds a reference to the prescaling struct so it can
	// be adjusted on-the-fly via callback. We simulate this callback here.
	prescale.left = 1.5f;
	prescale.right = 1.1f;
	limiter.Apply(in, out, frames);
	const SoftLimiter<frames>::out_array_t expected_scaled{-15000, 11000};
	EXPECT_EQ(out, expected_scaled);
}

} // namespace
