// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_FRAME_DIRTY_TRACKER_H
#define DOSBOX_RENDER_FRAME_DIRTY_TRACKER_H

#include <cstdint>

// Tracks whether the live source frame differs from the last latched one,
// and whether the render backend has uploaded the latest latch.
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
class FrameDirtyTracker {
public:
	// The live source frame differs from the last latched one (a source
	// line or the palette changed)
	void MarkDirty()
	{
		dirty = true;
	}

	bool IsDirty() const
	{
		return dirty;
	}

	// The completed dirty frame was latched: it is the new comparison
	// baseline and a new latch generation
	void NotifyLatched()
	{
		dirty = false;
		++latch_generation;
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
	}

	bool IsUploadPending() const
	{
		return uploaded_generation != latch_generation;
	}

	void NotifyUploaded()
	{
		uploaded_generation = latch_generation;
	}

private:
	static constexpr int64_t InvalidGeneration = -1;

	bool dirty = false;

	int64_t latch_generation    = 0;
	int64_t uploaded_generation = 0;
};

#endif // DOSBOX_RENDER_FRAME_DIRTY_TRACKER_H
