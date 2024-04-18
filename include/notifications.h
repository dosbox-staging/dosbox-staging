/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

enum class NotificationLevel { Info, Warning, Error };

template <typename... Args>
void notify([[maybe_unused]] const NotificationLevel level,
            const std::string_view topic, const std::string& format,
            const Args&... args) noexcept
{
	auto format_log_message = [&]() -> std::string {
		const auto format_with_prefix =
		        topic.empty()
		                ? format
		                : (std::string(topic) + ": " + format);

		const auto str = format_str(format_with_prefix, args...);
		return strip_ansi_markup(str);
	};

	switch (level) {
	case NotificationLevel::Info:
		LOG_MSG(format_log_message().c_str());
		break;

	case NotificationLevel::Warning:
		LOG_WARNING(format_log_message().c_str());
		break;

	case NotificationLevel::Error:
		LOG_ERR(format_log_message().c_str());
		break;
	}

	CONSOLE_Write(convert_ansi_markup(format.c_str()), args...);
	CONSOLE_Write("\n\n");
}

template <typename... Args>
void NOTIFY_InfoMsg(const std::string_view topic, const std::string& format,
                    const Args&... args) noexcept
{
	notify(NotificationLevel::Info, topic, format, args...);
}

template <typename... Args>
void NOTIFY_WarningMsg(const std::string_view topic, const std::string& format,
                       const Args&... args) noexcept
{
	notify(NotificationLevel::Warning, topic, format, args...);
}

template <typename... Args>
void NOTIFY_ErrorMsg(const std::string_view topic, const std::string& format,
                     const Args&... args) noexcept
{
	notify(NotificationLevel::Error, topic, format, args...);
}

#endif // DOSBOX_NOTIFICATIONS_H
