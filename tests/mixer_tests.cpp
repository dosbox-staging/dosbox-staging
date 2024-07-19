/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include "mixer.h"

#include <gtest/gtest.h>

static void callback(const uint16_t) {}

constexpr auto ChannelName = "TEST";

namespace {

TEST(MixerConfigureFadeOut, Boolean)
{
	MixerChannel channel(callback, ChannelName, {ChannelFeature::Sleep});

	ASSERT_TRUE(channel.ConfigureFadeOut("on"));
	ASSERT_TRUE(channel.ConfigureFadeOut("off"));
}

TEST(MixerConfigureFadeOut, ShortWait)
{
	MixerChannel channel(callback, ChannelName, {ChannelFeature::Sleep});

	ASSERT_TRUE(channel.ConfigureFadeOut("100 10"));
	ASSERT_TRUE(channel.ConfigureFadeOut("100 1500"));
	ASSERT_TRUE(channel.ConfigureFadeOut("100 3000"));
}

TEST(MixerConfigureFadeOut, MediumWait)
{
	MixerChannel channel(callback, ChannelName, {ChannelFeature::Sleep});

	ASSERT_TRUE(channel.ConfigureFadeOut("2500 10"));
	ASSERT_TRUE(channel.ConfigureFadeOut("2500 1500"));
	ASSERT_TRUE(channel.ConfigureFadeOut("2500 3000"));
}

TEST(MixerConfigureFadeOut, LongWait)
{
	MixerChannel channel(callback, ChannelName, {ChannelFeature::Sleep});

	ASSERT_TRUE(channel.ConfigureFadeOut("5000 10"));
	ASSERT_TRUE(channel.ConfigureFadeOut("5000 1500"));
	ASSERT_TRUE(channel.ConfigureFadeOut("5000 3000"));
}

TEST(MixerConfigureFadeOut, JunkStrings)
{
	MixerChannel channel(callback, ChannelName, {ChannelFeature::Sleep});
	// Junk/invalid
	ASSERT_FALSE(channel.ConfigureFadeOut(""));
	ASSERT_FALSE(channel.ConfigureFadeOut("junk"));
	ASSERT_FALSE(channel.ConfigureFadeOut("a b c"));
}

TEST(MixerConfigureFadeOut, OutOfBounds)
{
	MixerChannel channel(callback, ChannelName, {ChannelFeature::Sleep});
	// Out of bounds
	ASSERT_FALSE(channel.ConfigureFadeOut("99 9"));
	ASSERT_FALSE(channel.ConfigureFadeOut("-1 -10000"));
	ASSERT_FALSE(channel.ConfigureFadeOut("3001 10000"));
}

} // namespace
