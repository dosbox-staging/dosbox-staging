// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_NOTIFICATIONS_H
#define DOSBOX_NOTIFICATIONS_H

#include <string>

#include "misc/ansi_code_markup.h"
#include "misc/console.h"
#include "shell/shell.h"
#include "utils/string_utils.h"

/*
 * Notification subsystem
 * ======================
 *
 * The purpose of the notification subsystem is to present messages to the
 * user in a uniform way. The central idea is that the various subsystems
 * don't directly interact with the logging system, the DOS console, and the
 * on-screen display (OSD) drawing functions when they want to present
 * information to the user, but send more abstract notification messages to
 * the notifications API instead.
 *
 * These messages are tagged with additional metadata, such as the category
 * and source of the notification. The notification subsystem then routes the
 * messages to their appropriate destination(s) based on these tags (i.e., to
 * the logs, to the DOS console, or the OSD). A single notification can
 * result in messages being sent to multiple destinations (e.g., to the DOS
 * console and the logs).
 *
 * Apart from handling notifications in a uniform manner, this indirection
 * will also allow us to introduce a fine-grained OSD notification suppression
 * feature in the future.
 *
 * Notifications
 * -------------
 *
 * Notifications fall roughly into two categories:
 *
 * - "Positive" user feedback (e.g., changing the number of emulated CPU
 *   cycles, starting audio capture, switching between floppy or CD-ROM
 *   images, etc.)
 *
 * - "Negative" user feedback (e.g., warnings when attempting to set invalid
 *   config settings)
 *
 * Positive feedback gives reassurance that a user-initiated action succeeded
 * (e.g., that pressing the screenshot hotkey created a screenshot). It can
 * also inform the user about the current value of a setting (e.g., when
 * changing the volume or the number of CPU cycles via hotkeys).
 *
 * Negative feedback should be only used when the user can do something to
 * rectify the situation (e.g., a warning if they attempted to set an invalid
 * config setting values via a DOS console command).
 *
 * Note that it's still OK to log warnings and errors directly; not every
 * warning should be turned into a notification.
 */
namespace Notification {

enum class Level {

	/*
	 * Notifications related to single, discrete events should use the
	 * `Info` level. This is mostly intended for user feedback (e.g.,
	 * switching to a different Sound Blaster model, muting the audio,
	 * changing the monochrome palette, switching to the next mounted ISO
	 * image, etc.)
	 */
	Info,

	/*
	 * Notifications related to continuously adjustable settings should
	 * prefer the `ContinuousInfo` over `Info` (e.g., continuously changing
	 * the cycles setting, the composite video parameters, calibrating the
	 * joystick, adjusting the horizontal/vertical video stretch factors,
	 * etc.)
	 *
	 * The notification system is responsible for performing de-duplication
	 * or throttling of the messages before presenting them to the user; the
	 * caller need not worry about that.
	 */
	ContinuousInfo,

	/*
	 * The `Warning` notification level should be generally used to warn
	 * about invalid configuration change attempts initiated by the user
	 * from the command line. Warnings can also arise asynchronously in
	 * response to certain runtime events.
	 *
	 * GUI interactions should almost never result in warning notifications.
	 * The GUI should not allow invalid configuration change attempts by
	 * hiding or greying out invalid options.
	 *
	 * Should be used sparingly and only for discrete warnings events. If
	 * multiple similar warnings can arise in quick succession, the caller
	 * is responsible for de-duplicating or throttling the warnings.
	 */
	Warning,

	/*
	 * The `Error` level is reserved for situations when something goes
	 * really wrong. This is usually a runtime condition that negatively
	 * affects the functioning of the emulator (e.g., a dynamically linked
	 * library cannot be loaded, an MT-32 model could not be initialised due
	 * to an invalid ROM file, etc.)
	 *
	 * Should be used sparingly and only for discrete error events. If
	 * multiple similar errors can arise in quick succession, the caller
	 * is responsible for de-duplicating or throttling the errors.
	 */
	Error
};

enum class Source {
	// Notifications in response to running DOS commands or changing
	// configuration settings from the DOS console.
	Console,

