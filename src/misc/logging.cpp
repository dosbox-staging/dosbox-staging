// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/logging.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "config/config.h"
#include "config/setup.h"
#include "dosbox.h"
#include "misc/cross.h"
#include "misc/std_filesystem.h"
#include "utils/fs_utils.h"

#include "loguru/loguru.hpp"

namespace {

// Each run writes to a log named after the time it started, for example
// 'dosbox-2026-09-10-14-23-32.log'. The names sort by age, so the newest log
// is the last one in a directory listing.
constexpr std::string_view LogNamePrefix    = "dosbox-";
constexpr std::string_view LogNameExtension = ".log";

constexpr auto LogTimestampFormat = "%Y-%m-%d-%H-%M-%S";

// How many logs of previous runs are kept next to the log of the current run.
constexpr size_t MaxPreviousLogs = 5;

constexpr auto LogFileSinkId = "log-file";

// The lines logged before `init_logging()` has decided where they go.
std::vector<std::string> held_log_lines = {};

std::ofstream log_file = {};

LogMode parse_log_mode(const std::string& log_mode)
{
	if (log_mode == "console") {
		return LogMode::Console;
	}
	if (log_mode == "file") {
		return LogMode::File;
	}
	if (log_mode == "off") {
		return LogMode::Off;
	}
	return LogMode::ConsoleAndFile;
}

bool writes_to_file(const LogMode mode)
{
	return (mode == LogMode::File || mode == LogMode::ConsoleAndFile);
}

const char* verbosity_to_name(const loguru::Verbosity verbosity)
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

std::string make_log_file_name()
{
	const auto now     = std::time(nullptr);
	char timestamp[32] = {};

	std::strftime(timestamp,
	              sizeof(timestamp),
	              LogTimestampFormat,
	              std::localtime(&now));

	return std::string(LogNamePrefix) + timestamp + std::string(LogNameExtension);
}

// True only for names that `make_log_file_name()` could have produced, so that
// pruning never touches files the user put in the log directory.
bool is_run_log_name(const std::string& name)
{
	static const auto pattern = std::regex(
	        R"(dosbox-\d{4}-\d{2}-\d{2}-\d{2}-\d{2}-\d{2}\.log)");

	return std::regex_match(name, pattern);
}

// Returns the logs of all runs in the directory, newest first.
std::vector<std_fs::path> find_run_logs(const std_fs::path& dir)
{
	auto logs = std::vector<std_fs::path>{};
	auto ec   = std::error_code{};

	for (auto it = std_fs::directory_iterator(dir, ec);
	     !ec && it != std_fs::directory_iterator{};
	     it.increment(ec)) {

		if (is_run_log_name(it->path().filename().string())) {
			logs.push_back(it->path());
		}
	}

	// The names include the time of the run, so sorting by name sorts by age.
	std::sort(logs.begin(), logs.end(), std::greater<>{});
	return logs;
}

void remove_log(const std_fs::path& path)
{
	auto ec = std::error_code{};
	std_fs::remove(path, ec);

	if (ec && ec != std::errc::no_such_file_or_directory) {
		LOG_WARNING("LOG: Could not remove old log file '%s': %s",
		            path.string().c_str(),
		            ec.message().c_str());
	}
}

void prune_old_logs(const std_fs::path& dir)
{
	const auto logs = find_run_logs(dir);

	// The log of the current run is already in the directory and is the
	// newest, so keep it plus MaxPreviousLogs older ones.
	for (auto i = MaxPreviousLogs + 1; i < logs.size(); ++i) {
		remove_log(logs[i]);
	}
}

// Some log messages are coloured for the console with terminal escape
// sequences, which are of no use in a log file attached to a bug report.
std::string strip_terminal_colours(const std::string& text)
{
	// Matches a terminal control sequence, which has the form:
	//
	//   \x1b\[        the escape character, then '['
	//   [0-?]*        parameters, such as the '32' in '\x1b[32m'
	//   [ -/]*        intermediate characters (rarely used)
	//   [@-~]         the final character, such as the 'm' in '\x1b[32m'
	//
	// For example, '\x1b[32m', '\x1b[1m' and '\x1b[0m' each match.
	static const auto control_sequence = std::regex(R"(\x1b\[[0-?]*[ -/]*[@-~])");

	return std::regex_replace(text, control_sequence, "");
}

// Every log message passes through here, and is written to the log file as soon
// as one is open. The message can contain terminal colour codes meant for the
// console, so they are stripped. Loguru calls this with its own mutex held, so
// there is no need for locking here.
void write_log_line_callback(void*, const loguru::Message& message)
{
	const auto line = strip_terminal_colours(std::string(message.preamble) +
	                                         message.indentation +
	                                         message.prefix + message.message);

	if (log_file.is_open()) {
		log_file << line << '\n';
		log_file.flush();
	} else {
		held_log_lines.emplace_back(line);
	}
}

// Loguru writes a column header at the top of the files it opens itself; this
// sink writes the file, so it takes care of it. Loguru's other preamble lines
// are logged as regular messages, and are held back like all the others.
//
// This only covers the date, time, level and pipe columns. If another preamble
// column is enabled in init_loguru(), add it here too.
void write_log_file_header()
{
	if (!loguru::g_preamble_header) {
		return;
	}

	if (loguru::g_preamble_date) {
		log_file << "date       ";
	}
	if (loguru::g_preamble_time) {
		log_file << "time         ";
	}
	if (loguru::g_preamble_verbose) {
		log_file << "level";
	}
	if (loguru::g_preamble_pipe) {
		log_file << "| ";
	}

	log_file << '\n';
}

void write_held_log_lines()
{
	for (const auto& line : held_log_lines) {
		log_file << line << '\n';
	}

	log_file.flush();
	held_log_lines.clear();
}

bool start_log_file(const std_fs::path& path)
{
	log_file.open(path, std::ios::out | std::ios::trunc);

	if (!log_file.is_open()) {
		return false;
	}

	write_log_file_header();
	write_held_log_lines();

	return true;
}

void stop_holding_log_lines()
{
	loguru::remove_callback(LogFileSinkId);
	held_log_lines.clear();
	held_log_lines.shrink_to_fit();
}

void fall_back_to_console(const char* problem, const std_fs::path& path)
{
	stop_holding_log_lines();

	LOG_WARNING("LOG: %s '%s', keeping the log on the console",
	            problem,
	            path.string().c_str());
}

SectionProp* get_dosbox_section()
{
	const auto section = static_cast<SectionProp*>(control->GetSection("dosbox"));
	assert(section);
	return section;
}

std_fs::path get_log_dir()
{
	const auto log_dir_pref = get_dosbox_section()->GetString("log_dir");

	return log_dir_pref.empty() ? get_config_dir() / DefaultLogsDir
	                            : std_fs::path(log_dir_pref);
}

LogMode get_log_mode()
{
	return parse_log_mode(get_dosbox_section()->GetString("log_mode"));
}

} // namespace

