// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/logger.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <system_error>

#include "config/config.h"
#include "config/setup.h"
#include "dosbox.h"
#include "misc/cross.h"
#include "misc/std_filesystem.h"
#include "misc/support.h"
#include "utils/fs_utils.h"

namespace {

// Loguru's atexit handler (registered in `Logger::SetUpLoguru()`) logs during
// program exit. This object is constructed before that handler is registered,
// so it is destroyed after the handler runs.
//
Logger logger;

} // namespace

// ---------------------------------------------------------------------------
// Loguru callbacks
// ---------------------------------------------------------------------------

// Loguru takes plain function pointers for these two, so they have to
// be static. Everything else below is a normal member function.
//
// `user_data` is the `Logger` that was registered in `SetUpLoguru()`.
//
void Logger::SinkCallback(void* user_data, const loguru::Message& message)
{
	static_cast<Logger*>(user_data)->LogSink(message);
}

const char* Logger::VerbosityToName(const loguru::Verbosity verbosity)
{
	if (verbosity <= loguru::Verbosity_FATAL) {
		return "FATAL";
	}
	if (verbosity == loguru::Verbosity_ERROR) {
		return "ERROR";
	}
	if (verbosity == loguru::Verbosity_WARNING) {
		return "WARN ";
	}
	if (verbosity == loguru::Verbosity_INFO) {
		return "INFO ";
	}
	if (verbosity == loguru::Verbosity_1) {
		return "DEBUG";
	}
	return "TRACE";
}

// ---------------------------------------------------------------------------
// Log sink
// ---------------------------------------------------------------------------

// Every log message passes through here. Loguru calls this with its own mutex
// held, so calls never overlap, but the lock is still needed against
// `Init()`, which runs outside loguru's lock.
void Logger::LogSink(const loguru::Message& message)
{
	std::lock_guard lock(mutex);

	// Keep one callback to avoid a gap when switching logging phases.
	switch (phase) {
	case Phase::PreInit: LogSinkPreInit(message); break;
	case Phase::PostInit: LogSinkPostInit(message); break;
	default: assertm(false, "Invalid Logger::Phase");
	}
}

// Called with the mutex held.
void Logger::LogSinkPreInit(const loguru::Message& message)
{
	const Line line = {
	        message.verbosity,
	        std::string(message.preamble) + message.indentation,
	        std::string(message.prefix) + message.message,
	};
	held_lines.emplace_back(line);
}

