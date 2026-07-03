// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_FRAME_DIRTY_TRACKER_H
#define DOSBOX_RENDER_FRAME_DIRTY_TRACKER_H

#include <algorithm>
#include <cstdint>
#include <limits>

// A range of source frame rows, inclusive on both ends. `last` can be
// `FrameDirtyTracker::WholeFrameRows` for whole-frame invalidations, so
// consumers must clamp it to the frame height.
struct RowSpan {
	int first = 0;
	int last  = 0;
};

// Tracks whether the live source frame differs from the last latched one
// (and which rows), and whether the render backend has uploaded the latest
// latch (and which rows of it).
//
// The dirty flag means "the live source cache differs from the last
// latched frame", NOT "changed during this scanout". The rules:
//
//  - Set on any source line change and on a palette change.
//
//  - An aborted scanout does NOT clear it (there is deliberately no
//    operation for it): the aborted scanout's lines are already in the
//    source cache, so a follow-up frame with identical content would
//    compare clean against the cache and never be latched or uploaded --
//    the screen would keep showing the pre-abort frame forever.
//
//  - Cleared only when the completed frame is latched, which starts a new
//    latch generation.
//
// The upload side is generation-based: the backend uploads whenever the
// latch generation differs from the one it last uploaded, so redundant
// uploads of unchanged frames are skipped. Forcing a redraw (window
// redraws, texture recreation) invalidates the uploaded generation, which
// makes the next present re-expand and re-upload the existing latch even
// when no new frame has arrived.
//
// The row spans follow their owning state's lifecycle: the dirty span
// accumulates every marked row (surviving aborted scanouts, like the flag)
// and bounds the latch copy; latching moves it into the pending upload
// span -- unioned, so a latch that was never presented keeps its rows
// pending -- which bounds the expansion and upload and is cleared by the
// upload.
//
class FrameDirtyTracker {
public:
	static constexpr int WholeFrameRows = std::numeric_limits<int>::max();

	// A source line changed
	void MarkRowDirty(const int row)
	{
		dirty = true;

		dirty_span.first = std::min(dirty_span.first, row);
		dirty_span.last  = std::max(dirty_span.last, row);
	}

	// Something changed that potentially affects every pixel (e.g. a
	// palette change in an indexed colour mode)
	void MarkDirty()
	{
		dirty      = true;
		dirty_span = {0, WholeFrameRows};
	}

	bool IsDirty() const
	{
		return dirty;
	}

	// Rows changed since the last latch; only meaningful when IsDirty()
	RowSpan DirtyRowSpan() const
	{
		return dirty_span;
	}

	// The completed dirty frame was latched: it is the new comparison
	// baseline and a new latch generation
	void NotifyLatched()
	{
		dirty = false;
		++latch_generation;

		pending_upload_span.first = std::min(pending_upload_span.first,
		                                     dirty_span.first);
		pending_upload_span.last  = std::max(pending_upload_span.last,
                                                    dirty_span.last);

		dirty_span = EmptySpan;
	}

	int64_t LatchGeneration() const
	{
		return latch_generation;
	}

	// Force the next latch and upload even if the frame content is
	// unchanged
	void ForceRedraw()
	{
		dirty               = true;
		uploaded_generation = InvalidGeneration;

		dirty_span          = {0, WholeFrameRows};
		pending_upload_span = {0, WholeFrameRows};
	}

	bool IsUploadPending() const
	{
		return uploaded_generation != latch_generation;
	}

	// Latched rows the backend hasn't uploaded yet; only meaningful when
	// IsUploadPending()
	RowSpan PendingUploadRowSpan() const
	{
		return pending_upload_span;
	}

	void NotifyUploaded()
	{
		uploaded_generation = latch_generation;

		pending_upload_span = EmptySpan;
	}

private:
	static constexpr int64_t InvalidGeneration = -1;

	// `first > last`, so any marked row starts a fresh span
	static constexpr RowSpan EmptySpan = {WholeFrameRows, -1};

	bool dirty = false;

	RowSpan dirty_span          = EmptySpan;
	RowSpan pending_upload_span = EmptySpan;

	int64_t latch_generation    = 0;
	int64_t uploaded_generation = 0;
};

#endif // DOSBOX_RENDER_FRAME_DIRTY_TRACKER_H
