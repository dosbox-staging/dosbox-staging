// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PAUSE_FSM_H
#define DOSBOX_PAUSE_FSM_H

#include <optional>

// Pure pause-transition policy, split out from the side-effecting
// application layer in `dosbox.cpp` so the decision table can be unit
// tested in isolation. The threading, subsystem hooks, and the mutex all
// live in `dosbox.cpp`; only the "given this state and this event, what
// happens" logic lives here, and it is free of side effects and globals.

enum class PauseState {
	// Emulator running normally.
	Running,

	// A user pause (via hotkey or HTTP API) has been requested but the
	// audio fade-out that starts on pause is still in progress (~5 ms).
	//
	// The emulator core, mixer, MIDI renderer, and capture path all still
	// run normally -- that's what supplies the audio the fade-out
	// attenuates. Transitions to `UserPaused` on fade-out completion.
	UserPausePending,

	// Same as `UserPausePending`, but entered on auto-pause from window
	// focus loss (when the `pause_when_inactive` conf setting is enabled).
	// Transitions to `AutoPaused` on fade-out completion.
	//
	// Gets "upgraded" to `UserPausePending` if the user activates pause
	// mode while auto-pausing is pending -- it shouldn't auto-resume on
	// focus gain after that. The reverse never happens.
	AutoPausePending,

	// User-initiated pause is fully engaged (subsystems halted).
	UserPaused,

	// Auto-paused when the window lost focus (if the `pause_when_inactive`
	// conf setting is enabled); only the auto-resume path clears this,
	// leaving user-pauses alone.
	//
	// Gets "upgraded" to `UserPaused` if the user activates pause mode
	// while auto-paused -- it shouldn't auto-resume on focus gain after
	// that. The reverse never happens.
	AutoPaused,
};

// Intent signals that drive the pause FSM.
enum class PauseEvent {
	// These map to user/auto pause/resume events (`DOSBOX_Request*()` API in
	// `dosbox.cpp`)
	UserPause,
	UserResume,
	AutoPause,
	AutoResume,

	// Raised once the audio fade-out reaches zero, engaging a pending pause
	FadeComplete,
};

// Gets the next state for `event` in `current`, or `nullopt` if the event is
// a no-op in that state.
std::optional<PauseState> NextPauseState(PauseState current, PauseEvent event);

#endif // DOSBOX_PAUSE_FSM_H