// Called with the mutex held.
void Logger::LogSinkPostInit(const loguru::Message& message)
{
	// `Init()` removes the sink when not logging to a file, but a message
	// can still arrive between switching phases and removing it.
	if (!log_file.is_open()) {
		return;
	}

	const auto line = std::string(message.preamble) + message.indentation +
	                  message.prefix + message.message;

	log_file << StripTerminalColours(line) << '\n';
	log_file.flush();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

Logger::LogMode Logger::ParseLogMode(const std::string& log_mode_pref) const
{
	if (log_mode_pref == "off") {
		return LogMode::Off;
	}
	if (log_mode_pref == "console") {
		return LogMode::Console;
	}
	if (log_mode_pref == "file") {
		return LogMode::File;
	}
	return LogMode::ConsoleAndFile;
}

void Logger::SetLogMode(const LogMode mode)
{
	std::lock_guard lock(mutex);
	log_mode = mode;
}

bool Logger::LogsToFile() const
{
	return (log_mode == LogMode::File || log_mode == LogMode::ConsoleAndFile);
}

// Verbosity of the console output that follows from `log_mode`
loguru::Verbosity Logger::ConsoleVerbosity() const
{
	switch (log_mode) {
	case LogMode::Off: return loguru::Verbosity_OFF;

	case LogMode::Console:
	case LogMode::ConsoleAndFile: return loguru::Verbosity_MAX;

	case LogMode::File: return loguru::Verbosity_FATAL;
	default:
		assertm(false, "Invalid LogMode");
		return loguru::Verbosity_OFF;
	}
}

std_fs::path Logger::GetLogDir() const
{
	const auto log_dir_pref = get_section("dosbox")->GetString("log_dir");

	return log_dir_pref.empty() ? get_config_dir() / DefaultLogsDir
	                            : std_fs::path(log_dir_pref);
}

// ---------------------------------------------------------------------------
// Log files
// ---------------------------------------------------------------------------

std::string Logger::MakeLogFileName() const
{
	const auto now     = std::time(nullptr);
	char timestamp[32] = {};

	std::tm local_time = {};
	if (!cross::localtime_r(&now, &local_time)) {
		return {};
	}
	std::strftime(timestamp, sizeof(timestamp), LogTimestampFormat, &local_time);

	return std::string(LogNamePrefix) + timestamp + std::string(LogNameExtension);
}

// Returns all log files matching the log name pattern in the directory
// of the current log file, oldest first. Only call this after the log
// file was opened.
std::vector<std_fs::path> Logger::FindLogFiles() const
{
	std::vector<std_fs::path> logs = {};

	std::error_code err = {};

	for (const auto& entry :
	     std_fs::directory_iterator(log_path.parent_path(), err)) {
		if (err) {
			// Problem iterating, skip entry
			break;
		}

		const auto path = entry.path();
		const auto is_log_name = std::regex_match(path.filename().string(),
		                                          log_filename_pattern);
		if (is_log_name) {
			logs.emplace_back(path);
		}
	}

	// The names include the timestamp of the run, so sorting them in
	// ascending order sorts them by age.
	std::ranges::sort(logs);
	return logs;
}

void Logger::DeleteLog(const std_fs::path& path) const
{
	std::error_code err = {};
	std_fs::remove(path, err);

	if (err && err != std::errc::no_such_file_or_directory) {
		LOG_WARNING("LOG: Could not remove old log file '%s': %s",
		            path.string().c_str(),
		            err.message().c_str());
	}
}

// Deletes the oldest logs in the directory of the current log file
void Logger::PruneOldLogs() const
{
	const auto logs = FindLogFiles();

	// The log of the current run is already in the directory and is the
	// newest
	const auto num_to_delete = logs.size() > MaxLogFiles
	                                 ? logs.size() - MaxLogFiles
	                                 : 0;

	for (size_t i = 0; i < num_to_delete; ++i) {
		DeleteLog(logs[i]);
	}
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

std::string Logger::StripTerminalColours(const std::string& text) const
{
	return std::regex_replace(text, control_sequence, "");
}

// Replays a held line to the console using the same colouring that loguru
// uses for its own live stderr lines (see log_message() in loguru.cpp).
void Logger::PrintToConsole(const Line& line) const
{
	const auto v = line.verbosity;

	if (loguru::g_colorlogtostderr && loguru::terminal_has_color()) {
		if (v > loguru::Verbosity_WARNING) {
			fprintf(stderr,
			        "%s\x1b[2m%s%s%s%s\n",
			        loguru::terminal_reset(),
			        line.head.c_str(),
			        (v == loguru::Verbosity_INFO ? loguru::terminal_reset()
			                                     : ""),
			        line.body.c_str(),
			        loguru::terminal_reset());
		} else {
			fprintf(stderr,
			        "%s%s%s%s%s\n",
			        loguru::terminal_reset(),
			        (v == loguru::Verbosity_WARNING
			                 ? loguru::terminal_yellow()
			                 : loguru::terminal_red()),
			        line.head.c_str(),
			        line.body.c_str(),
			        loguru::terminal_reset());
		}
	} else {
		fprintf(stderr, "%s%s\n", line.head.c_str(), line.body.c_str());
	}
}

// Writes the held lines to the log file (if open) and to the console (if
// `log_mode` allows it), then clears the hold buffer and switches to the
// post-init phase.
//
// Call this with the mutex NOT held; it takes the lock itself.
//
void Logger::ReplayHeldLogLines()
{
	std::lock_guard lock(mutex);

	for (const auto& line : held_lines) {
		if (log_file.is_open()) {
			log_file << StripTerminalColours(line.head + line.body)
			         << '\n';
		}
		if (line.verbosity <= ConsoleVerbosity()) {
			PrintToConsole(line);
		}
	}

	if (log_file.is_open()) {
		log_file.flush();
	}
	fflush(stderr);

	held_lines.clear();
	phase = Phase::PostInit;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void Logger::SetUpLoguru(int argc, char* argv[])
{
	loguru::g_preamble_date    = true;
	loguru::g_preamble_time    = true;
	loguru::g_preamble_uptime  = false;
	loguru::g_preamble_thread  = false;
	loguru::g_preamble_file    = false;
	loguru::g_preamble_verbose = true;
	loguru::g_preamble_pipe    = true;

	loguru::set_verbosity_to_name_callback(VerbosityToName);

	// Nothing goes to the console or a log file until `Init()` is called --
	// that's when we can query the configured `log_mode` and `log_dir`
	// settings. Until then, all log messages are held in-memory.
	loguru::g_stderr_verbosity = loguru::Verbosity_OFF;

	loguru::add_callback(LogSinkId, SinkCallback, this, loguru::Verbosity_MAX);

	// Don't use loguru's crash handler for SIGTERM, just let SDL handle it
	// to perform a clean shutdown.
	loguru::Options options        = {};
	options.signal_options.sigterm = false;

	loguru::init(argc, argv, options);
}

void Logger::Init(const std::optional<std::string>& log_file_name)
{
	const auto log_mode = ParseLogMode(
	        get_section("dosbox")->GetString("log_mode"));

	SetLogMode(log_mode);

	std::string error = {};

	if (LogsToFile()) {
		const auto log_dir = GetLogDir();

		if (!create_dir_if_not_exist(log_dir)) {
			error = "Can't create directory '" + log_dir.string() + "'";

		} else {
			// If a name was passed in, append to that file.
			// Otherwise, start a new one.
			const auto path = log_dir /
			                  (log_file_name ? *log_file_name
			                                 : MakeLogFileName());

			const auto open_mode = log_file_name
			                             ? (std::ios::out | std::ios::app)
			                             : (std::ios::out |
			                                std::ios::trunc);

			// Lines are only held in memory until
			// `ReplayHeldLogLines()` runs, so the sink doesn't
			// touch `log_file` here.
			log_file.open(path, open_mode);

			if (log_file.is_open()) {
				log_path = path;
			} else {
				error = "Can't open '" + path.string() + "'";
			}
		}

		// Log to the console if we can't log to a file
		if (!error.empty()) {
			SetLogMode(LogMode::Console);
		}
	}

	loguru::g_stderr_verbosity = ConsoleVerbosity();

	ReplayHeldLogLines();

	// We don't need the sink if not logging to a file. This takes loguru's
	// mutex, so it runs without the log mutex held.
	if (!LogsToFile()) {
		loguru::remove_callback(LogSinkId);
	}

	// These log, so they run last, with no lock held
	if (!error.empty()) {
		LOG_WARNING("LOG: %s; using 'log_mode = console'", error.c_str());

		set_section_property_value("dosbox", "log_mode", "console");
	}

	if (LogsToFile()) {
		PruneOldLogs();
		LOG_MSG("LOG: Writing logs to '%s'", log_path.string().c_str());
	}

	QueueSettingsForRestart();
}

// A restart re-reads the config files, so a `log_mode` or `log_dir` edited
// in the meantime would take effect and split the run's log across
// directories. Queueing the values in use makes the restart pass them with
// `--set`, which overrides the config files.
void Logger::QueueSettingsForRestart() const
{
	const auto section = get_section("dosbox");

	section->GetProperty("log_mode")->SetQueuedValue(section->GetString("log_mode"));

	if (LogsToFile()) {
		section->GetProperty("log_dir")->SetQueuedValue(
		        log_path.parent_path().string());
	}
}

// Prints the held warnings and errors without opening a log file. For exits
// that happen before `Init()`; does nothing after it.
void Logger::FlushEarlyWarningsToConsole()
{
	std::lock_guard lock(mutex);

	if (phase == Phase::PostInit) {
		return;
	}

	for (const auto& line : held_lines) {
		if (line.verbosity <= loguru::Verbosity_WARNING) {
			PrintToConsole(line);
		}
	}
	fflush(stderr);

	held_lines.clear();
	loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
	phase                      = Phase::PostInit;
}

std::optional<std::string> Logger::GetLogFileName() const
{
	std::lock_guard lock(mutex);

	if (LogsToFile()) {
		return log_path.filename().string();
	}
	return {};
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

void LOG_SetUpLoguru(int argc, char* argv[])
{
	logger.SetUpLoguru(argc, argv);
}

void LOG_Init(const std::optional<std::string> log_file_name)
{
	logger.Init(log_file_name);
}

void LOG_FlushEarlyWarningsToConsole()
{
	logger.FlushEarlyWarningsToConsole();
}

std::optional<std::string> LOG_GetLogFileName()
{
	return logger.GetLogFileName();
}
