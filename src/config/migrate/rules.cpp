// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rules.h"

#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

// The rule table. See `config-findings.md` for the full catalogue of
// v0.80 → v0.81 → v0.82 → main migrations. Rules are added incrementally
// as concrete test configs come in.
const std::vector<Rule> rules = {
        // TODO: populate from config-findings.md
};

} // namespace

const Rule* FindRule(std::string_view section, std::string_view key)
{
	for (const auto& r : rules) {
		if (r.from_section == section && r.from_key == key) {
			return &r;
		}
	}
	return nullptr;
}

ApplyResult Apply(const Rule& rule, const InputLine& line)
{
	ApplyResult result = {};
	result.warning     = rule.warning;

	switch (rule.action) {
	case Action::Rename:
		result.emit.push_back({rule.to_key.empty() ? line.key : rule.to_key,
		                       line.value});
		break;

	case Action::Move:
		result.target_section = rule.to_section;
		result.emit.push_back({rule.to_key.empty() ? line.key : rule.to_key,
		                       line.value});
		break;

	case Action::Remove: result.removed_with_warning = true; break;

	case Action::ValueMap: {
		std::string new_value = line.value;
		for (const auto& [old_v, new_v] : rule.value_map) {
			if (old_v == line.value) {
				new_value = new_v;
				break;
			}
		}
		result.emit.push_back({line.key, new_value});
		break;
	}

	case Action::Split:
		result.emit.push_back({rule.to_key.empty() ? line.key : rule.to_key,
		                       line.value});
		result.emit.push_back({rule.split_key, rule.split_value});
		break;

	case Action::Custom:
		if (rule.custom) {
			return rule.custom(line);
		}
		break;
	}
	return result;
}

} // namespace ConfigMigrate
