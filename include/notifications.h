/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2025-2025  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_NOTIFICATIONS_H
#define DOSBOX_NOTIFICATIONS_H

#include <string>

#include "ansi_code_markup.h"
#include "console.h"
#include "string_utils.h"

// Notification subsystem
// ----------------------
//
// THe purpose of the notification subsystem is to present messages to the
// user in a uniform way. The central idea is that the various subsystems
// don't directly interact with the logging system, the DOS console, and the
// OSD drawing functions when they want to present information to the user,
// but send more abstract notification messages to the notifications subsystem
// instead.
//
// These messages are tagged with extra data, such as the category and source
// of the notification. The notification subsystem then routes the messages to
// their appropriate destination(s) based on these tags (i.e. to the logs, to
// the DOS console, or to the OSD). A single notification can result in
// messages being send to multiple destinations (e.g. both to the DOS console
// and the logs).
//
// Apart from handling notifications in a uniform manner, this indirection
// will also allow us to introduce fine-grained OSD notification suppression
// for in the future.
//
// Notifications
// -------------
//
// Notification fall roughly into two categories:
//
// - "Positive" user feedback (e.g, changing the cycles setting, starting
//   audio capture, switching between floppy or CD-ROM images, etc.)
//
// - "Negative" user feedback (e.g, warnings when attempting to set an invalid
//   config settings)
//
// Positive feedback gives reassurance to the user than an action initiated by
// them succeeded (e.g., that pressing the screenshot hotkey created a
// screenshot). It can also inform the user about the current value of a
// setting (e.g., when changing the volume or the cycles setting via hotkeys).
//
// Negative feedback should be only used when the user can actually do
// something to rectify the situation (e.g., a warning if they attempted to
// set invalid config values via DOS console commands). From this follows
// that it's still OK to log warnings and errors directly, not every warning
// should be turned into a notification.
//

namespace Notification {

enum class Level {
	// Notifications related to single, discrete events should use this
	// category. The `Info` level is mostly intended for user feedback
	// (e.g., switching to a different Sound Blaster model, muting the
	// audio, changing the monochrome palette, switching to the next
	// mounted ISO image, etc.)
	Info,

	// Notications related to continuous user feedback should prefer this
	// category over `Info` (e.g., continuously changing the cycles
	// settings, the composite video parameters, calibrating the joystick,
	// adjusting the horizontal/vertical video stretch factors, etc.)
	//
	// The notification system is responsible for performing
	// de-duplication or throttling of the message before presenting them
	// to the user; the caller need not to worry about that.
	ContinuousInfo,

	// Warning notifications should be generally used to warn the user
	// about invalid configuration change attempts initiated from the
	// command line. Warnings can also arise asynchronously in response to
	// certain run-time events.
	//
	// GUI interactions should almost never result in warning
	// notifications. The GUI should not allow invalid configuration
	// change attempts by hiding or graying out invalid options.
	//
	// Should be used sparingly and only for discrete warnings events. If
	// multiple similar warnings can arise in quick succession, the caller
	// is responsible for de-duplicating or throttling the warnings and
	// then only raising a single warning notification.
	Warning,

	// Something went really wrong, usually a run-time condition that
	// negatively affects the functioning of the emulator (e.g., a
	// dynamically linked library cannot be loaded, an MT-32 model could
	// not be initialised due to invalid ROM files, etc.)
	//
	// Should be used sparingly and only for discrete error events. If
	// multiple similar errors can arise in quick succession, the caller
	// is responsible for de-duplicating or throttling the errors and then
	// only raising a single error notification.
	Error
};

enum class Source {
	// Notifications in response to running DOS commands or changing
	// configuration settings from the command line.
	Console,

	// Asynchronous notifications arising from run-time conditions (e.g.,
	// a game displaying a message on the Roland MT-32's LCD screen).
	Event,

	// Notifications in response to the user interacting with the GUI.
	Gui,

	// Notifications in response to the user activating a hotkey.
	Hotkey
};

template <typename... Args>
void display_message([[maybe_unused]] const Level level, const Source source,
                     const std::string_view topic, const std::string& format,
                     const Args&... args) noexcept
{
	auto format_log_message = [&]() -> std::string {
		const auto format_with_prefix = topic.empty()
		                                      ? format
		                                      : (std::string(topic) +
		                                         ": " + format);

		const auto str = format_str(format_with_prefix, args...);
		return strip_ansi_markup(str);
	};

	switch (level) {
	case Level::Info: LOG_MSG(format_log_message().c_str()); break;

	case Level::Warning: LOG_WARNING(format_log_message().c_str()); break;

	case Level::Error: LOG_ERR(format_log_message().c_str()); break;
	}

	if (source == Source::CommandLine) {
		CONSOLE_Write(convert_ansi_markup(format.c_str()), args...);
		CONSOLE_Write("\n\n");
	}
}

/*
 * Display an informational message.
 *
 * The `source` parameter determines how the notification will be presented to
 * the user.
 *
 * The `topic` parameter is basically the category of the notification; it
 * should be identical to the log prefixes currently in use (`SB`, `MT32`,
 * `MIXER`, `CPU`, `MOUSE (COM1)`, etc.) If the message gets logged, the topic
 * will be used a log prefix. If the message gets displayed on the OSD, an
 * icon appropriate for the topic might be prepended to the message (e.g. a
 * speaker icon for the `SB`, `MT32`, `MIXER` topics, a processor icon for the
 * `CPU` topic, a mouse icon for `MOUSE (COM1)`, etc.)
 */
template <typename... Args>
void NOTIFY_DisplayInfoMessage(const Source source, const std::string_view topic,
                               const std::string& format, const Args&... args) noexcept
{
	display_message(Level::Info, source, topic, format, args...);
}

/*
 * Display a warning message (see `NOTIFY_DisplayInfoMessage` for details).
 */
template <typename... Args>
void NOTIFY_DisplayWarning(const Source source, const std::string_view topic,
                           const std::string& format, const Args&... args) noexcept
{
	display_message(Level::Warning, source, topic, format, args...);
}

/*
 * Display an error message (see `NOTIFY_DisplayInfoMessage` for details).
 */
template <typename... Args>
void NOTIFY_DisplayError(const Source source, const std::string_view topic,
                         const std::string& format, const Args&... args) noexcept
{
	display_message(Level::Error, source, topic, format, args...);
}

} // namespace Notification

#endif // DOSBOX_NOTIFICATIONS_H
