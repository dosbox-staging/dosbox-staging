// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/logging.h"

#include <cassert>
#include <cstddef>
#include <fstream>
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

constexpr auto LogFileName = "dosbox.log";

// Where the log that is being dropped is parked while the rotation is in
// progress, so that dropping it can wait until every other step has succeeded.
constexpr auto DroppedLogName = "dosbox.log.dropping";

// The log of the current run is kept along with the logs of the previous five
// runs.
constexpr auto MaxRotatedLogFiles = 5;

constexpr auto LogFileSinkId = "log-file";

// The lines logged before init_logging() has decided where they go.
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

// 'number' is one based: 'dosbox-001.log' holds the log of the run before the
// current one.
std::string make_rotated_log_name(const int number)
{
	constexpr auto MinDigits = size_t{3};

	auto digits = std::to_string(number);
	if (digits.size() < MinDigits) {
		digits.insert(0, MinDigits - digits.size(), '0');
	}
	auto name = std::string(LogFileName);
	return name.insert(name.rfind('.'), "-" + digits);
}

std_fs::path get_rotated_log_path(const std_fs::path& dir, const int number)
{
	return dir / make_rotated_log_name(number);
}

// Renames the log of the current run to 'dosbox-001.log', shifts the logs
// already there along, and drops the oldest one. The log that is being dropped
// is moved to DroppedLogName instead of being deleted, so that every step is a
// rename that can be undone: a step that fails undoes the renames already made
// and leaves the directory holding the same logs it held before. The log that
// is being dropped is deleted at the end, once everything else has succeeded.
// Returns false when a step fails; on the first run there is nothing to move,
// which is not a failure.
bool rotate_logs(const std_fs::path& dir)
{
	struct MoveLog {
		std_fs::path from = {};
		std_fs::path to = {};
	};
	auto moves = std::vector<MoveLog>{};

	// Renames one log, remembering it so that it can be put back. A slot that
	// no run has reached yet has nothing in it, which is not an error.
	const auto move_log = [&](const std_fs::path& from,
	                          const std_fs::path& to) {
		auto ec = std::error_code{};
		std_fs::rename(from, to, ec);

		if (ec == std::errc::no_such_file_or_directory) {
			return true;
		}
		if (ec) {
			LOG_WARNING(
				"LOG: Could not rename '%s' to '%s' while rotating the logs: %s",
				from.string().c_str(),
				to.string().c_str(),
				ec.message().c_str());
			return false;
		}
		moves.push_back({from, to});
		return true;
	};

	// Puts the logs that were already moved back where they were. The whole
	// rotation is a sequence of renames, not a single atomic step, so this is
	// what keeps a rotation that cannot be completed from leaving the logs
	// part way through being shifted.
	const auto restore_logs = [&moves]() {
		for (auto move = moves.rbegin(); move != moves.rend(); ++move) {
			auto ec = std::error_code{};
			std_fs::rename(move->to, move->from, ec);

			if (ec) {
				LOG_WARNING("LOG: Could not move '%s' back to '%s': %s",
				            move->to.string().c_str(),
				            move->from.string().c_str(),
				            ec.message().c_str());
			}
		}
	};

	// The log that is being dropped waits here while the others are moved. One
	// left over from an interrupted rotation is a log that rotation had
	// already committed to dropping, so it goes now.
	const auto dropped = dir / DroppedLogName;
	auto ec = std::error_code{};
	std_fs::remove(dropped, ec);

	if (ec && ec != std::errc::no_such_file_or_directory) {
		LOG_WARNING("LOG: Could not remove '%s' while rotating the logs: %s",
		            dropped.string().c_str(),
		            ec.message().c_str());
		return false;
	}

	// Move the oldest log out of the way, which frees its slot. Nothing has
	// been moved before this, so there is nothing to put back if it fails.
	if (!move_log(get_rotated_log_path(dir, MaxRotatedLogFiles), dropped)) {
		return false;
	}

	// Shift the rest along, newest first, so that each rename lands in the
	// slot the one before it has just emptied.
	for (auto number = MaxRotatedLogFiles - 1; number >= 1; --number) {
		if (!move_log(get_rotated_log_path(dir, number),
		              get_rotated_log_path(dir, number + 1))) {
			restore_logs();
			return false;
		}
	}

	// Then the log of this run, which has nothing there on the first run.
	if (!move_log(dir / LogFileName, get_rotated_log_path(dir, 1))) {
		restore_logs();
		return false;
	}

	// Everything else is in place, so the log that is being dropped can go.
	ec = {};
	std_fs::remove(dropped, ec);

	if (ec && ec != std::errc::no_such_file_or_directory) {
		LOG_WARNING("LOG: Could not remove '%s' while rotating the logs: %s",
		            dropped.string().c_str(),
		            ec.message().c_str());
		restore_logs();
		return false;
	}

	return true;
}