	// Asynchronous notifications arising from runtime conditions (e.g., a
	// game displaying a message on the Roland MT-32's LCD screen).
	Event,

	// Notifications in response to the user interacting with the GUI.
	Gui,

	// Notifications in response to the user activating a hotkey.
	Hotkey
};

template <typename... Args>
void display_message([[maybe_unused]] const Level level,
                     [[maybe_unused]] const Source source,
                     const std::string_view topic,
                     const std::string& message_key, const Args&... args) noexcept
{
	auto format_log_message = [&]() -> std::string {
		const auto format = replace(
		        strip_ansi_markup(MSG_GetEnglishRaw(message_key)), '\n', ' ');

		const auto format_with_prefix = topic.empty()
		                                      ? format
		                                      : (std::string(topic) +
		                                         ": " + format);

		return format_str(format_with_prefix, args...);
	};

	switch (level) {
	case Level::Info: LOG_MSG("%s", format_log_message().c_str()); break;

	case Level::Warning:
		LOG_WARNING("%s", format_log_message().c_str());
		break;

	case Level::Error: LOG_ERR("%s", format_log_message().c_str()); break;

	default: assertm(false, "Invalid Notification::Level value");
	}

	// If a notification is displayed during startup before the console is
	// initialised, don't try to print it to the console as that would
	// result in a crash. One way this can happen is when setting an
	// out-of-range config value for a config that will be only applied
	// after a restart (e.g., `memsize 123456` followed by `config -r`).
	//
	if (SHELL_IsRunning()) {
		const auto& format = MSG_Get(message_key);
		const auto str     = format_str(format, args...);

		CONSOLE_Write(convert_ansi_markup(str));
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
 * will be used as a log prefix. If the message gets displayed on the OSD, an
 * icon appropriate for the topic might be prepended to the message (e.g. a
 * speaker icon for the `SB`, `MT32`, `MIXER` topics, a processor icon for the
 * `CPU` topic, a mouse icon for `MOUSE (COM1)`, etc.)
 *
 * See `Notification::Level::Info` for further details.
 */
template <typename... Args>
void NOTIFY_DisplayInfoMessage(const Source source, const std::string_view topic,
                               const std::string& message_key,
                               const Args&... args) noexcept
{
	display_message(Level::Info, source, topic, message_key, args...);
}

/*
 * Display a informational message in response to the user changing a
 * continuously adjustable setting.
 *
 * See `NOTIFY_DisplayInfoMessage` and `Notification::Level::ContinuousInfo`
 * for further details.
 */
template <typename... Args>
void NOTIFY_DisplayContinuousInfoMessage(const Source source,
                                         const std::string_view topic,
                                         const std::string& message_key,
                                         const Args&... args) noexcept
{
	display_message(Level::ContinuousInfo, source, topic, message_key, args...);
}

/*
 * Display a warning message.
 *
 * See `NOTIFY_DisplayInfoMessage` and `Notification::Level::Warning` for
 * further details.
 */
template <typename... Args>
void NOTIFY_DisplayWarning(const Source source, const std::string_view topic,
                           const std::string& message_key, const Args&... args) noexcept
{
	display_message(Level::Warning, source, topic, message_key, args...);
}

/*
 * Display an error message.
 *
 *
 * See `NOTIFY_DisplayInfoMessage` and `Notification::Level::Error` for
 * further details.
 */
template <typename... Args>
void NOTIFY_DisplayError(const Source source, const std::string_view topic,
                         const std::string& message_key, const Args&... args) noexcept
{
	display_message(Level::Error, source, topic, message_key, args...);
}

} // namespace Notification

#endif // DOSBOX_NOTIFICATIONS_H
