// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "migrate.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>

#include "file_walker.h"
#include "rewriter.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

std::string timestamp_now()
{
	const auto now      = std::chrono::system_clock::now();
	const auto now_time = std::chrono::system_clock::to_time_t(now);
	std::tm tm          = {};
#ifdef _WIN32
	localtime_s(&tm, &now_time);
#else
	tm = *std::localtime(&now_time);
#endif
	char buf[32] = {};
	std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
	return buf;
}

bool write_bytes_atomically(const std_fs::path& path, const std::string& bytes)
{
	auto tmp = path;
	tmp += ".tmp";

	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out) {
			return false;
		}
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		if (!out) {
			return false;
		}
	}

	std::error_code ec;
	std_fs::rename(tmp, path, ec);
	if (ec) {
		std_fs::remove(tmp, ec);
		return false;
	}
	return true;
}

bool make_backup(const std_fs::path& path, std_fs::path& backup_path_out)
{
	auto bak = path;
	bak += ".bak." + timestamp_now();

	std::error_code ec;
	if (std_fs::exists(bak, ec)) {
		fprintf(stderr,
		        "migrate: refusing to overwrite existing backup '%s'\n",
		        bak.string().c_str());
		return false;
	}
	std_fs::copy_file(path, bak, ec);
	if (ec) {
		fprintf(stderr,
		        "migrate: cannot create backup '%s': %s\n",
		        bak.string().c_str(),
		        ec.message().c_str());
		return false;
	}
	backup_path_out = bak;
	return true;
}

// Process one file. Returns true if processed successfully (whether or not
// any rules fired; whether or not the file was rewritten).
bool process_file(const std_fs::path& path, const bool dry_run)
{
	auto parsed = ParseFile(path);
	if (!parsed) {
		fprintf(stderr, "migrate: cannot read '%s'\n", path.string().c_str());
		return false;
	}

	const auto stats = ApplyMigrations(*parsed);

	printf("  %s: rules=%d warnings=%d removed=%d moved=%d\n",
	       path.string().c_str(),
	       stats.rules_applied,
	       stats.warnings_emitted,
	       stats.settings_removed,
	       stats.units_moved);

	for (const auto& w : stats.warnings) {
		printf("    WARNING: %s\n", w.c_str());
	}

	if (stats.rules_applied == 0) {
		return true;
	}

	const auto serialised = SerialiseFile(*parsed);

	if (dry_run) {
		return true;
	}

	std_fs::path backup_path;
	if (!make_backup(path, backup_path)) {
		return false;
	}
	if (!write_bytes_atomically(path, serialised)) {
		fprintf(stderr,
		        "migrate: failed to write '%s'\n",
		        path.string().c_str());
		return false;
	}
	printf("    backup: %s\n", backup_path.string().c_str());
	return true;
}

} // namespace

int Run(const std_fs::path& root, const Options& opts)
{
	const auto candidates = FindConfigFiles(root);
	if (candidates.empty()) {
		printf("migrate: no DOSBox configs found under '%s'\n",
		       root.string().c_str());
		return 0;
	}

	printf("migrate: found %zu candidate file(s)%s\n",
	       candidates.size(),
	       opts.dry_run ? " (dry-run)" : "");

	int error_count = 0;
	for (const auto& path : candidates) {
		if (!process_file(path, opts.dry_run)) {
			++error_count;
		}
	}

	if (opts.dry_run) {
		printf("migrate: dry-run complete; pass --migrate-write to "
		       "apply changes.\n");
	}
	return error_count == 0 ? 0 : 1;
}

} // namespace ConfigMigrate
