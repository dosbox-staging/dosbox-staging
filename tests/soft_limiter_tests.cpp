/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-3, -2, -1, 0, 1, 2};

	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected{-3, -2, -1, 0, 1, 2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, InboundsProcessPartialFrames)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-3, -2, -1, 0, 1, 2};

	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, 1, out);
	const std::vector<int16_t> expected{-3, -2};
	EXPECT_EQ(out[0], expected[0]);
	EXPECT_EQ(out[1], expected[1]);
}

TEST(SoftLimiter, InboundsProcessTooManyFrames)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-3, -2, -1, 0, 1, 2};
	std::vector<int16_t> out(frames * 2);
#ifdef _GLIBCXX_ASSERTIONS
	EXPECT_DEATH({ limiter.Process(in, frames + 1, out); }, "");
#else
	EXPECT_DEBUG_DEATH({ limiter.Process(in, frames + 1, out); }, "");
#endif
}

TEST(SoftLimiter, OutOfBoundsLeftChannel)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-8.1f,    32000.0f, 65535.0f,
	                            32000.0f, 4.1f,     32000.0f};

	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected{-4, 32000, 32766, 32000, 2, 32000};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutOfBoundsRightChannel)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{32000.0f, -3.1f,    32000.0f,
	                            98304.1f, 32000.0f, 6.1f};

	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected{32000, -1, 32000, 32765, 32000, 2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBothChannelsPositive)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-8.1f,    -3.1f, 65535.0f,
	                            98304.1f, 4.1f,  6.1f};

	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected{-4, -1, 32766, 32765, 2, 2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBothChannelsNegative)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-8.1f,     -3.1f, -65535.0f,
	                            -98304.1f, 4.1f,  6.1f};

	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected{-4, -1, -32766, -32765, 2, 2};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBothChannelsMixed)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{40000.0f,  -40000.0f, 65534.0f,
	                            -98301.0f, 40000.0f,  -40000.0f};

	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected{19999,  -13332, 32766,
	                                    -32766, 19999,  -13332};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBigOneReleaseStep)
{
	const auto frames = 1;
	SoftLimiter limiter("test-channel");
	std::vector<float> in{-60000.0f, 80000.0f};
	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, 1, out);

	in[0] = static_cast<float>(out[0]);
	in[1] = static_cast<float>(out[1]);
	limiter.Process(in, frames, out);

	const std::vector<int16_t> expected{-17920, 13434};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsBig600ReleaseSteps)
{
	const auto frames = 1;
	SoftLimiter limiter("test-channel");
	std::vector<float> in{-60000.0f, 80000.0f};
	std::vector<int16_t> out(frames * 2);

	for (int i = 0; i < 600; ++i) {
		limiter.Process(in, frames, out);
		in[0] = -32767;
		in[1] = 32768;
	}
	const std::vector<int16_t> expected{-32766, 32766};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsSmallTwoReleaseSteps)
{
	const auto frames = 1;
	SoftLimiter limiter("test-channel");
	std::vector<float> in{-32800.0f, 32800.0f};
	std::vector<int16_t> out(frames * 2);
	for (int i = 0; i < 2; ++i) {
		limiter.Process(in, frames, out);
		in[0] = -32767;
		in[1] = 32767;
	}
	const std::vector<int16_t> expected{-32766, 32766};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsSmallTenReleaseSteps)
{
	const auto frames = 1;
	SoftLimiter limiter("test-channel");
	std::vector<float> in{-32800.0f, 32800.0f};
	std::vector<int16_t> out(frames * 2);

	for (int i = 0; i < 10; ++i) {
		limiter.Process(in, frames, out);
		in[0] = -32767;
		in[1] = 32768;
	}
	const std::vector<int16_t> expected{-32766, 32766};
	EXPECT_EQ(out, expected);
}

TEST(SoftLimiter, OutboundsPolyJoinPositive)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");

	const std::vector<float> first_chunk{18000, 18000, 20000,
	                                     20000, 22000, 22000};
	std::vector<int16_t> out(frames * 2);
	limiter.Process(first_chunk, frames, out);
	const std::vector<int16_t> expected_first{18000, 18000, 20000,
	                                          20000, 22000, 22000};
	EXPECT_EQ(out, expected_first);

	const std::vector<float> second_chunk{30000, 30000, 60000,
	                                      60000, 30000, 30000};
	limiter.Process(second_chunk, frames, out);

	const std::vector<int16_t> expected_second{24266, 24266, 32766,
	                                           32766, 16383, 16383};
	EXPECT_EQ(out, expected_second);
}

