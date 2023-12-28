/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_LOGGING_H
#define DOSBOX_LOGGING_H

#include <cstdio>
#include <string>

#include "compiler.h"

#include "../src/libs/loguru/loguru.hpp"

enum LOG_TYPES {
	LOG_ALL,
	LOG_VGA, LOG_VGAGFX,LOG_VGAMISC,LOG_INT10,
	LOG_SB,LOG_DMACONTROL,
	LOG_FPU,LOG_CPU,LOG_PAGING,
	LOG_FCB,LOG_FILES,LOG_IOCTL,LOG_EXEC,LOG_DOSMISC,
	LOG_PIT,LOG_KEYBOARD,LOG_PIC,
	LOG_MOUSE,LOG_BIOS,LOG_GUI,LOG_MISC,
	LOG_IO,
	LOG_PCI,
	LOG_REELMAGIC,
	LOG_MAX
};

enum LOG_SEVERITIES {
	LOG_NORMAL,
	LOG_WARN,
	LOG_ERROR
};

// A class that implements a logger that can handle log messages generated during
// the execution of dosbox. Uses the loguru library to do the actual logging.
class Logger {
public:
	// Adds logger to the list of loggers that will be used to log messages.
	// The id can be used to remove the logger.
	static void AddLogger(const char* id, std::unique_ptr<Logger> logger,
	                      LOG_SEVERITIES severity = LOG_NORMAL);

	// Removes the logger that was added with the given ID.
	static void RemoveLogger(const char* id);

	virtual ~Logger() = default;

	virtual void Log(const char* log_group_name,
	                 const loguru::Message& message) = 0;
	virtual void Flush() {}
};

// A class used to help with the LOG() macro. Use that macro instead of using
// this class directly.
class LogHelper {
	LOG_TYPES       d_type;
	LOG_SEVERITIES  d_severity;
	const char* file;
	int line;

public:
	LogHelper(LOG_TYPES type, LOG_SEVERITIES severity, const char* file, int line)
	        : d_type(type),
	          d_severity(severity),
	          file(file),
	          line(line)
	{}

	void operator()(const char* buf, ...)
		GCC_ATTRIBUTE(__format__(__printf__, 2, 3)); //../src/debug/debug_gui.cpp
};

void LOG_StartUp();

#define LOG(type, severity) LogHelper(type, severity, __FILE__, __LINE__)

// Keep for compatibility
#define LOG_MSG(...)	LOG_F(INFO, __VA_ARGS__)

#define LOG_INFO(...)		LOG_F(INFO, __VA_ARGS__)
#define LOG_WARNING(...)	LOG_F(WARNING, __VA_ARGS__)
#define LOG_ERR(...)		LOG_F(ERROR, __VA_ARGS__)

#ifdef NDEBUG
// LOG_DEBUG exists only for messages useful during development, and not to
// be redirected into internal DOSBox debugger for DOS programs (C_DEBUG feature).
#define LOG_DEBUG(...)
#define LOG_TRACE(...)
#else

template <typename... Args>
void LOG_DEBUG(const std::string& format, const Args&... args) noexcept
{
	const auto format_green = std::string(loguru::terminal_green()) +
	                          loguru::terminal_bold() + format +
	                          loguru::terminal_reset();

	DLOG_F(INFO, format_green.c_str(), args...);
}

template <typename... Args>
void LOG_TRACE(const std::string& format, const Args&... args) noexcept
{
	const auto format_purple = std::string(loguru::terminal_purple()) +
	                           loguru::terminal_bold() + format +
	                           loguru::terminal_reset();

	DLOG_F(INFO, format_purple.c_str(), args...);
}


#endif // NDEBUG

#endif
