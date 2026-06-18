// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rewriter.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <ios>
#include <string>

#include "rules.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

// ---------------------------------------------------------------------------
// Line-construction helpers (canonical formatting for rewritten lines).
// Original (untouched) lines are emitted byte-for-byte via their preserved
// raw + eol fields.
// ---------------------------------------------------------------------------

ClassifiedLine make_setting_line(const std::string& key, const std::string& value,
                                 const std::string& trailing_comment,
                                 const std::string& eol)
{
	ClassifiedLine c   = {};
	c.kind             = LineKind::Setting;
	c.key              = key;
	c.value            = value;
	c.trailing_comment = trailing_comment;
	c.eol              = eol;
	c.raw              = key + " = " + value;
	if (!trailing_comment.empty()) {
		c.raw += "  " + trailing_comment;
	}
	return c;
}

ClassifiedLine make_warning_line(const std::string& text, const std::string& eol)
{
	ClassifiedLine c = {};
	c.kind           = LineKind::Comment;
	c.eol            = eol;
	c.raw            = "# WARNING: " + text;
	return c;
}

ClassifiedLine make_commented_setting(const std::string& key, const std::string& value,
                                      const std::string& eol)
{
	ClassifiedLine c = {};
	c.kind           = LineKind::CommentedOutSetting;
	c.key            = key;
	c.value          = value;
	c.eol            = eol;
	c.raw            = "# " + key + " = " + value;
	return c;
}

ClassifiedLine make_section_header_line(const std::string& name,
                                        const std::string& eol)
{
	ClassifiedLine c = {};
	c.kind           = LineKind::SectionHeader;
	c.section        = name;
	c.eol            = eol;
	c.raw            = "[" + name + "]";
	return c;
}

// ---------------------------------------------------------------------------
// Byte-level helpers
// ---------------------------------------------------------------------------

std::vector<std::string> split_lines_preserving_eol(const std::string& buf)
{
	std::vector<std::string> lines;
	size_t start = 0;
	while (start < buf.size()) {
		const auto nl = buf.find('\n', start);
		if (nl == std::string::npos) {
			lines.push_back(buf.substr(start));
			break;
		}
		lines.push_back(buf.substr(start, nl - start + 1));
		start = nl + 1;
	}
	return lines;
}

std::string detect_dominant_eol(const ParsedFile& file)
{
	for (const auto& section : file.sections) {
		if (!section.header.eol.empty()) {
			return section.header.eol;
		}
		for (const auto& entry : section.entries) {
			if (entry.kind == SectionEntry::Kind::Raw) {
				if (!entry.raw_line.eol.empty()) {
					return entry.raw_line.eol;
				}
			} else if (!entry.unit.setting.eol.empty()) {
				return entry.unit.setting.eol;
			}
		}
	}
	return "\n";
}

// ---------------------------------------------------------------------------
// Move-queue: units removed from their source section, awaiting insertion
// into the target section.
// ---------------------------------------------------------------------------

struct PendingMove {
	std::string target_section           = {};
	std::vector<ClassifiedLine> comments = {};
	std::string new_key                  = {};
	std::string new_value                = {};
	std::string warning                  = {};
	std::string eol                      = {};
};

// ---------------------------------------------------------------------------
// In-place transformation of a single unit. Returns the updated unit, plus
// any extra-emitted lines that should be inserted immediately after, plus
// the move-queue entry if the rule says to relocate.
// ---------------------------------------------------------------------------

void insert_moved_units(ParsedFile& file, std::vector<PendingMove>& moves);

bool is_blank_raw(const SectionEntry& e)
{
	return e.kind == SectionEntry::Kind::Raw &&
	       e.raw_line.kind == LineKind::Blank;
}

ParsedSection* find_or_create_section(ParsedFile& file, const std::string& name,
                                      const std::string& eol)
{
	for (auto& s : file.sections) {
		if (s.name == name) {
			return &s;
		}
	}

	ParsedSection new_section = {};
	new_section.name          = name;
	new_section.header        = make_section_header_line(name, eol);

	const auto autoexec_it = std::find_if(file.sections.begin(),
	                                      file.sections.end(),
	                                      [](const ParsedSection& s) {
		                                      return s.name == "autoexec";
	                                      });

	if (autoexec_it != file.sections.end()) {
		const auto pos = file.sections.insert(autoexec_it,
		                                      std::move(new_section));
		return &*pos;
	}
	file.sections.push_back(std::move(new_section));
	return &file.sections.back();
}