TEST(SoftLimiter, OutboundsPolyJoinNegative)
{
	const auto frames = 3;
	SoftLimiter limiter("test-channel");

	const std::vector<float> first_chunk{-18000, -18000, -20000,
	                                     -20000, -22000, -22000};
	std::vector<int16_t> out(frames * 2);
	limiter.Process(first_chunk, frames, out);
	const std::vector<int16_t> expected_first{-18000, -18000, -20000,
	                                          -20000, -22000, -22000};
	EXPECT_EQ(out, expected_first);

	const std::vector<float> second_chunk{-30000, -30000, -60000,
	                                      -60000, -30000, -30000};
	limiter.Process(second_chunk, frames, out);

	const std::vector<int16_t> expected_second{-24266, -24266, -32766,
	                                           -32766, -16383, -16383};
	EXPECT_EQ(out, expected_second);
}

TEST(SoftLimiter, OutboundsJoinWithZeroCross)
{
	const auto frames = 6;
	SoftLimiter limiter("test-channel");

	const std::vector<float> first_chunk{-5000, 1000, -3000, 1000,
	                                     -1000, 1000, 0,     1000,
	                                     3000,  1000, 5000,  1000};
	std::vector<int16_t> out(frames * 2);
	limiter.Process(first_chunk, frames, out);

	const std::vector<float> second_chunk{15000,  1000, 25000,  1000,
	                                      32000,  1000, 0,      1000,
	                                      -15000, 1000, -40000, 1000};
	limiter.Process(second_chunk, frames, out);

	const std::vector<int16_t> expected_second{12287,  1000, 20478,  1000,
	                                           26212,  1000, 0,      1000,
	                                           -12287, 1000, -32765, 1000};
	EXPECT_EQ(out, expected_second);

	const std::vector<float> third_chunk{-25000, 1000, -15000, 1000,
	                                     -10000, 1000, -5000,  1000,
	                                     0,      1000, 3000,   1000};
	limiter.Process(third_chunk, frames, out);

	const std::vector<int16_t> expected_third{-20524, 1000, -12314, 1000,
	                                          -8209,  1000, -4104,  1000,
	                                          0,      1000, 2462,   1000};
	EXPECT_EQ(out, expected_third);
}

TEST(SoftLimiter, ScaleAttenuate)
{
	const auto frames = 1;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-30000.1f, 30000.0f};
	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected_first{-30000, 30000};
	EXPECT_EQ(out, expected_first);

	// The limiter holds a reference to the prescaling struct so it can
	// be adjusted on-the-fly via callback. We simulate this callback here.
	AudioFrame levels = {0.5f, 0.1f};
	const float range_multiplier = 1.0f;
	limiter.UpdateLevels(levels, range_multiplier);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected_scaled{-15000, 3000};
	EXPECT_EQ(out, expected_scaled);
}

TEST(SoftLimiter, ScaleAmplify)
{
	const auto frames = 1;
	SoftLimiter limiter("test-channel");
	const std::vector<float> in{-10000.1f, 10000.0f};
	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected_first{-10000, 10000};
	EXPECT_EQ(out, expected_first);

	// The limiter holds a reference to the prescaling struct so it can
	// be adjusted on-the-fly via callback. We simulate this callback here.
	AudioFrame levels = {1.5f, 1.1f};
	const float range_multiplier = 1.0f;
	limiter.UpdateLevels(levels, range_multiplier);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected_scaled{-15000, 11000};
	EXPECT_EQ(out, expected_scaled);
}

TEST(SoftLimiter, RangeMultiply)
{
	const auto frames = 1;
	SoftLimiter limiter("test-channel");

	AudioFrame levels = {1, 1};
	const float range_multiplier = 2;
	limiter.UpdateLevels(levels, range_multiplier);

	const std::vector<float> in{-10000.1f, 10000.0f};
	std::vector<int16_t> out(frames * 2);
	limiter.Process(in, frames, out);
	const std::vector<int16_t> expected{-20000, 20000};
	EXPECT_EQ(out, expected);
}

} // namespace
