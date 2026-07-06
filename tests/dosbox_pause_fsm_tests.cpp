// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox_pause_fsm.h"

#include <gtest/gtest.h>

#include <optional>

namespace {

using enum PauseState;

const char* name(const PauseState state)
{
	switch (state) {
	case Running: return "Running";
	case UserPausePending: return "UserPausePending";
	case AutoPausePending: return "AutoPausePending";
	case UserPaused: return "UserPaused";
	case AutoPaused: return "AutoPaused";
	}
	return "?";
}

const char* name(const PauseEvent event)
{
	switch (event) {
	case PauseEvent::UserPause: return "UserPause";
	case PauseEvent::UserResume: return "UserResume";
	case PauseEvent::AutoPause: return "AutoPause";
	case PauseEvent::AutoResume: return "AutoResume";
	case PauseEvent::FadeComplete: return "FadeComplete";
	}
	return "?";
}

// --------------------------------------------------------------------------
// Behaviourally-important rules, spelled out
// --------------------------------------------------------------------------

TEST(PauseFsm, UserPauseEngagesFromRunning)
{
	EXPECT_EQ(NextPauseState(Running, PauseEvent::UserPause), UserPausePending);
}

TEST(PauseFsm, UserPauseIsIdempotent)
{
	EXPECT_EQ(NextPauseState(UserPausePending, PauseEvent::UserPause),
	          std::nullopt);
	EXPECT_EQ(NextPauseState(UserPaused, PauseEvent::UserPause), std::nullopt);
}

TEST(PauseFsm, UserPauseUpgradesAutoPause)
{
	// A manual pause while auto-paused must stick around after focus gain,
	// so it upgrades to the user-owned states.
	EXPECT_EQ(NextPauseState(AutoPausePending, PauseEvent::UserPause),
	          UserPausePending);
	EXPECT_EQ(NextPauseState(AutoPaused, PauseEvent::UserPause), UserPaused);
}

TEST(PauseFsm, UserResumeClearsUserPauseOnly)
{
	EXPECT_EQ(NextPauseState(UserPausePending, PauseEvent::UserResume), Running);
	EXPECT_EQ(NextPauseState(UserPaused, PauseEvent::UserResume), Running);

	// Auto-pauses are left for the auto-resume path.
	EXPECT_EQ(NextPauseState(AutoPausePending, PauseEvent::UserResume),
	          std::nullopt);

	EXPECT_EQ(NextPauseState(AutoPaused, PauseEvent::UserResume), std::nullopt);
}

TEST(PauseFsm, AutoPauseEngagesOnlyFromRunning)
{
	EXPECT_EQ(NextPauseState(Running, PauseEvent::AutoPause), AutoPausePending);

	// A user pause survives an auto-pause signal; an auto-pause is idempotent.
	EXPECT_EQ(NextPauseState(UserPausePending, PauseEvent::AutoPause),
	          std::nullopt);

	EXPECT_EQ(NextPauseState(UserPaused, PauseEvent::AutoPause), std::nullopt);
	EXPECT_EQ(NextPauseState(AutoPausePending, PauseEvent::AutoPause),
	          std::nullopt);

	EXPECT_EQ(NextPauseState(AutoPaused, PauseEvent::AutoPause), std::nullopt);
}

TEST(PauseFsm, AutoResumeNeverResumesUserPause)
{
	EXPECT_EQ(NextPauseState(AutoPausePending, PauseEvent::AutoResume), Running);
	EXPECT_EQ(NextPauseState(AutoPaused, PauseEvent::AutoResume), Running);

	// The whole point of the user/auto split: focus gain must not undo a
	// manual pause.
	EXPECT_EQ(NextPauseState(UserPausePending, PauseEvent::AutoResume),
	          std::nullopt);

	EXPECT_EQ(NextPauseState(UserPaused, PauseEvent::AutoResume), std::nullopt);
}

TEST(PauseFsm, FadeCompleteEngagesPendingPause)
{
	EXPECT_EQ(NextPauseState(UserPausePending, PauseEvent::FadeComplete),
	          UserPaused);

	EXPECT_EQ(NextPauseState(AutoPausePending, PauseEvent::FadeComplete),
	          AutoPaused);

	// Only meaningful while a pause is pending.
	EXPECT_EQ(NextPauseState(Running, PauseEvent::FadeComplete), std::nullopt);
	EXPECT_EQ(NextPauseState(UserPaused, PauseEvent::FadeComplete), std::nullopt);
	EXPECT_EQ(NextPauseState(AutoPaused, PauseEvent::FadeComplete), std::nullopt);
}

// --------------------------------------------------------------------------
// Full transition matrix
// --------------------------------------------------------------------------
//
// Guards the entire decision table so any accidental change to a single
// cell is caught, not just the rules exercised above.

TEST(PauseFsm, FullTransitionMatrix)
{
	struct Case {
		PauseState from              = {};
		PauseEvent event             = {};
		std::optional<PauseState> to = {};
	};

	const Case cases[] = {
	        {         Running,    PauseEvent::UserPause, UserPausePending},
	        {         Running,   PauseEvent::UserResume,     std::nullopt},
	        {         Running,    PauseEvent::AutoPause, AutoPausePending},
	        {         Running,   PauseEvent::AutoResume,     std::nullopt},
	        {         Running, PauseEvent::FadeComplete,     std::nullopt},

	        {UserPausePending,    PauseEvent::UserPause,     std::nullopt},
	        {UserPausePending,   PauseEvent::UserResume,          Running},
	        {UserPausePending,    PauseEvent::AutoPause,     std::nullopt},
	        {UserPausePending,   PauseEvent::AutoResume,     std::nullopt},
	        {UserPausePending, PauseEvent::FadeComplete,       UserPaused},

	        {AutoPausePending,    PauseEvent::UserPause, UserPausePending},
	        {AutoPausePending,   PauseEvent::UserResume,     std::nullopt},
	        {AutoPausePending,    PauseEvent::AutoPause,     std::nullopt},
	        {AutoPausePending,   PauseEvent::AutoResume,          Running},
	        {AutoPausePending, PauseEvent::FadeComplete,       AutoPaused},

	        {      UserPaused,    PauseEvent::UserPause,     std::nullopt},
	        {      UserPaused,   PauseEvent::UserResume,          Running},
	        {      UserPaused,    PauseEvent::AutoPause,     std::nullopt},
	        {      UserPaused,   PauseEvent::AutoResume,     std::nullopt},
	        {      UserPaused, PauseEvent::FadeComplete,     std::nullopt},

	        {      AutoPaused,    PauseEvent::UserPause,       UserPaused},
	        {      AutoPaused,   PauseEvent::UserResume,     std::nullopt},
	        {      AutoPaused,    PauseEvent::AutoPause,     std::nullopt},
	        {      AutoPaused,   PauseEvent::AutoResume,          Running},
	        {      AutoPaused, PauseEvent::FadeComplete,     std::nullopt},
	};

	for (const auto& c : cases) {
		EXPECT_EQ(NextPauseState(c.from, c.event), c.to)
		        << "from=" << name(c.from) << " event=" << name(c.event);
	}
}

} // namespace
