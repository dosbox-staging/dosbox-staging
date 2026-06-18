// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_MIGRATE_LINE_CLASSIFIER_H
#define DOSBOX_CONFIG_MIGRATE_LINE_CLASSIFIER_H

#include <string>
#include <string_view>

namespace ConfigMigrate {

enum class LineKind {
	Blank,
	SectionHeader,       // [name]
	Comment,             // # free text  (or ;)
	CommentedOutSetting, // # key = value  (or ;)
	Setting,             // key = value
	DosCommand, // anything in [autoexec] that isn't blank/comment/header
	Unknown,
};

struct ClassifiedLine {
	LineKind kind = LineKind::Unknown;
	std::string raw; // original line, line ending stripped
	std::string eol; // original line ending ("\n", "\r\n", "\r", or empty)

	// For SectionHeader
	std::string section;

	// For Setting / CommentedOutSetting
	std::string key;
	std::string value;

	// For Setting: any inline trailing comment (including the `#` or `;`)
	std::string trailing_comment;
};

// Classify a single line. `in_autoexec` tells the classifier to treat
// non-INI, non-comment, non-blank lines as DOS commands rather than Unknown.
ClassifiedLine Classify(std::string_view line_with_eol, bool in_autoexec);

} // namespace ConfigMigrate

#endif