// Some log messages are coloured for the console with terminal escape
// sequences, which are of no use in a log file attached to a bug report.
std::string strip_terminal_colours(const std::string_view text)
{
	std::string stripped = {};
	stripped.reserve(text.size());

	for (auto pos = size_t{0}; pos < text.size(); ++pos) {
		if (text[pos] == '\x1b' && pos + 1 < text.size() &&
		    text[pos + 1] == '[') {

			// Skip up to and including the last character of the
			// control sequence.
			pos += 2;
			while (pos < text.size() &&
			       (text[pos] < '@' || text[pos] > '~')) {
				++pos;
			}
			continue;
		}
		stripped += text[pos];
	}
	return stripped;
}

// Every log message passes through here, and is written to the log file as soon
// as one is open.
void write_log_line(void*, const loguru::Message& message)
{
	const auto line =
	        strip_terminal_colours(std::string(message.preamble) +
	                               message.indentation + message.prefix +
	                               message.message);

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
void write_log_file_header()
{
	log_file << "File verbosity level: " << loguru::Verbosity_MAX << '\n';

	if (loguru::g_preamble_header) {
		if (loguru::g_preamble_date) {
			log_file << "date       ";
		}
		if (loguru::g_preamble_time) {
			log_file << "time         ";
		}
		if (loguru::g_preamble_pipe) {
			log_file << "| ";
		}
		log_file << '\n';
	}
}

bool start_log_file(const std_fs::path& path)
{
	log_file.open(path, std::ios::out | std::ios::trunc);
	if (!log_file.is_open()) {
		return false;
	}

	write_log_file_header();

	for (const auto& line : held_log_lines) {
		log_file << line << '\n';
	}
	log_file.flush();
	held_log_lines.clear();

	return true;
}

void stop_holding_log_lines()
{
	loguru::remove_callback(LogFileSinkId);
	held_log_lines.clear();
	held_log_lines.shrink_to_fit();
}

} // namespace

void init_loguru(int argc, char* argv[], const bool exit_after_init)
{
	loguru::g_preamble_date    = true;
	loguru::g_preamble_time    = true;
	loguru::g_preamble_uptime  = false;
	loguru::g_preamble_thread  = false;
	loguru::g_preamble_file    = false;
	loguru::g_preamble_verbose = false;
	loguru::g_preamble_pipe    = true;

	if (exit_after_init) {
		loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
	}

	// From here on the log lines are collected, because init_logging() only
	// knows whether there is a log file to write them to once the
	// configuration has been read.
	loguru::add_callback(LogFileSinkId,
	                     write_log_line,
	                     nullptr,
	                     loguru::Verbosity_MAX);

	// Don't use loguru's crash handler for SIGTERM, just let SDL handle it
	// to perform a clean shutdown.
	loguru::Options options = {};
	options.signal_options.sigterm = false;
	loguru::init(argc, argv, options);
}

void init_logging()
{
	const auto section = static_cast<SectionProp*>(control->GetSection("dosbox"));
	assert(section);

	const auto mode = parse_log_mode(section->GetString("log_mode"));

	const auto configured_dir = section->GetString("log_dir");
	const auto log_dir = configured_dir.empty()
	                           ? get_config_dir() / DefaultLogsDir
	                           : std_fs::path(configured_dir);

	if (mode == LogMode::Off) {
		loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
		stop_holding_log_lines();
		return;
	}
	if (mode == LogMode::Console) {
		stop_holding_log_lines();
		return;
	}

	if (!create_dir_if_not_exist(log_dir)) {
		stop_holding_log_lines();
		LOG_WARNING("LOG: Can't create directory '%s', keeping the log on the console",
		            log_dir.string().c_str());
		return;
	}

	if (!rotate_logs(log_dir)) {
		stop_holding_log_lines();
		LOG_WARNING("LOG: Could not rotate '%s', keeping the log on the console",
		            (log_dir / LogFileName).string().c_str());
		return;
	}

	const auto log_file_path = log_dir / LogFileName;

	if (!start_log_file(log_file_path)) {
		stop_holding_log_lines();
		LOG_WARNING("LOG: Could not open '%s', keeping the log on the console",
		            log_file_path.string().c_str());
		return;
	}

	LOG_MSG("Logging to '%s', mode: 'w', verbosity: %d",
	        log_file_path.string().c_str(),
	        loguru::Verbosity_MAX);

	if (mode == LogMode::File) {
		// Only the message logged just before an abort is left on the
		// console, at FATAL level.
		loguru::g_stderr_verbosity = loguru::Verbosity_FATAL;
	}
}
