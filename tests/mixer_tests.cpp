// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio/mixer.h"

#include <optional>

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

// --------------------------------------------------------------------------
// Mute FSM policy
// --------------------------------------------------------------------------

using enum MixerMuteState;

const char* name(const MixerMuteState state)
{
	switch (state) {
	case Audible: return "Audible";
	case UserMuted: return "UserMuted";
	case AutoMuted: return "AutoMuted";
	}
	return "?";
}

const char* name(const MuteRequest request)
{
	switch (request) {
	case MuteRequest::UserMute: return "UserMute";
	case MuteRequest::UserUnmute: return "UserUnmute";
	case MuteRequest::AutoMute: return "AutoMute";
	case MuteRequest::AutoUnmute: return "AutoUnmute";
	}
	return "?";
}

TEST(MixerMuteFsm, UserMuteTrumpsAutoMute)
{
	// A user mute engages from Audible and overrides an auto-mute; it is
	// idempotent once user-muted.
	EXPECT_EQ(MIXER_NextMuteState(Audible, MuteRequest::UserMute), UserMuted);
	EXPECT_EQ(MIXER_NextMuteState(AutoMuted, MuteRequest::UserMute), UserMuted);
	EXPECT_EQ(MIXER_NextMuteState(UserMuted, MuteRequest::UserMute), std::nullopt);
}

TEST(MixerMuteFsm, UserUnmuteClearsEitherMute)
{
	EXPECT_EQ(MIXER_NextMuteState(UserMuted, MuteRequest::UserUnmute), Audible);
	EXPECT_EQ(MIXER_NextMuteState(AutoMuted, MuteRequest::UserUnmute), Audible);
	EXPECT_EQ(MIXER_NextMuteState(Audible, MuteRequest::UserUnmute), std::nullopt);
}

TEST(MixerMuteFsm, AutoMuteEngagesOnlyFromAudible)
{
	EXPECT_EQ(MIXER_NextMuteState(Audible, MuteRequest::AutoMute), AutoMuted);

	// A user-mute survives focus loss; an existing auto-mute is idempotent.
	EXPECT_EQ(MIXER_NextMuteState(UserMuted, MuteRequest::AutoMute), std::nullopt);
	EXPECT_EQ(MIXER_NextMuteState(AutoMuted, MuteRequest::AutoMute), std::nullopt);
}

TEST(MixerMuteFsm, AutoUnmuteNeverClearsUserMute)
{
	EXPECT_EQ(MIXER_NextMuteState(AutoMuted, MuteRequest::AutoUnmute), Audible);

	// Focus gain must not clear a user-mute.
	EXPECT_EQ(MIXER_NextMuteState(UserMuted, MuteRequest::AutoUnmute),
	          std::nullopt);
	EXPECT_EQ(MIXER_NextMuteState(Audible, MuteRequest::AutoUnmute), std::nullopt);
}

TEST(MixerMuteFsm, FullTransitionMatrix)
{
	struct Case {
		MixerMuteState from              = {};
		MuteRequest request              = {};
		std::optional<MixerMuteState> to = {};
	};

	const Case cases[] = {
	        {  Audible,   MuteRequest::UserMute,    UserMuted},
	        {  Audible, MuteRequest::UserUnmute, std::nullopt},
	        {  Audible,   MuteRequest::AutoMute,    AutoMuted},
	        {  Audible, MuteRequest::AutoUnmute, std::nullopt},

	        {UserMuted,   MuteRequest::UserMute, std::nullopt},
	        {UserMuted, MuteRequest::UserUnmute,      Audible},
	        {UserMuted,   MuteRequest::AutoMute, std::nullopt},
	        {UserMuted, MuteRequest::AutoUnmute, std::nullopt},

	        {AutoMuted,   MuteRequest::UserMute,    UserMuted},
	        {AutoMuted, MuteRequest::UserUnmute,      Audible},
	        {AutoMuted,   MuteRequest::AutoMute, std::nullopt},
	        {AutoMuted, MuteRequest::AutoUnmute,      Audible},
	};

	for (const auto& c : cases) {
		EXPECT_EQ(MIXER_NextMuteState(c.from, c.request), c.to)
		        << "from=" << name(c.from)
		        << " request=" << name(c.request);
	}
}

} // namespace
