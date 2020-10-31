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

TEST(SoftLimiter, InboundsProcessAllFrames)
{
	constexpr int frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-3, -2, -1, 0, 1, 2};

	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected{-3, -2, -1, 0, 1, 2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, InboundsProcessPartialFrames)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-3, -2, -1, 0, 1, 2};

	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, 1);
	const SoftLimiter<frames>::out_array_t expected{-3, -2};
	EXPECT_EQ(out[0], expected[0]);
	EXPECT_EQ(out[1], expected[1]);
}

TEST(SoftLimiter, InboundsProcessTooManyFrames)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-3, -2, -1, 0, 1, 2};
	EXPECT_DEBUG_DEATH({ limiter.Apply(in, frames + 1); }, "");
}

TEST(SoftLimiter, OutOfBoundsLeftChannel)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-8.1f,    32000.0f, 65535.0f,
	                                         32000.0f, 4.1f,     32000.0f};

	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected{-4,    32000, 32766,
	                                                32000, 2,     32000};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutOfBoundsRightChannel)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{32000.0f, -3.1f,    32000.0f,
	                                         98304.1f, 32000.0f, 6.1f};

	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected{32000, -1,    32000,
	                                                32765, 32000, 2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBothChannelsPositive)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-8.1f,    -3.1f, 65535.0f,
	                                         98304.1f, 4.1f,  6.1f};

	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected{-4,    -1, 32766,
	                                                32765, 2,  2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBothChannelsNegative)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-8.1f,     -3.1f, -65535.0f,
	                                         -98304.1f, 4.1f,  6.1f};

	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected{-4,     -1, -32766,
	                                                -32765, 2,  2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBothChannelsMixed)
{
	const auto frames = 3;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{40000.0f, -40000.0f,
	                                         65534.0f, -98301.0f,
	                                         40000.0f, -40000.0f};

	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected{19999,  -13332, 32766,
	                                                -32766, 19999,  -13332};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBigOneReleaseStep)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-60000.0f, 80000.0f};
	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, 1);

	in[0] = static_cast<float>(out[0]);
	in[1] = static_cast<float>(out[1]);
	out = limiter.Apply(in, frames);

	const SoftLimiter<frames>::out_array_t expected{-17920, 13434};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBig600ReleaseSteps)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-60000.0f, 80000.0f};
	SoftLimiter<frames>::out_array_t out;

	for (int i = 0; i < 600; ++i) {
		out = limiter.Apply(in, frames);
		in[0] = -32767;
		in[1] = 32768;
	}
	const SoftLimiter<frames>::out_array_t expected{-32766, 32766};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsSmallTwoReleaseSteps)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-32800.0f, 32800.0f};
	SoftLimiter<frames>::out_array_t out;
	for (int i = 0; i < 2; ++i) {
		out = limiter.Apply(in, frames);
		in[0] = -32767;
		in[1] = 32767;
	}
	const SoftLimiter<frames>::out_array_t expected{-32766, 32766};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsSmallTenReleaseSteps)
{
	const auto frames = 1;
	const AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	SoftLimiter<frames>::in_array_t in{-32800.0f, 32800.0f};
	SoftLimiter<frames>::out_array_t out{};

	for (int i = 0; i < 10; ++i) {
		out = limiter.Apply(in, frames);
		in[0] = -32767;
		in[1] = 32768;
	}
	const SoftLimiter<frames>::out_array_t expected{-32766, 32766};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsPolyJoinPositive)
{
	const auto frames = 3;
	AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);

	const SoftLimiter<frames>::in_array_t first_chunk{18000, 18000, 20000,
	                                                  20000, 22000, 22000};
	SoftLimiter<frames>::out_array_t out = limiter.Apply(first_chunk, frames);
	const SoftLimiter<frames>::out_array_t expected_first{18000, 18000,
	                                                      20000, 20000,
	                                                      22000, 22000};
	EXPECT_EQ(out, expected_first);

	const SoftLimiter<frames>::in_array_t second_chunk{30000, 30000, 60000,
	                                                   60000, 30000, 30000};
	out = limiter.Apply(second_chunk, frames);

	const SoftLimiter<frames>::out_array_t expected_second{24266, 24266,
	                                                       32766, 32766,
	                                                       16383, 16383};
	EXPECT_EQ(out, expected_second);
}