bool same_emit_as_input(const ApplyResult& result, const InputLine& in)
{
	return result.emit.size() == 1 && result.emit[0].key == in.key &&
	       result.emit[0].value == in.value && result.warning.empty() &&
	       !result.removed_with_warning && result.target_section.empty();
}

} // namespace

// ---------------------------------------------------------------------------
// ParseFile / ParseBuffer
// ---------------------------------------------------------------------------

ParsedFile ParseBuffer(const std::string& buffer)
{
	const auto raw_lines = split_lines_preserving_eol(buffer);

	ParsedFile pf           = {};
	ParsedSection current   = {};
	bool current_has_header = false;
	bool in_autoexec        = false;
	std::vector<ClassifiedLine> pending_comments;

	auto flush_pending = [&]() {
		for (auto& c : pending_comments) {
			SectionEntry e = {};
			e.kind         = SectionEntry::Kind::Raw;
			e.raw_line     = std::move(c);
			current.entries.push_back(std::move(e));
		}
		pending_comments.clear();
	};

	auto finalise_section = [&]() {
		flush_pending();
		if (current_has_header || !current.entries.empty()) {
			pf.sections.push_back(std::move(current));
		}
		current            = ParsedSection{};
		current_has_header = false;
	};

	for (const auto& raw : raw_lines) {
		const auto classified = Classify(raw, in_autoexec);

		switch (classified.kind) {
		case LineKind::SectionHeader:
			finalise_section();
			current.header     = classified;
			current.name       = classified.section;
			current_has_header = true;
			in_autoexec        = (classified.section == "autoexec");
			break;

		case LineKind::Blank: {
			flush_pending();
			SectionEntry e = {};
			e.kind         = SectionEntry::Kind::Raw;
			e.raw_line     = classified;
			current.entries.push_back(std::move(e));
			break;
		}

		case LineKind::Comment:
			pending_comments.push_back(classified);
			break;

		case LineKind::CommentedOutSetting: {
			flush_pending();
			SectionEntry e = {};
			e.kind         = SectionEntry::Kind::Raw;
			e.raw_line     = classified;
			current.entries.push_back(std::move(e));
			break;
		}

		case LineKind::Setting: {
			AttachedUnit unit = {};
			unit.comments     = std::move(pending_comments);
			pending_comments.clear();
			unit.setting   = classified;
			SectionEntry e = {};
			e.kind         = SectionEntry::Kind::Unit;
			e.unit         = std::move(unit);
			current.entries.push_back(std::move(e));
			break;
		}

		case LineKind::DosCommand:
		case LineKind::Unknown: {
			flush_pending();
			SectionEntry e = {};
			e.kind         = SectionEntry::Kind::Raw;
			e.raw_line     = classified;
			current.entries.push_back(std::move(e));
			break;
		}
		}
	}

	finalise_section();
	return pf;
}

std::optional<ParsedFile> ParseFile(const std_fs::path& path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return std::nullopt;
	}
	in.seekg(0, std::ios::end);
	const auto size = static_cast<size_t>(in.tellg());
	in.seekg(0, std::ios::beg);

	std::string buf(size, '\0');
	if (size > 0) {
		in.read(buf.data(), static_cast<std::streamsize>(size));
	}
	return ParseBuffer(buf);
}

// ---------------------------------------------------------------------------
// ApplyMigrations
// ---------------------------------------------------------------------------

