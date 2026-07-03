// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/render/frame_dirty_tracker.h"

#include <gtest/gtest.h>

namespace {

TEST(FrameDirtyTracker, StartsCleanAndInSync)
{
	FrameDirtyTracker tracker = {};

	EXPECT_FALSE(tracker.IsDirty());
	EXPECT_FALSE(tracker.IsUploadPending());
}

TEST(FrameDirtyTracker, LineOrPaletteChangeMarksDirty)
{
	FrameDirtyTracker tracker = {};

	tracker.MarkDirty();

	EXPECT_TRUE(tracker.IsDirty());

	// No upload is pending until the frame is latched
	EXPECT_FALSE(tracker.IsUploadPending());
}

TEST(FrameDirtyTracker, DirtySurvivesAbortedScanouts)
{
	// There is deliberately NO operation for an aborted scanout: the
	// aborted lines are already in the source cache, so the flag must
	// stay set until a completed frame is latched. This pins the rule
	// the whole upload-skipping design hangs on.
	FrameDirtyTracker tracker = {};

	tracker.MarkDirty();

	// ... scanout aborts here (nothing to call) ...

	EXPECT_TRUE(tracker.IsDirty());
}

TEST(FrameDirtyTracker, LatchingClearsDirtyAndBumpsGeneration)
{
	FrameDirtyTracker tracker = {};

	const auto initial_generation = tracker.LatchGeneration();

	tracker.MarkDirty();
	tracker.NotifyLatched();

	EXPECT_FALSE(tracker.IsDirty());
	EXPECT_EQ(tracker.LatchGeneration(), initial_generation + 1);
	EXPECT_TRUE(tracker.IsUploadPending());
}

TEST(FrameDirtyTracker, UploadingCatchesUpToTheLatch)
{
	FrameDirtyTracker tracker = {};

	tracker.MarkDirty();
	tracker.NotifyLatched();
	tracker.NotifyUploaded();

	EXPECT_FALSE(tracker.IsUploadPending());

	// The next latched frame makes the upload pending again
	tracker.MarkDirty();
	tracker.NotifyLatched();

	EXPECT_TRUE(tracker.IsUploadPending());
}

TEST(FrameDirtyTracker, ForceRedrawInvalidatesTheUploadedGeneration)
{
	FrameDirtyTracker tracker = {};

	// Get in sync first
	tracker.MarkDirty();
	tracker.NotifyLatched();
	tracker.NotifyUploaded();

	// A forced redraw (window redraw, texture recreation) must trigger
	// a re-upload of the existing latch even with no new frame
	tracker.ForceRedraw();

	EXPECT_TRUE(tracker.IsDirty());
	EXPECT_TRUE(tracker.IsUploadPending());

	// The next completed frame goes through the normal cycle
	tracker.NotifyLatched();
	EXPECT_TRUE(tracker.IsUploadPending());

	tracker.NotifyUploaded();
	EXPECT_FALSE(tracker.IsUploadPending());
}

} // namespace