TEST(SoftLimiter, OutboundsPolyJoinNegative)
{
	const auto frames = 3;
	AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);

	const SoftLimiter<frames>::in_array_t first_chunk{-18000, -18000,
	                                                  -20000, -20000,
	                                                  -22000, -22000};
	SoftLimiter<frames>::out_array_t out = limiter.Apply(first_chunk, frames);
	const SoftLimiter<frames>::out_array_t expected_first{-18000, -18000,
	                                                      -20000, -20000,
	                                                      -22000, -22000};
	EXPECT_EQ(out, expected_first);

	const SoftLimiter<frames>::in_array_t second_chunk{-30000, -30000,
	                                                   -60000, -60000,
	                                                   -30000, -30000};
	out = limiter.Apply(second_chunk, frames);

	const SoftLimiter<frames>::out_array_t expected_second{-24266, -24266,
	                                                       -32766, -32766,
	                                                       -16383, -16383};
	EXPECT_EQ(out, expected_second);
}

TEST(SoftLimiter, OutboundsJoinWithZeroCross)
{
	const auto frames = 6;
	AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);

	const SoftLimiter<frames>::in_array_t first_chunk{-5000, 1000,  -3000,
	                                                  1000,  -1000, 1000,
	                                                  0,     1000,  3000,
	                                                  1000,  5000,  1000};
	SoftLimiter<frames>::out_array_t out = limiter.Apply(first_chunk, frames);

	const SoftLimiter<frames>::in_array_t second_chunk{
	        15000, 1000, 25000,  1000, 32000,  1000,
	        0,     1000, -15000, 1000, -40000, 1000};
	out = limiter.Apply(second_chunk, frames);

	const SoftLimiter<frames>::out_array_t expected_second{
	        12287, 1000, 20478,  1000, 26212,  1000,
	        0,     1000, -12287, 1000, -32765, 1000};
	EXPECT_EQ(out, expected_second);

	const SoftLimiter<frames>::in_array_t third_chunk{
	        -25000, 1000, -15000, 1000, -10000, 1000,
	        -5000,  1000, 0,      1000, 3000,   1000};
	out = limiter.Apply(third_chunk, frames);

	const SoftLimiter<frames>::out_array_t expected_third{
	        -20524, 1000, -12314, 1000, -8209, 1000,
	        -4104,  1000, 0,      1000, 2462,  1000};
	EXPECT_EQ(out, expected_third);
}

TEST(SoftLimiter, PrescaleAttenuate)
{
	const auto frames = 1;
	AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-30000.1f, 30000.0f};
	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected_first{-30000, 30000};
	EXPECT_EQ(out, expected_first);

	// The limiter holds a reference to the prescaling struct so it can
	// be adjusted on-the-fly via callback. We simulate this callback here.
	prescale.left = 0.5f;
	prescale.right = 0.1f;
	out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected_scaled{-15000, 3000};
	EXPECT_EQ(out, expected_scaled);
}

TEST(SoftLimiter, PrescaleAmplify)
{
	const auto frames = 1;
	AudioFrame prescale{1, 1};
	SoftLimiter<frames> limiter("test-channel", prescale);
	const SoftLimiter<frames>::in_array_t in{-10000.1f, 10000.0f};
	SoftLimiter<frames>::out_array_t out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected_first{-10000, 10000};
	EXPECT_EQ(out, expected_first);

	// The limiter holds a reference to the prescaling struct so it can
	// be adjusted on-the-fly via callback. We simulate this callback here.
	prescale.left = 1.5f;
	prescale.right = 1.1f;
	out = limiter.Apply(in, frames);
	const SoftLimiter<frames>::out_array_t expected_scaled{-15000, 11000};
	EXPECT_EQ(out, expected_scaled);
}

} // namespace
