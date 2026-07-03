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

TEST(FrameDirtyTracker, RowMarksAccumulateIntoTheDirtySpan)
{
	FrameDirtyTracker tracker = {};

	tracker.MarkRowDirty(42);
	tracker.MarkRowDirty(7);
	tracker.MarkRowDirty(13);

	EXPECT_TRUE(tracker.IsDirty());
	EXPECT_EQ(tracker.DirtyRowSpan().first, 7);
	EXPECT_EQ(tracker.DirtyRowSpan().last, 42);
}

TEST(FrameDirtyTracker, PaletteChangeDirtiesTheWholeFrame)
{
	FrameDirtyTracker tracker = {};

	tracker.MarkRowDirty(100);
	tracker.MarkDirty();

	EXPECT_EQ(tracker.DirtyRowSpan().first, 0);
	EXPECT_EQ(tracker.DirtyRowSpan().last, FrameDirtyTracker::WholeFrameRows);
}

TEST(FrameDirtyTracker, LatchingMovesTheDirtySpanToThePendingUpload)
{
	FrameDirtyTracker tracker = {};

	tracker.MarkRowDirty(10);
	tracker.MarkRowDirty(20);
	tracker.NotifyLatched();

	EXPECT_EQ(tracker.PendingUploadRowSpan().first, 10);
	EXPECT_EQ(tracker.PendingUploadRowSpan().last, 20);

	// The dirty span starts fresh: rows marked after the latch must not
	// widen the already-latched span
	tracker.MarkRowDirty(500);

	EXPECT_EQ(tracker.PendingUploadRowSpan().first, 10);
	EXPECT_EQ(tracker.PendingUploadRowSpan().last, 20);
	EXPECT_EQ(tracker.DirtyRowSpan().first, 500);
	EXPECT_EQ(tracker.DirtyRowSpan().last, 500);
}

TEST(FrameDirtyTracker, UnpresentedLatchesUnionTheirPendingSpans)
{
	// Presents can lag the DOS rate: when a second frame is latched
	// before the first was uploaded, the upload must cover both.
	FrameDirtyTracker tracker = {};

	tracker.MarkRowDirty(10);
	tracker.NotifyLatched();

	tracker.MarkRowDirty(300);
	tracker.NotifyLatched();

	EXPECT_EQ(tracker.PendingUploadRowSpan().first, 10);
	EXPECT_EQ(tracker.PendingUploadRowSpan().last, 300);
}

TEST(FrameDirtyTracker, UploadingClearsThePendingSpan)
{
	FrameDirtyTracker tracker = {};

	tracker.MarkRowDirty(10);
	tracker.NotifyLatched();
	tracker.NotifyUploaded();

	tracker.MarkRowDirty(200);
	tracker.NotifyLatched();

	// Only the new frame's rows are pending, not the already-uploaded
	// ones
	EXPECT_EQ(tracker.PendingUploadRowSpan().first, 200);
	EXPECT_EQ(tracker.PendingUploadRowSpan().last, 200);
}

TEST(FrameDirtyTracker, SpanSurvivesAbortedScanouts)
{
	// Same rule as the flag: aborted rows are already in the source
	// cache, so their span must stay pending for the eventual latch.
	FrameDirtyTracker tracker = {};

	tracker.MarkRowDirty(50);

	// ... scanout aborts here (nothing to call) ...

	tracker.MarkRowDirty(60);
	tracker.NotifyLatched();

	EXPECT_EQ(tracker.PendingUploadRowSpan().first, 50);
	EXPECT_EQ(tracker.PendingUploadRowSpan().last, 60);
}

TEST(FrameDirtyTracker, ForceRedrawPendsTheWholeFrame)
{
	FrameDirtyTracker tracker = {};

	// Get in sync first
	tracker.MarkRowDirty(10);
	tracker.NotifyLatched();
	tracker.NotifyUploaded();

	tracker.ForceRedraw();

	EXPECT_EQ(tracker.PendingUploadRowSpan().first, 0);
	EXPECT_EQ(tracker.PendingUploadRowSpan().last,
	          FrameDirtyTracker::WholeFrameRows);

	EXPECT_EQ(tracker.DirtyRowSpan().first, 0);
	EXPECT_EQ(tracker.DirtyRowSpan().last, FrameDirtyTracker::WholeFrameRows);
}

} // namespace
