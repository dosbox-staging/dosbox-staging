// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_MIGRATE_PARSED_FILE_H
#define DOSBOX_CONFIG_MIGRATE_PARSED_FILE_H

#include <string>
#include <vector>

#include "line_classifier.h"

namespace ConfigMigrate {

// A logical unit: zero or more contiguous comment lines that describe the
// next setting, followed by the setting line itself. Migration rules act on
// whole units — when a setting is moved to another section, its attached
// comments move with it.
struct AttachedUnit {
	std::vector<ClassifiedLine> comments = {};
	ClassifiedLine setting               = {};
};

// One entry inside a section: either a raw line that passes through
// unchanged (blank, comment alone, commented-out setting, DOS command,
// unknown) or an AttachedUnit (comments + setting).
struct SectionEntry {
	enum class Kind {
		Raw,
		Unit,
	};
	Kind kind               = Kind::Raw;
	ClassifiedLine raw_line = {};
	AttachedUnit unit       = {};
};

struct ParsedSection {
	// The section header line. Empty raw + empty eol indicate the implicit
	// pre-section "section" capturing any content before the first
	// `[name]` header.
	ClassifiedLine header             = {};
	std::string name                  = {};
	std::vector<SectionEntry> entries = {};
};

struct ParsedFile {
	std::vector<ParsedSection> sections = {};
};

} // namespace ConfigMigrate

#endif
