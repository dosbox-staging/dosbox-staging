// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_LOGGER_H
#define DOSBOX_LOGGER_H

#include <cstddef>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "misc/std_filesystem.h"

#include "loguru/loguru.hpp"

class Logger {
public:
	Logger() = default;

	// Prevent moving & copying; the logger registers itself with loguru by
	// address, so it must not be copied or moved.
	Logger(const Logger&)            = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&)                 = delete;
	Logger& operator=(Logger&&)      = delete;

	void SetUpLoguru(int argc, char* argv[]);

	// If `log_file_name` has a value, we'll append the logs to that file in
	// `log_dir` instead of creating a new one (used to continue logging
	// into the existing log file when "rebooting" the emulator, which
	// restarts the process).
	void Init(const std::optional<std::string>& log_file_name);

	// Returns the log file name without the full path if we're logging to a
	// file (e.g. `dosbox-20261022-185345.log`)
	std::optional<std::string> GetLogFileName() const;

private:
	struct Line {
		loguru::Verbosity verbosity = {};

		// preamble + indentation
		std::string head = {};

		// prefix + message (may contain ANSI from the message text)
		std::string body = {};
	};

	enum class Phase {
		PreInit,
		PostInit,
	};

	enum class LogMode {
		Off,
		Console,
		File,
		ConsoleAndFile,
	};

	// Each run writes to a log named after the time it started, for
	// example 'dosbox-20260910-142332.log'. The names sort by age, so the
	// newest log is the last one in a directory listing.
	//
	static constexpr std::string_view LogNamePrefix    = "dosbox-";
	static constexpr std::string_view LogNameExtension = ".log";
	static constexpr auto LogTimestampFormat           = "%Y%m%d-%H%M%S";

	static constexpr size_t MaxLogFiles = 10;

	static constexpr auto LogSinkId = "log-sink";

	static void SinkCallback(void* user_data, const loguru::Message& message);
	static const char* VerbosityToName(const loguru::Verbosity verbosity);

	void LogSink(const loguru::Message& message);

	void LogSinkPreInit(const loguru::Message& message);
	void LogSinkPostInit(const loguru::Message& message);

	LogMode ParseLogMode(const std::string& log_mode_pref) const;
	void SetLogMode(const LogMode mode);

	bool LogsToFile() const;
	loguru::Verbosity ConsoleVerbosity() const;
	std_fs::path GetLogDir() const;
	std::string MakeLogFileName() const;

	std::vector<std_fs::path> FindLogFiles() const;

	void DeleteLog(const std_fs::path& path) const;
	void PruneOldLogs() const;

	std::string StripTerminalColours(const std::string& text) const;
	void PrintToConsole(const Line& line) const;
	void ReplayHeldLogLines();

	// Set once in `Init()` from the `log_mode` setting. If the log file
	// can't be opened, it falls back to `LogMode::Console`. Nothing is
	// logged to a file or the console before `Init()`, so it starts as
	// `Off`.
	LogMode log_mode = LogMode::Off;

	Phase phase = Phase::PreInit;

	// We store the logged lines in this in-memory buffer in `PreInit`
	// phase
	std::vector<Line> held_lines = {};

	// The current log file. `log_path` is only meaningful while
	// `log_file` is open.
	std::ofstream log_file = {};
	std_fs::path log_path  = {};

	// Matches a log file name, such as 'dosbox-20260910-142332.log'
	const std::regex log_filename_pattern = std::regex(
	        R"(dosbox-\d{4}\d{2}\d{2}-\d{2}\d{2}\d{2}\.log)");

	// Matches a terminal control sequence, which has the form:
	//
	//   \x1b\[        the escape character, then '['
	//   [0-?]*        parameters, such as the '32' in '\x1b[32m'
	//   [ -/]*        intermediate characters (rarely used)
	//   [@-~]         the final character, such as the 'm' in '\x1b[32m'
	//
	// For example, '\x1b[32m', '\x1b[1m' and '\x1b[0m' each match.
	//
	// This is a member (and not a function-local static) on purpose.
	// Loguru's atexit handler logs "atexit" during program exit, which
	// reaches `StripTerminalColours()`. A function-local static regex is
	// constructed after that handler was registered, so it is destroyed
	// before the handler runs. The global `Logger` is constructed before
	// the handler is registered, so its members outlive the handler.
	//
	const std::regex control_sequence = std::regex(R"(\x1b\[[0-?]*[ -/]*[@-~])");

	// Guards `log_mode`, `phase`, `held_lines`, `log_file` and `log_path`
	mutable std::mutex mutex = {};
};

#endif // DOSBOX_LOGGER_H