void init_loguru(int argc, char* argv[], const bool exit_after_init)
{
	loguru::g_preamble_date    = true;
	loguru::g_preamble_time    = true;
	loguru::g_preamble_uptime  = false;
	loguru::g_preamble_thread  = false;
	loguru::g_preamble_file    = false;
	loguru::g_preamble_verbose = true;
	loguru::g_preamble_pipe    = true;

	loguru::set_verbosity_to_name_callback(verbosity_to_name);

	if (exit_after_init) {
		loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
	}

	// From here on the log lines are collected, because init_logging() only
	// knows whether there is a log file to write them to once the
	// configuration has been read.
	loguru::add_callback(LogFileSinkId,
	                     write_log_line_callback,
	                     nullptr,
	                     loguru::Verbosity_MAX);

	// Don't use loguru's crash handler for SIGTERM, just let SDL handle it
	// to perform a clean shutdown.
	loguru::Options options        = {};
	options.signal_options.sigterm = false;

	loguru::init(argc, argv, options);
}

void init_logging()
{
	const auto log_mode = get_log_mode();

	if (log_mode == LogMode::Off) {
		loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
	}
	if (!writes_to_file(log_mode)) {
		stop_holding_log_lines();
		return;
	}

	const auto log_dir = get_log_dir();

	if (!create_dir_if_not_exist(log_dir)) {
		fall_back_to_console("Can't create directory", log_dir);
		return;
	}

	const auto log_file_path = log_dir / make_log_file_name();

	if (!start_log_file(log_file_path)) {
		fall_back_to_console("Could not open", log_file_path);
		return;
	}

	prune_old_logs(log_dir);

	LOG_MSG("LOG: Writing logs to '%s'", log_file_path.string().c_str());

	if (log_mode == LogMode::File) {
		// Only the message logged just before an abort is left on the
		// console, at FATAL level.
		loguru::g_stderr_verbosity = loguru::Verbosity_FATAL;
	}
}
