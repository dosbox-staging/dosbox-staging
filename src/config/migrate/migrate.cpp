// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "migrate.h"

#include <cstdio>
#include <fstream>
#include <string>

#include "file_walker.h"
#include "line_classifier.h"
#include "rules.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

struct FileReport {
	int settings_seen          = 0;
	int rules_matched          = 0;
	int warnings               = 0;
	int commented_out_settings = 0;
	int dos_command_lines      = 0;
	int unknown_lines          = 0;
};

FileReport process_file(const std_fs::path& path)
{
	FileReport report = {};

	std::ifstream in(path, std::ios::binary);
	if (!in) {
		fprintf(stderr, "migrate: cannot open '%s'\n", path.string().c_str());
		return report;
	}

	std::string current_section;
	bool in_autoexec = false;

	std::string line;
	while (std::getline(in, line)) {
		// Preserve the newline for the classifier so it can capture
		// the original EOL bytes.
		const auto classified = Classify(line + "\n", in_autoexec);

		switch (classified.kind) {
		case LineKind::SectionHeader:
			current_section = classified.section;
			in_autoexec     = (current_section == "autoexec");
			break;

		case LineKind::Setting: {
			++report.settings_seen;
			if (const auto* rule = FindRule(current_section,
			                                classified.key)) {
				++report.rules_matched;
				if (!rule->warning.empty()) {
					++report.warnings;
				}
			}
			break;
		}

		case LineKind::CommentedOutSetting:
			++report.commented_out_settings;
			break;

		case LineKind::DosCommand: ++report.dos_command_lines; break;

		case LineKind::Unknown: ++report.unknown_lines; break;

		case LineKind::Blank:
		case LineKind::Comment: break;
		}
	}
	return report;
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

	int total_settings = 0;
	int total_rules    = 0;
	int total_warnings = 0;
	for (const auto& path : candidates) {
		const auto report = process_file(path);
		printf("  %s: settings=%d rules=%d warnings=%d "
		       "commented-out=%d dos-cmd=%d unknown=%d\n",
		       path.string().c_str(),
		       report.settings_seen,
		       report.rules_matched,
		       report.warnings,
		       report.commented_out_settings,
		       report.dos_command_lines,
		       report.unknown_lines);
		total_settings += report.settings_seen;
		total_rules += report.rules_matched;
		total_warnings += report.warnings;
	}

	printf("migrate: %d setting(s) seen, %d rule(s) matched, %d warning(s)\n",
	       total_settings,
	       total_rules,
	       total_warnings);

	if (total_rules == 0) {
		printf("migrate: rule table is empty (see config-findings.md). "
		       "No rewrites performed.\n");
	} else if (opts.dry_run) {
		printf("migrate: dry-run; pass --migrate-write to apply changes.\n");
	} else {
		printf("migrate: TODO — actual rewrite not yet implemented.\n");
	}
	return 0;
}

} // namespace ConfigMigrate