RewriteStats ApplyMigrations(ParsedFile& file)
{
	RewriteStats stats = {};
	std::vector<PendingMove> moves;

	for (auto& section : file.sections) {
		size_t i = 0;
		while (i < section.entries.size()) {
			auto& entry = section.entries[i];
			if (entry.kind != SectionEntry::Kind::Unit) {
				++i;
				continue;
			}

			auto& unit = entry.unit;
			const auto* rule = FindRule(section.name, unit.setting.key);
			if (!rule) {
				++i;
				continue;
			}

			InputLine in_line = {};
			in_line.section   = section.name;
			in_line.key       = unit.setting.key;
			in_line.value     = unit.setting.value;

			const ApplyResult result = Apply(*rule, in_line);

			// Idempotency / no-op short-circuit: a rule that
			// matched but produced exactly the same key+value and
			// no warning is a pass-through. Leave the original line
			// untouched.
			if (same_emit_as_input(result, in_line)) {
				++i;
				continue;
			}

			++stats.rules_applied;
			if (!result.warning.empty()) {
				++stats.warnings_emitted;
				stats.warnings.push_back(result.warning);
			}

			const auto orig_eol   = unit.setting.eol;
			const auto orig_trail = unit.setting.trailing_comment;

			if (result.removed_with_warning) {
				const auto orig_key   = unit.setting.key;
				const auto orig_value = unit.setting.value;
				unit.setting = make_commented_setting(orig_key,
				                                      orig_value,
				                                      orig_eol);
				if (!result.warning.empty()) {
					unit.comments.push_back(
					        make_warning_line(result.warning,
					                          orig_eol));
				}
				++stats.settings_removed;
				++i;
				continue;
			}

			if (result.emit.empty()) {
				// Silent removal — drop the unit entirely.
				++stats.settings_removed;
				section.entries.erase(section.entries.begin() +
				                      static_cast<std::ptrdiff_t>(i));
				continue;
			}

			if (!result.target_section.empty() &&
			    result.target_section != section.name) {
				// Cross-section move: pull this unit out and
				// queue it for the target section. Comments
				// move with it.
				PendingMove m    = {};
				m.target_section = result.target_section;
				m.comments       = std::move(unit.comments);
				m.new_key        = result.emit[0].key;
				m.new_value      = result.emit[0].value;
				m.warning        = result.warning;
				m.eol            = orig_eol;
				moves.push_back(std::move(m));

				++stats.units_moved;
				section.entries.erase(section.entries.begin() +
				                      static_cast<std::ptrdiff_t>(i));

				// Consume one trailing blank line to avoid
				// leaving a double-blank where the unit used to
				// be.
				if (i < section.entries.size() &&
				    is_blank_raw(section.entries[i])) {
					section.entries.erase(
					        section.entries.begin() +
					        static_cast<std::ptrdiff_t>(i));
				}
				continue;
			}

			// In-place rewrite. The first emit replaces the setting
			// line; any additional emits (Split) become bare Unit
			// entries immediately after.
			unit.setting = make_setting_line(result.emit[0].key,
			                                 result.emit[0].value,
			                                 orig_trail,
			                                 orig_eol);
			if (!result.warning.empty()) {
				unit.comments.push_back(
				        make_warning_line(result.warning, orig_eol));
			}

			for (size_t k = 1; k < result.emit.size(); ++k) {
				SectionEntry extra = {};
				extra.kind         = SectionEntry::Kind::Unit;
				extra.unit.setting =
				        make_setting_line(result.emit[k].key,
				                          result.emit[k].value,
				                          "",
				                          orig_eol);
				section.entries.insert(
				        section.entries.begin() +
				                static_cast<std::ptrdiff_t>(i + k),
				        std::move(extra));
			}
			i += result.emit.size();
		}
	}

	insert_moved_units(file, moves);
	return stats;
}

namespace {

void insert_moved_units(ParsedFile& file, std::vector<PendingMove>& moves)
{
	if (moves.empty()) {
		return;
	}
	const auto default_eol = detect_dominant_eol(file);

	for (auto& move : moves) {
		const auto eol = move.eol.empty() ? default_eol : move.eol;
		auto* target = find_or_create_section(file, move.target_section, eol);

		AttachedUnit unit = {};
		unit.comments     = std::move(move.comments);
		if (!move.warning.empty()) {
			unit.comments.push_back(make_warning_line(move.warning, eol));
		}
		unit.setting = make_setting_line(move.new_key, move.new_value, "", eol);

		SectionEntry e = {};
		e.kind         = SectionEntry::Kind::Unit;
		e.unit         = std::move(unit);

		// Insert before any trailing blank lines so the section's
		// closing separator stays in place.
		auto pos = target->entries.size();
		while (pos > 0 && is_blank_raw(target->entries[pos - 1])) {
			--pos;
		}
		target->entries.insert(target->entries.begin() +
		                               static_cast<std::ptrdiff_t>(pos),
		                       std::move(e));
	}
}

} // namespace

// ---------------------------------------------------------------------------
// SerialiseFile
// ---------------------------------------------------------------------------

std::string SerialiseFile(const ParsedFile& file)
{
	std::string out;
	for (const auto& section : file.sections) {
		// Emit header (skipped for the implicit pre-section section).
		if (!section.header.raw.empty() || !section.header.eol.empty()) {
			out += section.header.raw;
			out += section.header.eol;
		}
		for (const auto& entry : section.entries) {
			if (entry.kind == SectionEntry::Kind::Raw) {
				out += entry.raw_line.raw;
				out += entry.raw_line.eol;
			} else {
				for (const auto& c : entry.unit.comments) {
					out += c.raw;
					out += c.eol;
				}
				out += entry.unit.setting.raw;
				out += entry.unit.setting.eol;
			}
		}
	}
	return out;
}

} // namespace ConfigMigrate
