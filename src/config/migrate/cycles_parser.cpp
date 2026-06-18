// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cycles_parser.h"

#include <cctype>
#include <optional>
#include <string>
#include <vector>

#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

constexpr int CpuCyclesMin             = 50;
constexpr int CpuCyclesMax             = 2'000'000;
constexpr int CpuCyclesRealModeDefault = 3000;

std::vector<std::string> tokenise(const std::string& s)
{
	std::vector<std::string> tokens;
	auto i = s.find_first_not_of(" \t");
	while (i != std::string::npos) {
		const auto j = s.find_first_of(" \t", i);
		tokens.push_back(s.substr(i,
		                          j == std::string::npos ? std::string::npos
		                                                 : j - i));
		if (j == std::string::npos) {
			break;
		}
		i = s.find_first_not_of(" \t", j);
	}
	return tokens;
}

std::optional<int> parse_int(const std::string& s)
{
	if (s.empty()) {
		return std::nullopt;
	}
	for (const auto c : s) {
		if (!std::isdigit(static_cast<unsigned char>(c))) {
			return std::nullopt;
		}
	}
	try {
		return std::stoi(s);
	} catch (...) {
		return std::nullopt;
	}
}

struct ParsedModifiers {
	int perc_used   = 100;
	int cycle_limit = -1;
};

// Walk the trailing tokens starting at `start` and collect any `<N>%`
// percent cap or `limit <N>` modifier. Unknown tokens are silently
// ignored (matching legacy parser leniency).
ParsedModifiers consume_modifiers(const std::vector<std::string>& tokens,
                                  const size_t start)
{
	ParsedModifiers m = {};
	for (size_t k = start; k < tokens.size(); ++k) {
		const auto& t = tokens[k];
		if (!t.empty() && t.back() == '%') {
			if (const auto n = parse_int(t.substr(0, t.size() - 1)); n) {
				m.perc_used = *n;
			}
		} else if (t == "limit") {
			if (k + 1 < tokens.size()) {
				if (const auto n = parse_int(tokens[k + 1]); n) {
					m.cycle_limit = *n;
				}
				++k;
			}
		}
	}
	return m;
}

// Adapt a raw legacy N to a valid cpu_cycles value. Mirrors the legacy
// behaviour of treating N <= 0 as "use default", and the modern system's
// [50, 2_000_000] range constraint.
std::string adapt_n(const int n, bool& used_legacy_default, bool& clamped)
{
	if (n <= 0) {
		used_legacy_default = true;
		return std::to_string(CpuCyclesRealModeDefault);
	}
	if (n < CpuCyclesMin) {
		clamped = true;
		return std::to_string(CpuCyclesMin);
	}
	if (n > CpuCyclesMax) {
		clamped = true;
		return std::to_string(CpuCyclesMax);
	}
	return std::to_string(n);
}

std::string build_warning(const std::string& original_value,
                          const ParsedModifiers& mods,
                          const bool used_legacy_default, const bool clamped)
{
	std::string w;
	const bool has_percent = (mods.perc_used != 100);
	const bool has_limit   = (mods.cycle_limit != -1);

	if (has_percent || has_limit) {
		w = "'cycles = " + original_value + "' uses ";
		if (has_percent && has_limit) {
			w += "percent cap and limit modifiers";
		} else if (has_percent) {
			w += "percent cap modifier";
		} else {
			w += "limit modifier";
		}
		w += " that are not supported by the new cpu_cycles* settings; "
		     "the modifier was dropped during migration. Tune "
		     "cpu_cycles manually if needed.";
	}

	if (used_legacy_default) {
		if (!w.empty()) {
			w += " ";
		}
		w += "Value was treated as default by the legacy parser; "
		     "migrated to cpu_cycles = " +
		     std::to_string(CpuCyclesRealModeDefault) + ".";
	}

	if (clamped) {
		if (!w.empty()) {
			w += " ";
		}
		w += "Value was outside the new valid range (" +
		     std::to_string(CpuCyclesMin) + ".." +
		     std::to_string(CpuCyclesMax) + ") and was clamped.";
	}
	return w;
}

} // namespace

ApplyResult MigrateLegacyCycles(const InputLine& line)
{
	ApplyResult r = {};

	const auto tokens = tokenise(line.value);
	if (tokens.empty()) {
		// Whitespace-only / empty: drop the line.
		return r;
	}

	const auto& type            = tokens[0];
	ParsedModifiers mods        = {};
	bool used_legacy_default    = false;
	bool clamped                = false;
	std::string cpu_cycles      = {};
	std::string cpu_cycles_prot = {};

	if (type == "max") {
		mods            = consume_modifiers(tokens, 1);
		cpu_cycles      = "max";
		cpu_cycles_prot = "auto";

	} else if (type == "auto") {
		// Optional explicit real-mode N before the modifiers.
		size_t mod_start = 1;
		std::optional<int> explicit_n;
		if (tokens.size() > 1) {
			explicit_n = parse_int(tokens[1]);
			if (explicit_n) {
				mod_start = 2;
			}
		}
		mods = consume_modifiers(tokens, mod_start);

		cpu_cycles      = explicit_n
		                        ? adapt_n(*explicit_n, used_legacy_default, clamped)
		                        : std::to_string(CpuCyclesRealModeDefault);
		cpu_cycles_prot = "max";

	} else if (type == "fixed") {
		std::optional<int> n;
		if (tokens.size() > 1) {
			n = parse_int(tokens[1]);
		}
		const auto v    = n ? adapt_n(*n, used_legacy_default, clamped)
		                    : std::to_string(CpuCyclesRealModeDefault);
		cpu_cycles      = v;
		cpu_cycles_prot = v;

	} else if (const auto n = parse_int(type); n) {
		const auto v    = adapt_n(*n, used_legacy_default, clamped);
		cpu_cycles      = v;
		cpu_cycles_prot = v;

	} else {
		// Unrecognised input — preserve the original line with a WARNING.
		r.emit.push_back({line.key, line.value});
		r.warning = "'cycles = " + line.value +
		            "' is not recognised. Preserved as-is for the "
		            "legacy parser to handle (falls back to " +
		            std::to_string(CpuCyclesRealModeDefault) + " cycles).";
		return r;
	}

	r.emit.push_back({"cpu_cycles", cpu_cycles});
	r.emit.push_back({"cpu_cycles_protected", cpu_cycles_prot});
	r.warning = build_warning(line.value, mods, used_legacy_default, clamped);
	return r;
}

} // namespace ConfigMigrate
