// SPDX-FileCopyrightText:  2019-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_LOGGING_H
#define DOSBOX_LOGGING_H

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "misc/std_filesystem.h"

#include "misc/compiler.h"

#include "loguru/loguru.hpp"

enum LOG_TYPES {
	LOG_ALL,
	LOG_VGA,
	LOG_VGAGFX,
	LOG_VGAMISC,
	LOG_INT10,
	LOG_SB,
	LOG_DMACONTROL,
	LOG_FPU,
	LOG_CPU,
	LOG_PAGING,
	LOG_FCB,
	LOG_FILES,
	LOG_IOCTL,
	LOG_EXEC,
	LOG_DOSMISC,
	LOG_PIT,
	LOG_KEYBOARD,
	LOG_PIC,
	LOG_MOUSE,
	LOG_BIOS,
	LOG_GUI,
	LOG_MISC,
	LOG_IO,
	LOG_PCI,
	LOG_REELMAGIC,
	LOG_MAX
};

enum LOG_SEVERITIES { LOG_NORMAL, LOG_WARN, LOG_ERROR };

// Should be called as early as possible during startup. We'll start
// collecting log messages in-memory after this was called.
void LOG_SetUpLoguru(int argc, char* argv[]);

// Should be called when we're ready to query the `log_mode` and `log_dir`
// settings (after the configs and the CLI options were processed). Will flush
// the log messages stored in-memory until this point to the console and/or
// the log file, then keep logging normally to either or both targets (if
// logging is enabled).
void LOG_Init(const std::optional<std::string> log_name);

// For command line actions that print their results and exit, and for startup
// errors, i.e., exits that happen before `LOG_Init()`. Prints the warnings and
// errors logged so far to the console without creating a log file. Does nothing
// if `LOG_Init()` has already run.
void LOG_FlushEarlyWarningsToConsole();

// Returns the log file name without the full path if we're logging to a file
// (e.g. `dosbox-20261022-185345.log`)
std::optional<std::string> LOG_GetLogFileName();

#if C_DEBUGGER
class LOG {
	LOG_TYPES d_type;
	LOG_SEVERITIES d_severity;

public:
	LOG(LOG_TYPES type, LOG_SEVERITIES severity)
	        : d_type(type),
	          d_severity(severity)
	{}
	void operator()(const char* buf, ...)
	        GCC_ATTRIBUTE(__format__(__printf__, 2, 3)); //../src/debug/debug_gui.cpp
};

void DEBUG_ShowMsg(const char* format, ...)
        GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

#define LOG_MSG DEBUG_ShowMsg

#define LOG_INFO(...)    LOG(LOG_ALL, LOG_NORMAL)(__VA_ARGS__)
#define LOG_WARNING(...) LOG(LOG_ALL, LOG_WARN)(__VA_ARGS__)
#define LOG_ERR(...)     LOG(LOG_ALL, LOG_ERROR)(__VA_ARGS__)

#else // C_DEBUGGER

struct LOG {
	inline LOG(LOG_TYPES, LOG_SEVERITIES) {}
	inline void operator()(const char*, ...) const {}
}; // add missing operators to here

// try to avoid anything smaller than bit32...
void GFX_ShowMsg(const char* format, ...)
        GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

// Keep for compatibility
#define LOG_MSG(...) LOG_F(INFO, __VA_ARGS__)

#define LOG_INFO(...)    LOG_F(INFO, __VA_ARGS__)
#define LOG_WARNING(...) LOG_F(WARNING, __VA_ARGS__)
#define LOG_ERR(...)     LOG_F(ERROR, __VA_ARGS__)

#endif // C_DEBUGGER

#ifdef NDEBUG
// LOG_DEBUG exists only for messages useful during development, and not to
// be redirected into internal DOSBox debugger for DOS programs (C_DEBUGGER
// feature).
#define LOG_DEBUG(...)
#define LOG_TRACE(...)
#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

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

#pragma GCC diagnostic pop
#pragma clang diagnostic pop

#endif // NDEBUG

#endif
