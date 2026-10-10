// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MACOS_WINDOW_H
#define DOSBOX_MACOS_WINDOW_H

// Stops macOS from restoring the last size and position a window had on a
// display when the user drags the window back to that display. Without this,
// AppKit can resize our window to a stale size when the user drops it on
// another display. See the implementation for the details.
//
// Must be called before the window is first moved between displays; we call
// it right after initialising SDL.
void DisableWindowFrameRestoreOnDisplayChange();

#endif // DOSBOX_MACOS_WINDOW_H
