// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_MIGRATE_REWRITER_H
#define DOSBOX_CONFIG_MIGRATE_REWRITER_H

#include <optional>
#include <string>
#include <vector>

#include "misc/std_filesystem.h"
#include "parsed_file.h"

namespace ConfigMigrate {

// Parse a config file from disk into a structured representation.
// Returns nullopt on read error.
std::optional<ParsedFile> ParseFile(const std_fs::path& path);

// Parse a config file from a byte buffer. Used by tests and by ParseFile.
ParsedFile ParseBuffer(const std::string& buffer);

struct RewriteStats {
	int rules_applied    = 0;
	int warnings_emitted = 0;
	int settings_removed = 0;
	int units_moved      = 0;

	// Diagnostic messages produced during the rewrite (the WARNING text
	// from each rule that fired with a warning). Useful for the dry-run
	// summary and for inline PR-style preview.
	std::vector<std::string> warnings = {};
};

// Apply the migration rules in-place. Order:
//   1. Walk all sections, rewrite or remove units, queue moves.
//   2. Insert moved units into their target sections, creating target
//      sections before `[autoexec]` (or at EOF) when they don't yet exist.
RewriteStats ApplyMigrations(ParsedFile& file);

// Serialise the (possibly modified) file back into a byte stream.
// Untouched lines are emitted byte-for-byte using their preserved raw text
// and original line endings.
std::string SerialiseFile(const ParsedFile& file);

} // namespace ConfigMigrate

#endif
