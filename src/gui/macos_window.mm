// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/macos_window.h"

#import <Foundation/Foundation.h>

// The problem
// -----------
//
// Since macOS 12 (Monterey), AppKit remembers the frame (size and position)
// each window last had on each display. When the user drags a window to a
// display and releases the mouse button, AppKit restores the frame the
// window last had on that display, if there is one.
//
// AppKit records the frame whenever the window's frame changes, including
// when the program itself resizes the window (SDL does that via
// `-[NSWindow setFrame:display:]`). When the user resizes the window by
// hand, AppKit invalidates the frames recorded for the other displays, but
// it doesn't do that when the program resizes the window. So windows sized
// by the program can be restored to stale sizes.
//
// For example:
//
//   1. Set `window_size = 800x600` on display A.
//
//   2. Drag the window to display B, then set `window_size = 1000x700`
//      there.
//
//   3. Drag the window back to display A. When the mouse button is
//      released, AppKit restores the 800x600 size the window last had on
//      display A, instead of keeping the 1000x700 size.
//
// We only learn about this via `SDL_EVENT_WINDOW_RESIZED`, which looks just
// like the user resizing the window, so the stale size also ends up in the
// `window_size` setting. This breaks our rules for display changes: windows
// sized in pixels keep their size in pixels, and all other windows keep
// their size in logical units.
//
// The same AppKit behaviour has been reported by others, e.g.:
// https://github.com/iina/iina/issues/4248
//
// The fix
// -------
//
// There is no public API to opt out. We found the undocumented
// `NSScreenLayoutAdjustOnWindowMove` app setting by inspecting AppKit on
// macOS 26:
//
//   - `-[NSWindow _adjustWindowFrame:forMoveFromScreen:toScreen:]` and its
//     `location:` variant perform the restore. Both only do so if
//     `NSScreenLayoutAdjustOnWindowMove` is enabled (the default).
//
//   - The setting is read via `NSUserDefaults` (AppKit's internal
//     `_NSGetBoolAppConfig()` helper), and the result is cached on first
//     use. So we must set it before the window is first moved between
//     displays.
//
// We set it in the registration domain of `NSUserDefaults`, which is not
// persisted. Users can still override it with `defaults write`.
//
// We verified the effect by calling the private method above with and
// without the setting in a test program, and with real window drags between
// a 200% and a 100% scaled display.
//
// Why not something else?
// -----------------------
//
// Undoing the restore after the fact (i.e., setting the size again after
// AppKit has restored the stale size) makes the window visibly jump twice.
// Worse, AppKit's restore can arrive after we've already seen the mouse
// button released and set the correct size, and we can't tell it apart from
// the user resizing the window. The stale size would then win and end up in
// the `window_size` setting.
//
// Risks
// -----
//
// The setting is undocumented, so a future macOS version might ignore it.
// Then the stale restore described above would come back, but nothing else
// would break.
//
// Upstream
// --------
//
// We've reported this to SDL; if SDL disables the restore itself (or adds a
// hint for it), this workaround can be removed:
// https://github.com/libsdl-org/SDL/issues/16485
//
void DisableWindowFrameRestoreOnDisplayChange()
{
	[[NSUserDefaults standardUserDefaults]
	        registerDefaults:@{@"NSScreenLayoutAdjustOnWindowMove": @NO}];
}
