// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_MIGRATE_RULES_H
#define DOSBOX_CONFIG_MIGRATE_RULES_H

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ConfigMigrate {

enum class Action {
	Rename,   // same section; key (and/or value) rewritten
	Move,     // cross-section move (with optional key rename)
	Remove,   // key gone, no replacement
	ValueMap, // value mapping only; key stays
	Split,    // one key becomes two
	Custom,   // function-pointer dispatch (cycles, cputype 386, etc.)
};

struct EmittedSetting {
	std::string key;
	std::string value;
};

struct ApplyResult {
	// Primary emitted setting line(s). Exactly one for
	// Rename/ValueMap/Move/Custom; two for Split; empty for Remove.
	std::vector<EmittedSetting> emit;

	// Non-empty when the unit should be inserted into a different section.
	std::string target_section;

	// Optional WARNING comment text to prepend above the emitted line(s).
	std::string warning;

	bool removed_with_warning = false;
};

struct InputLine {
	std::string section;
	std::string key;
	std::string value;
};

using CustomFn = std::function<ApplyResult(const InputLine&)>;

struct Rule {
	std::string from_section;
	std::string from_key;
	Action action = Action::Rename;

	// For Rename / Move / Split (first emitted line)
	std::string to_section; // empty for same-section
	std::string to_key;     // empty means keep the original key

	// For Split: the second emitted key/value
	std::string split_key;
	std::string split_value;

	// For ValueMap: simple table; pair.first = old value, pair.second = new
	std::vector<std::pair<std::string, std::string>> value_map;

	// For Custom: dispatch function
	CustomFn custom;

	// Default WARNING comment text (may be empty).
	std::string warning;
};

// Look up the rule for (section, key); returns nullptr if no rule applies.
const Rule* FindRule(std::string_view section, std::string_view key);

// Apply the rule to a parsed input line. Pure function.
ApplyResult Apply(const Rule& rule, const InputLine& line);

} // namespace ConfigMigrate

#endif
